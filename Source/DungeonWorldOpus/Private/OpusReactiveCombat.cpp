#include "OpusReactiveCombat.h"
#include "DWRuleWorldSubsystem.h"

bool UOpusReactiveCombat::BindPlayer(UDWRuleWorldSubsystem* InRules)
{
 if(Rules)return Rules==InRules;
 if(!InRules||!InRules->PlayerSession())return false;
 auto& S=*InRules->PlayerSession();FString Error;
 DWRules::FDWBuildSource Source;Source.ActiveId=TEXT("__reflected_pulse");Source.Traits.AddDefaulted_GetRef().Id=Source.ActiveId;
 ReflectionPlan=S.Host.CompileSource(Source,Error);if(ReflectionPlan<0)return false;
 Rules=InRules;auto Previous=MoveTemp(S.Combat.OnHeroHurt);
 const TWeakObjectPtr<UOpusReactiveCombat> Weak(this);
 S.Combat.OnHeroHurt=[Weak,Previous=MoveTemp(Previous)](const DWRules::FDWCharacterHit& Hit,const DWRules::FDWTakenHit& Taken)
 {
  if(Previous)Previous(Hit,Taken);
  if(auto* Self=Weak.Get();Self&&Self->Rules&&Self->Rules->PlayerSession()&&Taken.Taken>0&&!Taken.bEvaded)
   Self->OnLandedAttack.Broadcast(Hit.OriginalAmount,Hit.bDot,Self->Rules->PlayerSession()->Character.bDead);
 };
 return true;
}
TArray<int32> UOpusReactiveCombat::ReflectedPulse(FVector Origin,double RadiusCm,double Amount,const TArray<int32>& Excluded)
{
 TArray<int32> Out;auto* S=Rules?Rules->PlayerSession():nullptr;
 if(!S||S->Character.bDead||ReflectionPlan<0||RadiusCm<=0||Amount<=0||!FMath::IsFinite(Amount))return Out;
 DWRules::FDWStrikeOptions O;O.Plan=ReflectionPlan;O.Effect=TEXT("reflected_pulse");O.bArea=false;O.bSilent=true;
 O.Info.bReflected=true;O.Info.bHasCenter=true;O.Info.Center=FVector3f(Origin/100.);O.Info.Radius=RadiusCm/100.;
 for(int32 Id:Excluded)O.Exclude.Add(uint32(Id));
 for(const auto& H:S->Combat.Strike(FVector3d(Origin/100.),RadiusCm/100.,Amount,uint8(DWRules::EElement::Physical),O))Out.Add(int32(H.Id));
 return Out;
}
