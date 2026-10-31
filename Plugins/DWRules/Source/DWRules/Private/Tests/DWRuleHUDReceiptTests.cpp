#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DWRuleCombat.h"
#include "DWRuleLibraryLoader.h"
#include "Misc/Paths.h"
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleHUDObserverTest,"DWRules.Tower.HUDObserver",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDWRuleHUDObserverTest::RunTest(const FString& Parameters)
{
 FDWRuleLibrary Library; FString Error;
 if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),Library,Error)) { AddError(Error); return false; }
 FDWRuleHost Host; if(!Host.SetLibrary(Library,Error)) { AddError(Error); return false; }
 FDWRuleCharacter Observed(17),Control(17); TArray<FDWHeldTrait> Held;
 if(!Observed.SetSheet(Held,Error)||!Control.SetSheet(Held,Error)) { AddError(Error); return false; }
 Observed.Life=Control.Life=200; Observed.Shield=Control.Shield=20;
 FDWCombatRuntime Combat(Host,Observed,7); int32 Count=0; double Seen=0;
 Combat.OnHeroHurt=[&](const FDWCharacterHit& Hit,const FDWTakenHit& Taken)
 {
  ++Count; Seen=Taken.LifeLost+Taken.ShieldLost;
  TestEqual(TEXT("observer runs after pools are committed"),Observed.Life,Control.Life);
  TestEqual(TEXT("observer preserves original amount"),Hit.Amount,70.);
 };
 FDWCharacterHit Hit; Hit.Amount=70; Hit.Element=EElement::Physical; Hit.Kind.bAttack=true;
 const auto Expected=Control.TakeHit(Hit); const auto Actual=Combat.HeroDamage(Hit);
 TestEqual(TEXT("exactly one observer call"),Count,1);
 TestEqual(TEXT("reported value is actual life plus shield loss"),Seen,Expected.LifeLost+Expected.ShieldLost);
 TestEqual(TEXT("observer leaves damage return unchanged"),Actual.Taken,Expected.Taken);
 TestEqual(TEXT("observer leaves life unchanged"),Observed.Life,Control.Life);
 TestEqual(TEXT("observer leaves shield unchanged"),Observed.Shield,Control.Shield);
 TestEqual(TEXT("observer leaves event chain unchanged"),Observed.CombatEvents.Num(),Control.CombatEvents.Num());
 Combat.OnHeroHurt=nullptr;
 Hit.Amount=0; TestEqual(TEXT("unobserved zero damage stays zero"),Combat.HeroDamage(Hit).LifeLost,0.);
 return true;
}
#endif
