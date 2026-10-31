#include "OpusMapEvents.h"
#include "DWRulePawnEquipment.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace DWRules;
namespace {
int32 Number(const TSharedPtr<FJsonObject>& D,const TCHAR* K,int32 Default){if(!D->HasField(K))return Default;double V;if(!D->TryGetNumberField(K,V)||!FMath::IsFinite(V)||V!=FMath::FloorToDouble(V)||V<0||V>1000000)return MIN_int32;return int32(V);}
FString String(const TSharedPtr<FJsonObject>& D,const TCHAR* K){FString V;D->TryGetStringField(K,V);return V;}
const FDWEquipmentInstance* Find(UDWRuleWorldSubsystem* R,const FString& Uid){return R?R->PlayerInventory().Loadout.Instances.FindByPredicate([&](const auto& I){return I.Uid==Uid;}):nullptr;}
FOpusEntry View(const FDWEquipmentInstance& I)
{
 FOpusEntry E;E.Id=I.Uid;E.Key=TEXT("equipment:")+I.Uid;E.Kind=TEXT("equipment");E.Name=I.Name;E.Slot=I.Slot.ToString();E.Rarity=I.Rarity.ToString();E.Tier=I.Tier;E.Count=1;E.Identified=I.bIdentified;E.Synthesized=I.bSynthesized;E.FixedAffixCount=(I.Legendary?1:0)+I.AdditionalLegendary.Num();
 E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/equipment")/(I.BaseId.ToString()+TEXT(".png"));return E;
}
const FDWEquipmentAffix* Line(const FDWEquipmentInstance& I,int32 Index,bool& Fixed)
{
 Fixed=false;if(Index<0)return nullptr;if(I.Affixes.IsValidIndex(Index))return &I.Affixes[Index];Index-=I.Affixes.Num();Fixed=true;
 if(I.Legendary){if(Index==0)return &I.Legendary.GetValue();Index--;}
 return I.AdditionalLegendary.IsValidIndex(Index)?&I.AdditionalLegendary[Index]:nullptr;
}
}
FString UOpusMapEvents::LocalConfig(){FString S;FFileHelper::LoadFileToString(S,*(FPaths::ProjectContentDir()/TEXT("MapEvents/events.json")));return S;}
bool UOpusMapEvents::ReadConfig(const FString& Json,FOpusMapEventConfig& Out,FString& Error)
{
 TSharedPtr<FJsonObject> D;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),D)||!D.IsValid()||Number(D,TEXT("schemaVersion"),0)!=1){Error=TEXT("奇遇配置版本无效");return false;}
 FOpusMapEventConfig C;C.ChancePercent=Number(D,TEXT("chancePercent"),75);C.MinEvents=Number(D,TEXT("minEvents"),1);C.MaxEvents=Number(D,TEXT("maxEvents"),2);
 const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
 if(C.ChancePercent<0||C.ChancePercent>100||C.MinEvents<0||C.MaxEvents<C.MinEvents||C.MaxEvents>8||(!D->TryGetArrayField(TEXT("definitions"),Values)||Values->Num()>8||C.MaxEvents>Values->Num())){Error=TEXT("奇遇配置范围无效");return false;}
 TSet<FString> Seen;for(const auto& V:*Values)
 {
  if(!V.IsValid()||V->Type!=EJson::Object){Error=TEXT("奇遇定义无效");return false;}auto O=V->AsObject();FOpusMapEventDefinition E;
  E.Id=String(O,TEXT("id"));E.Kind=String(O,TEXT("kind"));E.Name=String(O,TEXT("name"));E.Description=String(O,TEXT("description"));E.Weight=Number(O,TEXT("weight"),1);E.MinFloor=Number(O,TEXT("minFloor"),1);E.Guards=Number(O,TEXT("guards"),0);E.CandidateCount=Number(O,TEXT("candidateCount"),3);E.ChooseCount=Number(O,TEXT("chooseCount"),1);E.Rarity=FName(*String(O,TEXT("rarity")));
  if(E.Id.IsEmpty()||E.Name.IsEmpty()||Seen.Contains(E.Id)||E.Weight<1||E.MinFloor<1||E.MinFloor>100||E.Guards<0||E.Guards>30||E.CandidateCount<1||E.CandidateCount>6||E.ChooseCount<1||E.ChooseCount>E.CandidateCount||(E.Kind!=TEXT("altar")&&E.Kind!=TEXT("fusion")&&E.Kind!=TEXT("vault")&&E.Kind!=TEXT("choice"))){Error=TEXT("奇遇定义字段无效");return false;}
  const TArray<TSharedPtr<FJsonValue>>* Money=nullptr;if(O->TryGetArrayField(TEXT("currencies"),Money))for(const auto& M:*Money)E.Currencies.Add(FName(*M->AsString()));
  if((E.Kind==TEXT("choice")||E.Kind==TEXT("vault"))&&E.Rarity!=TEXT("rare")&&E.Rarity!=TEXT("legendary")){Error=TEXT("奇遇奖励品质无效");return false;}
  if(E.Kind==TEXT("altar")&&E.Currencies.IsEmpty()){Error=TEXT("法坛缺少指定通货");return false;}Seen.Add(E.Id);C.Definitions.Add(MoveTemp(E));
 }
 Out=MoveTemp(C);Error.Reset();return true;
}
TArray<FName> UOpusMapEvents::EquipmentCandidates(UDWRuleWorldSubsystem* R){TArray<FName> Out;if(R)for(const auto& P:R->Library().Equipment)if(!FDWPawnEquipmentRules::IsFixedSlot(P.Value.Slot))Out.Add(P.Key);return Out;}
int32 UOpusMapEvents::FusionLineCount(UDWRuleWorldSubsystem* R,const FString& Uid){const auto* I=Find(R,Uid);return I?I->Affixes.Num()+(I->Legendary?1:0)+I->AdditionalLegendary.Num():0;}
TArray<FOpusEntry> UOpusMapEvents::Equipment(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusEntry> Out;if(R)for(const auto& I:R->PlayerInventory().Loadout.Instances){auto E=View(I);E.Equipped=R->PlayerInventory().Loadout.Equipment.FindRef(I.Slot)==FName(*I.Uid);E.Locked=R->IsPlayerItemLocked(I.Uid);Out.Add(E);}return Out;
}
bool UOpusEventOffer::Generate(UDWRuleWorldSubsystem* R,const TArray<FName>& Allowed,int32 Seed,int32 Tier,FName Rarity,int32 Count,int32 Choose,FString& Error)
{
 if(OwnerRules||!R||Count<1||Count>6||Choose<1||Choose>Count||Tier<1||Tier>10||(Rarity!=TEXT("rare")&&Rarity!=TEXT("legendary"))){Error=TEXT("奖励生成参数无效");return false;}
 TArray<const FDWCollection*> Pool;for(FName Id:Allowed)if(const auto* D=R->Library().Equipment.Find(Id))if(!FDWPawnEquipmentRules::IsFixedSlot(D->Slot)&&(D->Rarity!=TEXT("legendary")||Rarity==TEXT("legendary"))&&(Rarity!=TEXT("legendary")||D->Legendary.IsSet()||D->Traits.Num()))Pool.AddUnique(D);
 if(Pool.IsEmpty()){Error=TEXT("本地图没有可用于该奇遇的装备");return false;}
 FDWEquipmentRandom Random{uint32(Seed)};TArray<FDWEquipmentInstance> Next;auto Available=Pool;
 for(int32 N=0;N<Count;N++)
 {
  if(Available.IsEmpty())Available=Pool;const int32 Index=FMath::Min(Available.Num()-1,int32(Random.Next()*Available.Num()));const auto* D=Available[Index];Available.RemoveAt(Index);
  FDWEquipmentInstance I;if(!FDWEquipmentRules::Roll(R->Library(),*D,Rarity,[&](){return Random.Next();},TEXT("event_")+FGuid::NewGuid().ToString(EGuidFormats::Digits),I,Error,Tier))return false;Next.Add(MoveTemp(I));
 }
 Items=MoveTemp(Next);OwnerRules=R;ChooseCount=Choose;Error.Reset();return true;
}
bool UOpusEventOffer::PrepareFusion(UDWRuleWorldSubsystem* R,const FString& Left,const FString& Right,const FString& Token,const TArray<int32>& LeftIndices,const TArray<int32>& RightIndices,FString& Error)
{
 const auto* L=Find(R,Left);const auto* B=Find(R,Right);
 if(OwnerRules||!L||!B||Left==Right||L->Slot!=B->Slot||!L->bIdentified||!B->bIdentified||!R->IsPlayerItemLocked(Left)||!R->IsPlayerItemLocked(Right)){Error=TEXT("请放入两件同部位、已鉴定的装备并锁定");return false;}
 auto Result=*L;Result.Uid=TEXT("fusion_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);Result.bSynthesized=true;Result.Affixes.Reset();Result.Legendary.Reset();Result.AdditionalLegendary.Reset();Result.ReforgeTargets.Reset();
 auto Take=[&](const FDWEquipmentInstance& Source,const TArray<int32>& Indices)
 {
  TSet<int32> Seen;for(int32 Index:Indices){bool Fixed=false;const auto* A=Line(Source,Index,Fixed);if(!A||Seen.Contains(Index)){Error=TEXT("合成抽取记录无效");return false;}Seen.Add(Index);if(Fixed){if(!Result.Legendary)Result.Legendary=*A;else Result.AdditionalLegendary.Add(*A);}else Result.Affixes.Add(*A);}return true;
 };
 if(!Take(*L,LeftIndices)||!Take(*B,RightIndices))return false;
 Result.Rarity=Result.Legendary?FName(TEXT("legendary")):Result.Affixes.Num()>=3?FName(TEXT("rare")):Result.Affixes.Num()?FName(TEXT("magic")):FName(TEXT("common"));
 Items.Add(MoveTemp(Result));Consumed={Left,Right};LockToken=Token;OwnerRules=R;ChooseCount=1;Error.Reset();return true;
}
TArray<FOpusEntry> UOpusEventOffer::Entries() const{TArray<FOpusEntry> Out;for(const auto& I:Items)Out.Add(View(I));return Out;}
TArray<FOpusHoverLine> UOpusEventOffer::Lines(int32 Index) const{return OwnerRules&&Items.IsValidIndex(Index)?UOpusData::InstanceLines(Items[Index],OwnerRules->Library()):TArray<FOpusHoverLine>();}
bool UOpusEventOffer::Claim(const TArray<int32>& Indices,FString& Error)
{
 if(Claimed||!OwnerRules||Indices.Num()!=ChooseCount){Error=TEXT("奖励已领取或选择数量不符");return false;}
 TSet<int32> Seen;TArray<FDWEquipmentInstance> Granted;for(int32 Index:Indices){if(!Items.IsValidIndex(Index)||Seen.Contains(Index)){Error=TEXT("奖励选择无效");return false;}Seen.Add(Index);Granted.Add(Items[Index]);}
 if(!OwnerRules->CommitPlayerEventItems(Consumed,Granted,LockToken,Error))return false;Claimed=true;return true;
}
