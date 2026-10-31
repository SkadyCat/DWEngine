#include "OpusData.h"
#include "OpusMapItems.h"
#include "DWRulePawnEquipment.h"
#include "Internationalization/Regex.h"
#include "OpusTraitText.h"
using namespace DWRules;
namespace
{
FString Format(const FString& Text,const FDWTraitEntry& Entry,const FDWRuleLibrary& Library)
{
 return OpusTraitText::Format(Text,Entry,Library);
}
FOpusHoverLine AffixLine(const FDWEquipmentAffix& A,const FDWRuleLibrary& Library,int32 Style,int32 Index=-1,int32 Grade=0)
{
 FOpusHoverLine Line;Line.Text=A.Name;Line.Style=Style;Line.Tier=A.Tier;Line.Index=Index;Line.Grade=Grade;
 for(const auto& T:A.Traits)if(const auto* D=Library.Traits.Find(T.Id))
 {
  const FString Name=Format(D->DisplayName,T,Library),Description=Format(D->Description,T,Library);
  if(A.Traits.Num()==1)Line.Text=Name;
  if(!Line.Description.IsEmpty())Line.Description+=TEXT("\n");Line.Description+=Description.IsEmpty()?Name:Description;
 }
 return Line;
}
void TraitIds(const FDWTraitEntry& Entry,TSet<FName>& Ids){Ids.Add(Entry.Id);for(const auto& C:Entry.With)TraitIds(C,Ids);}
}
TArray<FOpusHoverLine> UOpusData::InstanceLines(const FDWEquipmentInstance& I,const FDWRuleLibrary& Library)
{
 TArray<FOpusHoverLine> Out;
 auto Add=[&](const FString& Text,int32 Style=0){if(!Text.IsEmpty()){FOpusHoverLine L;L.Text=Text;L.Style=Style;Out.Add(L);}};
  if(!I.bIdentified){Add(TEXT("未鉴定装备"));Add(TEXT("鉴定后生成基础属性、词条和具体数值。"),1);return Out;}
  if(const auto* Template=Library.Equipment.Find(I.BaseId))Add(Template->Description,1);
  if(!I.Skill.IsNone())if(const auto* Skill=Library.Builds.Find(I.Skill))Add(TEXT("绑定技能：")+Skill->DisplayName);
  for(const auto& A:I.Bases)Out.Add(AffixLine(A,Library,3));
  if(I.bSynthesized)Add(TEXT("合成装备 · 继承词条保留来源数值"),1);
  if(I.Legendary)Out.Add(AffixLine(*I.Legendary,Library,4));
  for(const auto& A:I.AdditionalLegendary)Out.Add(AffixLine(A,Library,4));
  for(int32 Index=0;Index<I.Affixes.Num();Index++){const int32 Bound=I.ReforgeTargets.Find(Index);Out.Add(AffixLine(I.Affixes[Index],Library,2,Index,Bound==INDEX_NONE?0:Bound+1));}
  return Out;
}
TArray<FOpusHoverLine> UOpusData::HoverLines(UDWRuleWorldSubsystem* R,const FString& Id)
{
 TArray<FOpusHoverLine> Out;if(!R)return Out;
 Out=UOpusMapItems::Lines(R,Id);if(!Out.IsEmpty())return Out;
 for(const auto& I:R->PlayerInventory().Loadout.Instances)if(I.Uid==Id)return InstanceLines(I,R->Library());
 auto Add=[&](const FString& Text){if(!Text.IsEmpty()){FOpusHoverLine L;L.Text=Text;Out.Add(L);}};
 for(const auto& E:Inventory(R))if(E.Id==Id)
 {
  if(!E.Identified){Add(TEXT("未鉴定装备"));Add(TEXT("鉴定后生成基础属性、词条和具体数值。"));return Out;}
  TArray<FString> Lines;E.Description.ParseIntoArrayLines(Lines);for(const auto& Line:Lines)Add(Line);
  return Out;
 }
 return Out;
}
FOpusCraftState UOpusData::CraftState(UDWRuleWorldSubsystem* R,const FString& Id,FName Currency,FName Omen,bool VirtualCurrency)
{
 FOpusCraftState View;if(!R)return View;const auto& Profile=R->PlayerInventory();const auto& Library=R->Library();
 const auto* Item=Profile.Loadout.Instances.FindByPredicate([&](const auto& I){return I.Uid==Id;});
 View.Instance=Item&&Item->bIdentified;if(Item)View.BoundCount=Item->ReforgeTargets.Num();
 FName Family;if(!Omen.IsNone())if(const auto* D=Library.Items.Find(Omen))Family=FDWEquipmentRules::OmenFamily(Library,*D);
 const auto Pool=FDWEquipmentRules::Pool(Library);
 for(const auto& A:Pool)
 {
  bool Include=Family.IsNone();if(!Include){TSet<FName> Ids;for(const auto& T:A.Traits)TraitIds(T,Ids);for(FName Trait:Ids)if(Library.Tags.FamiliesOf(Trait).Contains(Family)){Include=true;break;}}
  if(Include)View.PoolCount++;
 }
 if(!Item||!Item->bIdentified){View.Blocked=TEXT("请先鉴定装备");return View;}
 if(!VirtualCurrency&&R->IsPlayerItemLocked(Id)){View.Blocked=TEXT("请在锁定这件装备的祭坛中制作");return View;}
 if(FDWPawnEquipmentRules::IsFixedSlot(Item->Slot)){View.Blocked=TEXT("固定装备槽不能加工词条");return View;}
 const auto* Money=Library.Items.Find(Currency);
 if(!Money||Money->Category!=TEXT("currency")||(!VirtualCurrency&&Profile.Loadout.Owned.FindRef(Currency)<=0)){View.Blocked=TEXT("请选择并激活通货");return View;}
 View.Operation=Money->CraftOp.ToString();View.Count=Money->CraftCount;
 if(Money->CraftOp==TEXT("remove_affixes"))
 {
  View.PoolCount=Item->Affixes.Num();
  if(!Omen.IsNone())View.Blocked=TEXT("移除词缀不能搭配预兆");
  else if(Item->Affixes.Num()<Money->CraftCount)View.Blocked=TEXT("没有足够的可移除随机词缀");
  return View;
 }
 if(!Omen.IsNone()&&(Family.IsNone()||Profile.Loadout.Owned.FindRef(Omen)<=0)){View.Blocked=TEXT("预兆不足");return View;}
 if(!View.PoolCount){View.Blocked=TEXT("所选预兆没有可用词条");return View;}
 if(Money->CraftOp==TEXT("add_affixes")||Money->CraftOp==TEXT("roll_affixes"))return View;
 if(Money->CraftOp!=TEXT("reroll")){View.Blocked=TEXT("通货不支持装备加工");return View;}
 if(Item->Affixes.IsEmpty()){View.Blocked=TEXT("没有随机词条，请先使用陨星石");return View;}
 // Read-only UI eligibility. The existing native transaction owns validation,
 // RNG, conflicts, grade binding, payment and every item mutation.
 const int32 Grade=Money->CraftGrade;
 for(int32 Index=0;Index<Item->Affixes.Num();Index++)
 {
  const int32 Bound=Item->ReforgeTargets.Find(Index);
  if(Grade>=1&&Grade<=3&&(Bound==INDEX_NONE?View.BoundCount<3&&Grade==View.BoundCount+1:Grade==Bound+1))View.EligibleAffixes.Add(Index);
 }
 if(View.EligibleAffixes.IsEmpty())View.Blocked=TEXT("灵石须按下、中、上品依次选定词条；已选定词条只能使用对应品级");
 return View;
}
