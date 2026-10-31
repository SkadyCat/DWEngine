#include "OpusDropService.h"
#include "DWRuleWorldSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "OpusMapEvents.h"

UOpusDropService::UOpusDropService(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=true;}
FString UOpusDropService::LocalVersion()
{
 FSHA1 Hash;const uint8 Zero=0;
 for(const TCHAR* Group:{TEXT("Equipment"),TEXT("Items")})
 {
  const FString Root=FPaths::ProjectContentDir()/TEXT("RulesTower")/Group;TArray<FString> Files;
  IFileManager::Get().FindFiles(Files,*(Root/TEXT("*.json")),true,false);Files.Sort();
  if(Files.IsEmpty())return FString();
  for(const FString& File:Files)
  {
   TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*(Root/File)))return FString();
   const FTCHARToUTF8 Name(*(FString(Group)/File));Hash.Update(reinterpret_cast<const uint8*>(Name.Get()),Name.Length());Hash.Update(&Zero,1);
   Hash.Update(Bytes.GetData(),Bytes.Num());Hash.Update(&Zero,1);
  }
 }
 Hash.Final();uint8 Bytes[20];Hash.GetHash(Bytes);return BytesToHex(Bytes,20).ToLower();
}
void UOpusDropService::Initialize(UDWRuleWorldSubsystem* WorldRules)
{
 Rules=WorldRules;Initialized=true;BaseUrl=TEXT("http://127.0.0.1:5188");
 FString Configured;if(GConfig->GetString(TEXT("Opus.TradeBackend"),TEXT("BaseUrl"),Configured,GGameIni)&&!Configured.TrimStartAndEnd().IsEmpty())BaseUrl=Configured.TrimStartAndEnd();
 FString Override;if(FParse::Value(FCommandLine::Get(),TEXT("DWODropService="),Override))BaseUrl=Override;
 while(BaseUrl.EndsWith(TEXT("/")))BaseUrl.LeftChopInline(1);
 CatalogVersion=LocalVersion();
 // Existing isolated tests explicitly run without a backend. The online test
 // flag removes that exemption and exercises the actual HTTP transport.
 Offline=FParse::Param(FCommandLine::Get(),TEXT("DWODropsOffline"))||
  (FParse::Param(FCommandLine::Get(),TEXT("DWOTransient"))&&!FParse::Param(FCommandLine::Get(),TEXT("DWODropServiceTest"))&&!FParse::Param(FCommandLine::Get(),TEXT("DWODropsOnline")));
}
void UOpusDropService::RequestPool(FName Core,int32 Floor,int32 Tier,const FString& MapItemId)
{
 ++Generation;if(Pending){Pending->OnProcessRequestComplete().Unbind();Pending->CancelRequest();Pending.Reset();}
 MapCore=Core;MapFloor=Floor;MaxTier=Tier;ActiveMapItem=MapItemId;Ready=false;Failed=false;Error.Reset();PoolVersion.Reset();Attempts=0;RetryAt=0;EquipmentCount=0;ItemCount=0;
 if(Offline){EventConfigJson=UOpusMapEvents::LocalConfig();EquipmentIds=UOpusMapEvents::EquipmentCandidates(Rules);Ready=true;UE_LOG(LogTemp,Display,TEXT("DROP_SERVICE_OFFLINE explicit=1 core=%s"),*Core.ToString());return;}
 if(CatalogVersion.IsEmpty()){Rejected(TEXT("无法读取本地道具目录版本"),false);return;}
 if(!BaseUrl.StartsWith(TEXT("https://"))&&!BaseUrl.StartsWith(TEXT("http://127.0.0.1:"))&&!BaseUrl.StartsWith(TEXT("http://localhost:")))
 {Rejected(TEXT("远程掉落服务必须使用 HTTPS"),false);return;}
 // BeginPlay may synchronously construct a large authored HUD. Start HTTP on
 // the first component tick so that time is not counted as transport latency.
 RetryAt=FPlatformTime::Seconds()+0.001;
}
void UOpusDropService::Retry(){if(!Ready&&!Pending.IsValid()){Attempts=0;RetryAt=0;Failed=false;Error.Reset();Send();}}
void UOpusDropService::Send()
{
 if(!Rules||!Initialized)return;
 const int32 Epoch=Generation;++Attempts;++Requests;
 Pending=FHttpModule::Get().CreateRequest();Pending->SetVerb(TEXT("GET"));Pending->SetTimeout(4.f);
 Pending->SetHeader(TEXT("Accept"),TEXT("application/json"));
 Pending->SetURL(BaseUrl+TEXT("/v1/maps/")+FGenericPlatformHttp::UrlEncode(MapCore.ToString())+TEXT("/drop-pool?floor=")+FString::FromInt(MapFloor)+TEXT("&catalogVersion=")+CatalogVersion+(ActiveMapItem.IsEmpty()?FString():TEXT("&mapItemId=")+FGenericPlatformHttp::UrlEncode(ActiveMapItem)));
 Pending->OnProcessRequestComplete().BindWeakLambda(this,[this,Epoch](FHttpRequestPtr,FHttpResponsePtr Response,bool Success)
 {
  if(Epoch!=Generation)return;Pending.Reset();
  if(!Success||!Response.IsValid()){Rejected(TEXT("掉落服务连接失败"),true);return;}
  const int32 Code=Response->GetResponseCode();
  if(Code!=200){Rejected(Code==409?TEXT("道具目录版本不一致，请更新后端规则目录"):FString::Printf(TEXT("掉落服务返回 HTTP %d"),Code),Code>=500||Code==429);return;}
  FString Reason;if(!Apply(Response->GetContentAsString(),Reason)){Rejected(Reason,false);return;}
  Ready=true;Failed=false;Error.Reset();RetryAt=0;
  UE_LOG(LogTemp,Display,TEXT("DROP_SERVICE_READY core=%s floor=%d tier=%d equipment=%d items=%d catalog=%s pool=%s requests=%d"),*MapCore.ToString(),MapFloor,MaxTier,EquipmentCount,ItemCount,*CatalogVersion,*PoolVersion,Requests);
 });
 if(!Pending->ProcessRequest()){Pending.Reset();Rejected(TEXT("无法发起掉落请求"),true);}
}
void UOpusDropService::Rejected(const FString& Reason,bool Transient)
{
 Error=Reason;Ready=false;
 if(Transient&&Attempts<3){RetryAt=FPlatformTime::Seconds()+Attempts;Failed=false;}
 else{Failed=true;RetryAt=0;}
 UE_LOG(LogTemp,Warning,TEXT("DROP_SERVICE_REJECTED core=%s attempt=%d retry=%d error=%s"),*MapCore.ToString(),Attempts,Failed?0:1,*Reason);
}
bool UOpusDropService::Apply(const FString& Json,FString& Reason)
{
 if(Json.Len()>256*1024){Reason=TEXT("掉落道具池响应过大");return false;}
 TSharedPtr<FJsonObject> Doc;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Doc)||!Doc.IsValid()){Reason=TEXT("掉落道具池 JSON 无效");return false;}
 int32 Schema=0,Tier=0;FString Version,Core,Pool;
 if(!Doc->TryGetNumberField(TEXT("schemaVersion"),Schema)||Schema!=1||!Doc->TryGetNumberField(TEXT("maxTier"),Tier)||Tier!=MaxTier||
  !Doc->TryGetStringField(TEXT("catalogVersion"),Version)||Version!=CatalogVersion||!Doc->TryGetStringField(TEXT("mapCoreId"),Core)||Core!=MapCore.ToString()||
  !Doc->TryGetStringField(TEXT("poolVersion"),Pool)||Pool.Len()!=64){Reason=TEXT("掉落道具池版本、地图或 T 阶不匹配");return false;}
 FString ResponseMapItem;Doc->TryGetStringField(TEXT("mapItemId"),ResponseMapItem);if(ResponseMapItem!=ActiveMapItem){Reason=TEXT("地图物品不匹配");return false;}
 TArray<FName> Equipment,Items;
 auto Read=[&](const TCHAR* Field,TArray<FName>& Out)
 {
  const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(!Doc->TryGetArrayField(Field,Values)||Values->Num()>4096)return false;
  for(const auto& Value:*Values){FString Id;if(!Value.IsValid()||!Value->TryGetString(Id)||Id.IsEmpty()||Id.Len()>128)return false;Out.Add(FName(*Id));}return true;
 };
 if(!Read(TEXT("equipmentIds"),Equipment)||!Read(TEXT("itemIds"),Items)){Reason=TEXT("掉落候选列表无效");return false;}
 const TSharedPtr<FJsonObject>* Events=nullptr;
 if(!Doc->TryGetObjectField(TEXT("events"),Events)){Reason=TEXT("掉落服务缺少奇遇配置，请更新后端");return false;}
 FString EventJson;FJsonSerializer::Serialize(Events->ToSharedRef(),TJsonWriterFactory<>::Create(&EventJson));
 FOpusMapEventConfig EventConfig;if(!UOpusMapEvents::ReadConfig(EventJson,EventConfig,Reason))return false;
 if(!Rules->SetPlayerFloorDropPool(Equipment,Items,Reason))return false;
 EquipmentIds=Equipment;EventConfigJson=EventJson;EquipmentCount=Equipment.Num();ItemCount=Items.Num();PoolVersion=Pool;return true;
}
void UOpusDropService::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
 Super::TickComponent(DeltaTime,TickType,TickFunction);
 if(RetryAt>0&&FPlatformTime::Seconds()>=RetryAt){RetryAt=0;Send();}
}
void UOpusDropService::EndPlay(const EEndPlayReason::Type Reason)
{
 ++Generation;RetryAt=0;if(Pending){Pending->OnProcessRequestComplete().Unbind();Pending->CancelRequest();Pending.Reset();}Super::EndPlay(Reason);
}
