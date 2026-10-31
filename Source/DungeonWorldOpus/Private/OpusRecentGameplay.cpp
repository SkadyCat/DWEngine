#include "OpusRecentGameplay.h"
#include "DWRuleWorldSubsystem.h"
#include "DWRuleModernCombat.h"
#include <limits>
using namespace DWRules;
namespace
{
bool AuraSource(const FDWRuleLibrary& L,FName Id,FDWBuildSource& Out,double& Reservation)
{
 FString Error;TArray<FDWContribution> Rows;bool Aura=false;Reservation=0;
 if(!L.Aggregate(Id,{},std::numeric_limits<double>::max(),Out,Rows,Error))return false;
 for(const auto& T:Out.Traits)for(const auto& E:T.Sheet)if(E.Trigger==TEXT("aura_skill")){Aura=true;Reservation+=T.Params.FindRef(TEXT("reservation_percent"));}
 return Aura;
}
struct FAuraCache { TWeakObjectPtr<UDWRuleWorldSubsystem> Rules;const FDWRuleSession* Session=nullptr;FString Key;TArray<FOpusAuraChoice> Choices; };
TArray<FAuraCache> AuraCache;
}
TArray<FOpusAuraChoice> UOpusRecentGameplay::AuraChoices(UDWRuleWorldSubsystem* Rules)
{
 TArray<FOpusAuraChoice> Out;if(!Rules||!Rules->PlayerSession())return Out;
 const auto* Session=Rules->PlayerSession();const FString Key=FString::Printf(TEXT("%llu"),static_cast<unsigned long long>(Session->LoadoutRevision));
 for(const auto& Cache:AuraCache)if(Cache.Rules==Rules&&Cache.Session==Session&&Cache.Key==Key)return Cache.Choices;
 const auto& L=Rules->Library();TSet<FName> Seen;
 auto Add=[&](FName Id){if(Seen.Contains(Id))return;Seen.Add(Id);FDWBuildSource Source;double Reserved=0;if(!AuraSource(L,Id,Source,Reserved))return;const auto* Def=L.Builds.Find(Id);if(!Def)return;FOpusAuraChoice C;C.Id=Id;C.Name=Def->DisplayName;C.Description=Def->Description;C.Reservation=Reserved;Out.Add(C);};
 auto Gear=Rules->PlayerSession()->Loadout;Gear.PassiveParts.Reset();Gear.RuntimeGroups.Reset();
 for(const auto& Part:Gear.EquipmentParts(L))
 {
  if(!Part.bEquipment||Part.Collection.Slot==TEXT("weapon")||Part.Collection.Slot==TEXT("offhand"))continue;
  for(FName Id:Part.Collection.Skills)Add(Id);
  FDWBuildSource S;TArray<FDWContribution> Rows;FString Error;
  if(L.Aggregate(NAME_None,{Part},std::numeric_limits<double>::max(),S,Rows,Error))for(const auto& T:S.Traits)for(const auto& E:T.Sheet)if(!E.GrantSkill.IsNone())Add(E.GrantSkill);
 }
 Out.Sort([](const auto& A,const auto& B){return A.Id.LexicalLess(B.Id);});
 AuraCache.RemoveAll([&](const auto& C){return !C.Rules.IsValid()||C.Rules==Rules;});if(AuraCache.Num()>=64)AuraCache.RemoveAt(0);
 AuraCache.Add({Rules,Session,Key,Out});return Out;
}
bool UOpusRecentGameplay::ApplyAuras(UDWRuleWorldSubsystem* Rules,const TArray<FName>& Ids,FString& Error)
{
 if(!Rules||!Rules->PlayerSession()){Error=TEXT("no player session");return false;}
 if(Ids.Num()>4){Error=TEXT("光环最多装配四个");return false;}
 const auto Choices=AuraChoices(Rules);TSet<FName> Seen;double Reserved=0;TArray<FDWCollectionPart> Parts;
 for(FName Id:Ids)
 {
  if(Seen.Contains(Id)){Error=TEXT("同一光环不能重复启用");return false;}Seen.Add(Id);
  const auto* Choice=Choices.FindByPredicate([&](const auto& C){return C.Id==Id;});if(!Choice){Error=TEXT("请先装备授予该光环的非武器物品");return false;}
  Reserved+=Choice->Reservation;if(Reserved>100+1.e-7){Error=TEXT("光环封印魔力总比例不能超过100%");return false;}
  FDWCollectionPart P;P.Collection.Id=FName(*(TEXT("__aura_")+Id.ToString()));P.Collection.Name=Choice->Name;P.Label=TEXT("光环 · ")+Choice->Name;
  for(FName Trait:Rules->Library().Builds[Id].Traits){FDWTraitEntry E;E.Id=Trait;P.Collection.Traits.Add(E);}Parts.Add(MoveTemp(P));
 }
 auto* S=Rules->PlayerSession();auto Previous=S->Loadout,Next=Previous;const double Mana=S->Character.Mana,OldReserved=S->Character.ManaReservationPercent;Next.RuntimeGroups.Add(TEXT("auras"),Parts);
 if(!S->SetLoadout(MoveTemp(Next),Error)){FString Ignored;S->SetLoadout(MoveTemp(Previous),Ignored);S->Character.Mana=Mana;S->Character.ReserveMana(OldReserved);return false;}
 S->Character.Mana=Mana;S->Character.ReserveMana(Reserved);return true;
}
TArray<FOpusMonsterEmission> UOpusRecentGameplay::DrainMonsterEmissions(UDWRuleWorldSubsystem* Rules)
{
 TArray<FOpusMonsterEmission> Out;auto* S=Rules?Rules->PlayerSession():nullptr;if(!S)return Out;
 for(const auto& E:S->Combat.TemporaryRequests){FOpusMonsterEmission R;R.Kind=E.Kind;R.Count=E.Count;R.Duration=E.Duration;R.Radius=E.Radius*100;R.Angle=E.Angle;R.Origin=FVector(E.Origin)*100;R.Invulnerable=E.bInvulnerable;Out.Add(R);}S->Combat.TemporaryRequests.Reset();return Out;
}
void UOpusRecentGameplay::ReleaseTemporaryMonster(UDWRuleWorldSubsystem* Rules,int32 Id)
{if(!Rules)return;SetTargetInvulnerable(Rules,Id,false);Rules->ForgetGameTarget(Id);if(auto* S=Rules->PlayerSession())S->Combat.TemporaryMonsters=FMath::Max(0,S->Combat.TemporaryMonsters-1);}
void UOpusRecentGameplay::SetTargetInvulnerable(UDWRuleWorldSubsystem* Rules,int32 Id,bool Enabled)
{if(Rules)Rules->RuleWorld().SetInvulnerable(Rules->ResolveTargetId(Id),Enabled);}
double UOpusRecentGameplay::ManaReservation(UDWRuleWorldSubsystem* Rules){auto* S=Rules?Rules->PlayerSession():nullptr;return S?S->Character.ManaReservationPercent:0;}
double UOpusRecentGameplay::UsableMana(UDWRuleWorldSubsystem* Rules){auto* S=Rules?Rules->PlayerSession():nullptr;return S?S->Character.ManaCapacity():0;}
bool UOpusRecentGameplay::SelfTarget(UDWRuleWorldSubsystem* Rules)
{
 const auto* S=Rules?Rules->PlayerSession():nullptr;if(!S||S->Combat.Hero.Plan<0)return false;
 const auto& P=S->Host.World.GetPlan(S->Combat.Hero.Plan);const int32 Cast=P.FindEvent(TEXT("cast"));
 FString Active=P.ActiveId.ToString();int32 Suffix;if(Active.FindChar('~',Suffix))Active.LeftInline(Suffix);
 for(const auto& R:P.Rules)if(R.Event==Cast)
 {
  bool Match=true;for(int32 I=R.ConditionFirst;I<R.ConditionFirst+R.ConditionCount;++I){const auto& C=P.Conditions[I];if(C.Kind==ECond::Active&&(C.Compare==ECompare::Ne?FName(*Active)==C.Active:FName(*Active)!=C.Active))Match=false;}if(!Match)continue;
  for(int32 I=R.ActionFirst;I<R.ActionFirst+R.ActionCount;++I){const auto& A=P.Actions[I];if(A.Op==EOp::MaintainEmitter||(A.Op==EOp::ApplyStatus&&A.Origin==EOrigin::Owner))return true;}
 }return false;
}
bool UOpusRecentGameplay::PointTarget(UDWRuleWorldSubsystem* Rules)
{const auto* S=Rules?Rules->PlayerSession():nullptr;if(!S||S->Combat.Hero.Plan<0)return false;for(const auto& A:S->Host.World.GetPlan(S->Combat.Hero.Plan).Actions)if(A.Op==EOp::Emit&&(A.Flags&FDWActionDef::Parabolic))return true;return false;}
double UOpusRecentGameplay::SkillCooldown(UDWRuleWorldSubsystem* Rules,int32 Slot)
{auto* S=Rules?Rules->PlayerSession():nullptr;return S&&S->Skills.IsValidIndex(Slot)?FMath::Max(0.,S->Combat.SkillReadyAt.FindRef(S->Skills[Slot].Id)-S->Combat.Time):0;}
int32 UOpusRecentGameplay::Mirrors(UDWRuleWorldSubsystem* Rules){auto* S=Rules?Rules->PlayerSession():nullptr;return S&&S->Combat.Modern?S->Combat.Modern->Mirrors:0;}
TArray<FOpusGroundView> UOpusRecentGameplay::Grounds(UDWRuleWorldSubsystem* Rules)
{
 TArray<FOpusGroundView> Out;auto* S=Rules?Rules->PlayerSession():nullptr;if(!S||!S->Combat.Modern)return Out;
 for(const auto& G:S->Combat.Modern->Grounds){FOpusGroundView V;V.Type=G.Type;V.Position=FVector(G.Position)*100;V.Normal=FVector(G.Normal);V.Radius=G.Radius*100;V.Remaining=G.Left;V.Element=G.Element;Out.Add(V);}return Out;
}
int32 UOpusRecentGameplay::Captures(UDWRuleWorldSubsystem* Rules){auto* S=Rules?Rules->PlayerSession():nullptr;return S&&S->Combat.Modern?S->Combat.Modern->Captured:0;}
