#include "OpusDeliveryPolicy.h"
#include "DWRuleWorldSubsystem.h"
#include "DWRuleModernCombat.h"

bool UOpusDeliveryPolicy::BindPolicy(UDWRuleWorldSubsystem* InRules,UObject* Observer,FName DamageFunction,FName MotionFunction)
{
 if(Bound)return Rules==InRules;
 if(!InRules||!InRules->PlayerSession()||!Observer||!Observer->FindFunction(DamageFunction)||!Observer->FindFunction(MotionFunction))return false;
 Rules=InRules;Bound=true;Permission.BindUFunction(Observer,DamageFunction);Motion.BindUFunction(Observer,MotionFunction);
 auto& C=Rules->PlayerSession()->Combat;auto PreviousDamage=MoveTemp(C.CanReceiveHeroDamage);auto PreviousMotion=MoveTemp(C.OnProjectileMotion);
 const TWeakObjectPtr<UOpusDeliveryPolicy> Weak(this);
 C.CanReceiveHeroDamage=[Weak,Previous=MoveTemp(PreviousDamage)](){
  if(Previous&&!Previous())return false;
  auto* Self=Weak.Get();return !Self||!Self->Permission.IsBound()||Self->Permission.Execute();
 };
 C.OnProjectileMotion=[Weak,Previous=MoveTemp(PreviousMotion)](int32 Index,float Dt){
  if(Previous)Previous(Index,Dt);
  if(auto* Self=Weak.Get();Self&&Self->Motion.IsBound())Self->Motion.Execute(Index,Dt);
 };
 return true;
}
double UOpusDeliveryPolicy::LiveStat(FName Path)
{
 if(!Rules||!Rules->PlayerSession())return 0;
 auto& S=*Rules->PlayerSession();const double Clock=Rules->RuleWorld().GetClock();
 // A status may begin or expire inside the world step. Refresh the composed
 // status traits once per simulation substep before evaluating delivery policy.
 TArray<FName> Current;Rules->RuleWorld().GetGrantedTraits(S.TargetId,Current);
 if(SheetClock!=Clock||Current!=Granted){S.Host.RefreshGrantedDefense();FString Error;S.RefreshSheet(Error);SheetClock=Clock;Granted=MoveTemp(Current);}
 return S.Character.S(Path);
}
FName UOpusDeliveryPolicy::SelectedSkill() const
{
 if(!Rules||!Rules->PlayerSession())return NAME_None;
 const auto& S=*Rules->PlayerSession();return S.Skills.IsValidIndex(S.Selected)?S.Skills[S.Selected].Id:NAME_None;
}
FName UOpusDeliveryPolicy::SkillAt(int32 Slot) const
{
 if(!Rules||!Rules->PlayerSession())return NAME_None;
 const auto& S=*Rules->PlayerSession();return S.Skills.IsValidIndex(Slot)?S.Skills[Slot].Id:NAME_None;
}
int32 UOpusDeliveryPolicy::ProjectileCount() const {return Rules?Rules->RuleWorld().Projectiles().Num:0;}
FOpusProjectileState UOpusDeliveryPolicy::Projectile(int32 Index) const
{
 FOpusProjectileState Out;if(!Rules||!Rules->PlayerSession())return Out;
 const auto& W=Rules->RuleWorld();const auto& C=W.Projectiles();
 if(Index<0||Index>=C.Num||(C.Flags[Index]&DWRules::FDWProjectiles::Dead)||!W.IsRootValid(C.Root[Index]))return Out;
 const auto* Source=W.GetOwner(W.GetRoot(C.Root[Index]).Owner);const auto* Hero=W.GetOwner(Rules->PlayerSession()->Combat.Hero.Owner);
 Out.Valid=Source&&Hero;Out.Hostile=Out.Valid&&Source->Team!=Hero->Team;
 Out.Position=FVector(C.Position[Index])*100;Out.Velocity=FVector(C.Velocity[Index])*100;Out.Id=int32(C.Id[Index]);Out.Age=C.Age[Index];Out.Kind=W.GetPlan(W.GetRoot(C.Root[Index]).Plan).KindNames[C.Kind[Index]];
 if(const auto* Shot=Rules->PlayerSession()->Combat.Modern->Shots.Find(C.Id[Index]))Out.GroundTint=Shot->GroundTint;return Out;
}
void UOpusDeliveryPolicy::SetVelocity(int32 Index,FVector Velocity)
{
 if(Rules&&Rules->PlayerSession())Rules->PlayerSession()->Combat.SetProjectileVelocity(Index,FVector3f(Velocity/100));
}
