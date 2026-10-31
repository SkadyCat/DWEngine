#include "OpusMapItems.h"
#include "OpusTraitText.h"
#include "DWRuleSession.h"
#include "DWRuleCharacter.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
namespace
{
using FObject=TSharedPtr<FJsonObject>;
TMap<TWeakObjectPtr<UDWRuleWorldSubsystem>,FOpusMapItemState> Views;
FObject Config(){FString Text;FObject O;FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Rules/MapItems/catalog.json")));FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O);return O;}
FString Quality(const FString& V){return V==TEXT("rare")?TEXT("稀有"):V==TEXT("magic")?TEXT("魔法"):TEXT("普通");}
FOpusEntry Entry(UDWRuleWorldSubsystem* Rules,const FOpusMapItem& I)
{
 FOpusEntry E;E.Id=I.Uid;E.Key=TEXT("item:")+I.Uid;E.Kind=TEXT("item");E.Category=TEXT("map");E.Rarity=I.Rarity;E.Tier=I.Tier;E.Count=1;
 E.Name=FString::Printf(TEXT("地图 T%d · %s"),I.Tier,*Quality(I.Rarity));E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/item/map.png");
 E.Description=FString::Printf(TEXT("使用后消耗 1 张，进入第 %d 层地图。\n%d 条地图词缀"),I.Tier,I.Affixes.Num());
 for(const auto& A:I.Affixes)if(const auto* T=Rules->Library().Traits.Find(FName(*A.Id))){DWRules::FDWTraitEntry V;V.Id=FName(*A.Id);V.Params.Add(TEXT("n"),A.Value);E.Description+=TEXT("\n")+OpusTraitText::Format(T->DisplayName,V,Rules->Library());E.AffixIds.Add(A.Id);}
 return E;
}
}
TArray<FOpusEntry> UOpusMapItems::Catalog(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusEntry> Out;if(!R)return Out;
 for(const auto& P:R->Library().Items)if(P.Value.Category==TEXT("map")){
  FOpusEntry E;E.Id=P.Key.ToString();E.Key=TEXT("item:")+E.Id;E.Kind=TEXT("item");E.Category=TEXT("map");E.Name=P.Value.Name;E.Rarity=P.Value.Rarity.ToString();E.Tier=P.Value.Tier;E.Description=P.Value.Description;E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/item/map.png");Out.Add(E);
 }
 Out.Sort([](const auto& A,const auto& B){return A.Tier==B.Tier?A.Id<B.Id:A.Tier<B.Tier;});return Out;
}
TArray<FOpusMapAffix> UOpusMapItems::AffixPool()
{
 TArray<FOpusMapAffix> Out;auto O=Config();const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
 if(O&&O->TryGetArrayField(TEXT("affixes"),Rows))for(const auto& Row:*Rows){auto V=Row->AsObject();if(!V)continue;FOpusMapAffix A;V->TryGetStringField(TEXT("id"),A.Id);V->TryGetNumberField(TEXT("min"),A.Min);V->TryGetNumberField(TEXT("max"),A.Max);Out.Add(A);}return Out;
}
TArray<FOpusMapQuality> UOpusMapItems::Qualities()
{
 TArray<FOpusMapQuality> Out;auto O=Config();const FObject* Q=nullptr;if(O&&O->TryGetObjectField(TEXT("rarities"),Q))for(const auto& Pair:(*Q)->Values){auto V=Pair.Value->AsObject();if(!V)continue;FOpusMapQuality R;R.Id=FString(Pair.Key.ToView());R.Min=V->GetIntegerField(TEXT("minAffixes"));R.Max=V->GetIntegerField(TEXT("maxAffixes"));Out.Add(R);}return Out;
}
bool UOpusMapItems::DecodeProfile(const FString& Profile,FOpusMapItemState& State,FString& Error)
{
 State=FOpusMapItemState();if(Profile.IsEmpty())return true;FObject O;const FObject* Data=nullptr;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Profile),O)||!O){Error=TEXT("地图物品存档不是有效 JSON");return false;}
 if(!O->HasField(TEXT("opusMapItems")))return true;
 if(!O->TryGetObjectField(TEXT("opusMapItems"),Data)||!FJsonObjectConverter::JsonObjectToUStruct((*Data).ToSharedRef(),&State,0,0)){Error=TEXT("地图物品存档结构无效");return false;}return true;
}
FString UOpusMapItems::EncodeProfile(const FString& Profile,const FOpusMapItemState& State)
{
 FObject O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Profile),O)||!O)return TEXT("");auto Data=MakeShared<FJsonObject>();if(!FJsonObjectConverter::UStructToJsonObject(FOpusMapItemState::StaticStruct(),&State,Data,0,0))return TEXT("");O->SetObjectField(TEXT("opusMapItems"),Data);FString Out;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Out));return Out;
}
void UOpusMapItems::Publish(UDWRuleWorldSubsystem* Rules,const FOpusMapItemState& State){if(Rules)Views.Add(Rules,State);}
TArray<FOpusEntry> UOpusMapItems::Inventory(UDWRuleWorldSubsystem* Rules){TArray<FOpusEntry> Out;if(Rules)if(const auto* S=Views.Find(Rules))for(const auto& I:S->Items)Out.Add(Entry(Rules,I));return Out;}
TArray<FOpusHoverLine> UOpusMapItems::Lines(UDWRuleWorldSubsystem* R,const FString& Id)
{
 TArray<FOpusHoverLine> Out;if(!R)return Out;if(const auto* S=Views.Find(R))for(const auto& I:S->Items)if(I.Uid==Id){
  FOpusHoverLine H;H.Text=FString::Printf(TEXT("使用消耗 1 张 · 进入 T%d（第 %d 层）"),I.Tier,I.Tier);H.Style=1;Out.Add(H);
  H.Text=FString::Printf(TEXT("%d 条地图专属词缀"),I.Affixes.Num());Out.Add(H);
  int32 Index=0;for(const auto& A:I.Affixes)if(const auto* T=R->Library().Traits.Find(FName(*A.Id))){DWRules::FDWTraitEntry V;V.Id=FName(*A.Id);V.Params.Add(TEXT("n"),A.Value);FOpusHoverLine L;L.Style=2;L.Index=Index++;L.Text=OpusTraitText::Format(T->DisplayName,V,R->Library());L.Description=T->Description;Out.Add(L);}return Out;
 }return Out;
}
bool UOpusMapItems::PurchaseTemplate(UDWRuleWorldSubsystem* R,const FString& Key,FString& Error)
{
 if(!R||!Key.StartsWith(TEXT("item:"))){Error=TEXT("地图物品商品无效");return false;}const FName Id(*Key.Mid(5));const auto* D=R->Library().Items.Find(Id);
 if(!D||D->Category!=TEXT("map")){Error=TEXT("不是地图物品");return false;}
 // Preserve the existing shop / GM acquisition policy. Convert its one
 // catalogue copy into the AS-owned rolled instance.
 if(!R->BuyPlayerListing(Key,Error))return false;
 auto& Profile=const_cast<DWRules::FDWRuleProfile&>(R->PlayerInventory());Profile.Consume(Id);
 if(auto* S=R->PlayerSession()){const int32 Count=Profile.Loadout.Owned.FindRef(Id);if(Count)S->Loadout.Owned.Add(Id,Count);else S->Loadout.Owned.Remove(Id);}return true;
}
bool UOpusMapItems::Evaluate(UDWRuleWorldSubsystem* R,FName Core,const FOpusMapItem& Item,FOpusMapData& Out,FString& Error)
{
 if(Item.Uid.IsEmpty())return UOpusNative::MapCoreData(R,Core,Out,Error);
 if(!R){Error=TEXT("missing rules");return false;}const auto* Base=R->Library().Equipment.Find(Core);if(!Base||Base->Slot!=TEXT("map")){Error=TEXT("unknown map core");return false;}
 DWRules::FDWCollectionPart P;P.Collection=*Base;P.bEquipment=true;P.Label=TEXT("map-core");DWRules::FDWCollectionPart M;M.Collection.Id=FName(*Item.Uid);M.Collection.Category=TEXT("map");M.Label=TEXT("map-item");
 for(const auto& A:Item.Affixes){DWRules::FDWTraitEntry T;T.Id=FName(*A.Id);T.Params.Add(TEXT("n"),A.Value);M.Collection.Traits.Add(T);}
 DWRules::FDWBuildSource Source;TArray<DWRules::FDWContribution> Rows;if(!R->Library().Aggregate(NAME_None,{P,M},TNumericLimits<double>::Max(),Source,Rows,Error))return false;
 TArray<DWRules::FDWHeldTrait> Held;for(const auto& Row:Rows)if(!Row.bMerged&&!Row.bExpired)Held.Add(Row.Held);DWRules::FDWRuleCharacter C;if(!C.SetSheet(Held,Error)&&!Error.IsEmpty())return false;
 Out=FOpusMapData();Out.Tier=Item.Tier;Out.LifeBase=C.S(TEXT("minion.life_base"));Out.LifeIncreased=C.S(TEXT("minion.life_increased"));Out.LifeMore=C.S(TEXT("minion.life_more"));Out.DamageIncreased=C.S(TEXT("minion.damage"));Out.DamageMore=C.S(TEXT("minion.more"));Out.Packs=C.S(TEXT("map.monster_packs"));return true;
}
