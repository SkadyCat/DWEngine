#include "OpusData.h"
#include "OpusMapItems.h"
#include "Internationalization/Regex.h"
#include "DWRuleShop.h"
#include "DWRuleMinions.h"
#include "Misc/Paths.h"
#include "DWRulePresetReader.h"
#include "OpusTraitText.h"
using namespace DWRules;
namespace {
FString Trait(const FDWRuleLibrary& L,const FDWTraitEntry& E)
{
 const auto* D=L.Traits.Find(E.Id);return OpusTraitText::Format(D?D->DisplayName:E.Id.ToString(),E,L);
}
FString Lines(const FDWRuleLibrary& L,const FDWCollection& D){FString S=D.Description;for(const auto& E:D.Bases)S+=TEXT("\n")+Trait(L,E);for(const auto& E:D.Traits)S+=TEXT("\n")+Trait(L,E);return S;}
FOpusEntry Entry(const FDWRuleLibrary& L,const FDWRuleProfile& P,FName Kind,const FString& Id,const FDWCollection& D,int32 N){FOpusEntry E;E.Id=Id;E.Key=Kind.ToString()+TEXT(":")+Id;E.Kind=Kind.ToString();E.Name=D.Name;E.Description=Lines(L,D);E.Slot=D.Slot.IsNone()?TEXT(""):D.Slot.ToString();E.Category=D.Category.ToString();E.Rarity=D.Rarity.ToString();E.Count=N;E.CraftOp=D.CraftOp.ToString();E.CraftCount=D.CraftCount;E.CurrencyGrade=D.Category==TEXT("currency")&&D.CraftOp==TEXT("reroll")?D.CraftGrade:0;E.Equipped=Kind==TEXT("equipment")?P.Loadout.Equipment.FindRef(D.Slot)==FName(*Id):P.Belt.Contains(FName(*Id));E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons")/E.Kind/(Id+TEXT(".png"));return E;}
void AffixIds(const FDWEquipmentInstance& Item,FOpusEntry& E){
 TFunction<void(const FDWTraitEntry&)> Add=[&](const FDWTraitEntry& T){E.AffixIds.AddUnique(T.Id.ToString());for(const auto& C:T.With)Add(C);};
 auto A=[&](const FDWEquipmentAffix& V){E.AffixIds.AddUnique(V.Id.ToString());for(const auto& T:V.Traits)Add(T);};
 for(const auto& V:Item.Bases)A(V);for(const auto& V:Item.Affixes)A(V);if(Item.Legendary)A(*Item.Legendary);for(const auto& V:Item.AdditionalLegendary)A(V);
}
TArray<FOpusEntry> Floor(UDWRuleWorldSubsystem* R,bool Stock){TArray<FOpusEntry> Out;auto* F=R?R->PlayerFloorRewards():nullptr;if(!F)return Out;for(const auto& L:Stock?F->Stock:F->Ground){if(L.bClaimed)continue;FOpusEntry E;E.Id=L.Id.ToString();E.Key=L.Key;E.Name=L.Name;E.Kind=L.Kind.ToString();E.Category=L.Category.ToString();E.Description=L.Description;E.Rarity=L.Rarity.ToString();E.Count=L.Count;E.Price=L.Price;E.Tier=L.Tier;E.Identified=L.Kind!=TEXT("equipment")||L.Item.bIdentified;if(L.Kind==TEXT("equipment"))AffixIds(L.Item,E);E.Origin=L.Origin.ToString();E.Position=FVector(L.Position.X,L.Position.Y,0);E.SourcePosition=FVector(L.SourcePosition.X,L.SourcePosition.Y,0);
if(L.Kind==TEXT("equipment")){E.Slot=L.Item.Slot.ToString();E.Description=E.Identified?Lines(R->Library(),FDWEquipmentRules::Document(L.Item,&R->Library())):TEXT("尚未鉴定");}
else if(const auto* D=R->Library().Items.Find(L.Id)){E.CurrencyGrade=D->Category==TEXT("currency")&&D->CraftOp==TEXT("reroll")?D->CraftGrade:0;E.Description=Lines(R->Library(),*D);}
E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons")/E.Kind/((L.Kind==TEXT("equipment")?L.Item.BaseId.ToString():E.Id)+TEXT(".png"));Out.Add(E);}return Out;}
}
TArray<FOpusEntry> UOpusData::Inventory(UDWRuleWorldSubsystem* R){TArray<FOpusEntry> Out;if(!R)return Out;const auto& P=R->PlayerInventory();const auto& L=R->Library();for(const auto& V:P.OwnedEquipment)if(V.Value>0)if(const auto* D=L.Equipment.Find(V.Key))Out.Add(Entry(L,P,TEXT("equipment"),V.Key.ToString(),*D,V.Value));for(const auto& I:P.Loadout.Instances){auto D=FDWEquipmentRules::Document(I,&L);auto E=Entry(L,P,TEXT("equipment"),I.Uid,D,1);E.Identified=I.bIdentified;E.Locked=R->IsPlayerItemLocked(I.Uid);E.Synthesized=I.bSynthesized;E.FixedAffixCount=(I.Legendary?1:0)+I.AdditionalLegendary.Num();E.Tier=I.Tier;E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/equipment")/(I.BaseId.ToString()+TEXT(".png"));if(!I.bIdentified)E.Description=TEXT("未鉴定装备");Out.Add(E);}for(const auto& V:P.Loadout.Owned)if(V.Value>0){if(const auto* D=L.Items.Find(V.Key))Out.Add(Entry(L,P,TEXT("item"),V.Key.ToString(),*D,V.Value));else if(const auto* B=L.Builds.Find(V.Key)){FOpusEntry E;E.Id=V.Key.ToString();E.Key=TEXT("skill:")+E.Id;E.Kind=TEXT("skill");E.Name=B->DisplayName;E.Description=B->Description;E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/skill")/(E.Id+TEXT(".png"));E.Count=V.Value;E.Equipped=P.Loadout.Skills.Contains(V.Key);Out.Add(E);}}Out.Append(UOpusMapItems::Inventory(R));Out.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});return Out;}
TArray<FOpusEntry> UOpusData::Skills(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusEntry> Out;if(!R)return Out;
 for(const auto& S:R->PlayerInventory().Loadout.AvailableSkills(R->Library()))
 {
  FOpusEntry E;E.Id=S.Id.ToString();E.Key=E.Id;E.Name=S.Name;E.Kind=TEXT("skill");E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/skill")/(E.Id+TEXT(".png"));E.Slot=FString::FromInt(R->PlayerInventory().Loadout.Skills.Find(S.Id));E.Equipped=R->PlayerInventory().Loadout.Skills.Contains(S.Id);
  if(const auto* B=R->Library().Builds.Find(S.Id))
  {
   E.Description=B->Description;
   if(E.Description.IsEmpty())if(const auto* A=R->Library().Traits.Find(B->ActiveId))E.Description=A->Description;
  }
  if(!E.Description.IsEmpty())E.Description+=TEXT("\n");E.Description+=S.bEquipmentGranted?TEXT("装备授予"):TEXT("已拥有");Out.Add(E);
 }
 return Out;
}
TArray<FOpusEntry> UOpusData::Listings(UDWRuleWorldSubsystem* R){TArray<FOpusEntry> Out;if(R)for(const auto& L:FDWShopRules::Listings(R->Library())){FOpusEntry E;E.Key=L.Key;E.Id=L.Id.ToString();E.Kind=L.Kind.ToString();E.Name=L.Name;E.Description=L.Description;E.Price=0;E.Rarity=L.Rarity.ToString();if(E.Kind==TEXT("item"))if(const auto* D=R->Library().Items.Find(L.Id)){E.Category=D->Category.ToString();E.Tier=D->Tier;}E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons")/E.Kind/((E.Category==TEXT("map")?TEXT("map"):E.Id)+TEXT(".png"));Out.Add(E);}return Out;}
TArray<FOpusEntry> UOpusData::Ground(UDWRuleWorldSubsystem* R){return Floor(R,false);}
TArray<FOpusEntry> UOpusData::Merchant(UDWRuleWorldSubsystem* R){return Floor(R,true);}
TArray<FOpusEntry> UOpusData::Presets(){static TArray<FOpusEntry> Cache;if(Cache.IsEmpty()){TArray<FDWRulePreset> Rows;FString Error;if(FDWRulePresetReader::Catalogue(Rows,Error))for(const auto& R:Rows){FOpusEntry E;E.Key=R.Id.ToString();E.Id=E.Key;E.Name=R.Name;E.Description=R.Description;Cache.Add(E);}}return Cache;}
TArray<FOpusEntry> UOpusData::EquipmentAffixes(UDWRuleWorldSubsystem* R,const FString& Id){TArray<FOpusEntry> Out;if(!R)return Out;for(const auto& I:R->PlayerInventory().Loadout.Instances)if(I.Uid==Id){int32 Index=0;for(const auto& A:I.Affixes){FOpusEntry E;E.Id=FString::FromInt(Index++);E.Name=A.Name;E.Tier=A.Tier;for(const auto& T:A.Traits)E.Description+=Trait(R->Library(),T)+TEXT("\n");Out.Add(E);}break;}return Out;}
TArray<FOpusEntry> UOpusData::AffixPool(UDWRuleWorldSubsystem* R){TArray<FOpusEntry> Out;if(R)for(const auto& A:FDWEquipmentRules::Pool(R->Library())){FOpusEntry E;E.Id=A.Id.ToString();E.Key=E.Id;E.Name=A.Name;Out.Add(E);}return Out;}
TArray<FOpusEntry> UOpusData::Character(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusEntry> Out;if(!R||!R->PlayerSession())return Out;auto& S=*R->PlayerSession();auto& C=S.Character;const auto P=R->GetPlayerPools();const auto O=C.Offense();
 auto Add=[&](int G,int I,const FString& V){FOpusEntry E;E.Id=FString::Printf(TEXT("battle_live_modal_page::battle-stat-%d-%d-val"),G,I);E.Description=V;Out.Add(E);};
 auto V=[](double N){return FString::Printf(TEXT("%.1f"),N).Replace(TEXT(".0"),TEXT(""));};
 const int Elements[]={int(EElement::Physical),int(EElement::Fire),int(EElement::Frost),int(EElement::Lightning),int(EElement::Poison),int(EElement::Void)};
 for(int I=0;I<6;I++)Add(0,I,V(O.Flat[Elements[I]]));
 Add(1,0,TEXT("—"));Add(2,0,V(C.Strength));Add(2,1,V(C.Dexterity));Add(2,2,V(C.Intelligence));Add(2,3,V(C.Constitution));
 const int Plan=S.Combat.Hero.Plan;const double CastSpeed=R->GetPlanStat(Plan,TEXT("cast.speed_percent"));
 Add(3,0,V(3*FMath::Max(.1,1+CastSpeed/100)*P.ActionScale)+TEXT(" 次/秒"));Add(3,1,V(6.2*P.MoveScale)+TEXT(" m/s"));
 Add(3,2,V(R->GetPlanStat(Plan,TEXT("owner.critical_chance")))+TEXT("%"));Add(3,3,V(R->GetPlanStat(Plan,TEXT("owner.critical_multiplier")))+TEXT("%"));Add(3,4,TEXT("—"));Add(3,5,V((1+CastSpeed/100)*P.ActionScale*100)+TEXT("%"));
 Add(4,0,V(O.Increased[int(EElement::Fire)])+TEXT("%"));Add(4,1,V(O.Increased[int(EElement::Frost)])+TEXT("%"));Add(4,2,TEXT("—"));Add(4,3,V(C.S(TEXT("minion.damage")))+TEXT("%"));
 const auto* Defense=S.Host.World.TargetDefenseStats(S.TargetId);for(int I=0;I<6;I++)Add(5,I,V(Defense?Defense->Get(ElementStatId(Elements[I],EElementStat::Resistance)):0)+TEXT("%"));
 Add(6,0,V(P.MaxLife));Add(6,1,V(P.MaxShield));Add(6,2,V(P.MaxMana));Add(6,3,TEXT("—"));Add(6,4,TEXT("—"));return Out;
}
TArray<FOpusEntry> UOpusData::CharacterEffects(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusEntry> Out;if(!R)return Out;const auto& L=R->Library();const auto& P=R->PlayerInventory();
 for(auto Id:P.Loadout.Skills)if(!Id.IsNone())if(const auto* B=L.Builds.Find(Id)){FOpusEntry E;E.Id=Id.ToString();E.Name=B->DisplayName;E.Kind=TEXT("技能");E.Description=B->Description;E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/skill")/(E.Id+TEXT(".png"));if(const auto* A=L.Traits.Find(B->ActiveId)){if(E.Description.IsEmpty())E.Description=A->Description;}Out.Add(E);}
 for(auto E:Inventory(R))if(E.Kind==TEXT("equipment")&&E.Equipped){E.Kind=TEXT("装备");Out.Add(E);}return Out;
}
TArray<FOpusEntry> UOpusData::Summons(UDWRuleWorldSubsystem* R){TArray<FOpusEntry> Out;if(R&&R->PlayerSession())for(auto* M:R->PlayerSession()->Combat.Minions->Alive()){FOpusEntry E;E.Id=FString::FromInt(M->Id);E.Name=M->Kind.ToString();E.Description=FString::Printf(TEXT("气血 %.0f / %.0f · 护盾 %.0f / %.0f"),M->Life,M->MaxLife,M->Shield,M->MaxShield);Out.Add(E);}return Out;}
TArray<FOpusEntry> UOpusData::MapEquipment(UDWRuleWorldSubsystem* R,FName Core){TArray<FOpusEntry> Out;if(R)if(const auto* D=R->Library().Equipment.Find(Core.IsNone()?FName(TEXT("map_core_equipment")):Core);D&&D->Slot==TEXT("map")){auto E=Entry(R->Library(),R->PlayerInventory(),TEXT("equipment"),D->Id.ToString(),*D,1);E.Tier=D->Tier;E.Equipped=true;Out.Add(E);}return Out;}
TArray<FOpusEntry> UOpusData::MapCores(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusEntry> Out;if(!R)return Out;for(const auto& Pair:R->Library().Equipment)if(Pair.Value.Slot==TEXT("map")){auto E=Entry(R->Library(),R->PlayerInventory(),TEXT("equipment"),Pair.Key.ToString(),Pair.Value,1);E.Tier=Pair.Value.Tier;Out.Add(E);}Out.Sort([](const auto& A,const auto& B){return A.Tier==B.Tier?A.Id<B.Id:A.Tier<B.Tier;});return Out;
}
FString UOpusData::SkillName(UDWRuleWorldSubsystem* R,int32 Slot){if(!R||!R->PlayerInventory().Loadout.Skills.IsValidIndex(Slot))return TEXT("");FName Id=R->PlayerInventory().Loadout.Skills[Slot];return Id.IsNone()?TEXT("空槽"):R->GetBuildDisplayName(Id);}
double UOpusData::Dps(UDWRuleWorldSubsystem* R){double Value=0;int32 Targets=0;if(R)R->PlayerHUDDps(0,Value,Targets);return Value;}

FString UOpusData::SkillIcon(UDWRuleWorldSubsystem* R,int32 Slot){if(!R||!R->PlayerInventory().Loadout.Skills.IsValidIndex(Slot))return FString();FName Id=R->PlayerInventory().Loadout.Skills[Slot];return Id.IsNone()?FString():FPaths::ProjectContentDir()/TEXT("OpusIcons/skill")/(Id.ToString()+TEXT(".png"));}
