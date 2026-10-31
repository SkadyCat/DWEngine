#include "OpusCharacterEvents.h"
#include "DWRuleWorldSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include <limits>

bool UOpusCharacterEvents::BindPlayer(UDWRuleWorldSubsystem* InRules)
{
 if(Bound)return Rules==InRules;
 if(!InRules||!InRules->PlayerSession())return false;
 Rules=InRules;Bound=true;
 auto& Combat=Rules->PlayerSession()->Combat;
 auto Previous=MoveTemp(Combat.OnHeroHurt);
 const TWeakObjectPtr<UOpusCharacterEvents> Weak(this);
 Combat.OnHeroHurt=[Weak,Previous=MoveTemp(Previous)](const DWRules::FDWCharacterHit& Hit,const DWRules::FDWTakenHit& Taken)
 {
  if(Previous)Previous(Hit,Taken);
  if(auto* Self=Weak.Get();Self&&Self->Rules&&Self->Rules->PlayerSession())
   // Taken includes the mitigated damage allocated to shield: shield absorption
   // must not suppress stun buildup, even when LifeLost is zero.
   if(Taken.Taken>0&&!Taken.bEvaded)Self->OnResolvedHit.Broadcast(Taken.Taken,Hit.bDot,Self->Rules->PlayerSession()->Character.bDead);
 };
 return true;
}
double UOpusCharacterEvents::SheetStat(FName Path) const
{
 return Rules&&Rules->PlayerSession()?Rules->PlayerSession()->Character.S(Path):0;
}
double UOpusCharacterEvents::SimulationClock() const
{
 return Rules?Rules->RuleWorld().GetClock():0;
}
void UOpusCharacterEvents::UseExternalStun(bool Enabled)
{
 if(!Rules||!Rules->PlayerSession()||ExternalStun==Enabled)return;
 auto& Base=Rules->PlayerSession()->Character.Base;
 if(Enabled){SingleHitFraction=Base.StunFraction;Base.StunFraction=std::numeric_limits<double>::max();}
 else Base.StunFraction=SingleHitFraction;
 ExternalStun=Enabled;
}
void UOpusCharacterEvents::EndPlay(const EEndPlayReason::Type Reason)
{
 UseExternalStun(false);Super::EndPlay(Reason);
}
bool UOpusCharacterEvents::TestHit(double Amount,bool Dot)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("DWOTransient"))||!Rules||!Rules->PlayerSession())return false;
 auto& Session=*Rules->PlayerSession();
 auto Previous=MoveTemp(Session.Character.RandomOverride);
 Session.Character.RandomOverride=[](){return 1.0;};
 DWRules::FDWCharacterHit Hit;Hit.Amount=Amount;Hit.Element=DWRules::EElement::Physical;Hit.bDot=Dot;
 Session.Combat.HeroDamage(Hit);
 Session.Character.RandomOverride=MoveTemp(Previous);return true;
}
