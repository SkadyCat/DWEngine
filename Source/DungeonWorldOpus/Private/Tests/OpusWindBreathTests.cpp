#include "DWRuleLibraryLoader.h"
#include "DWRuleSession.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
namespace
{
struct FWindFixture
{
 FDWRuleHost Host;TUniquePtr<FDWRuleSession> S;FString Error;int32 Owner=-1,Plan=-1;
 bool Init()
 {
  FDWRuleLibrary L;if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),L,Error)||!Host.SetLibrary(MoveTemp(L),Error))return false;
  Owner=Host.World.AddOwner(1,0);Host.World.UpdateOwner(Owner,FVector3f(0,0,.6f),FVector3f(1,0,0));Host.World.Targets().Add(FVector3f(0,0,.6f),.4f,400,0,1);
  S=MakeUnique<FDWRuleSession>(Host,Owner,1,71);
  FDWBuildSource Neutral;Neutral.ActiveId=TEXT("__wind_probe");auto& Trait=Neutral.Traits.AddDefaulted_GetRef();Trait.Id=Neutral.ActiveId;
  FDWModifierSource Crit;Crit.Stat=TEXT("owner.critical_chance");Crit.Op=EModOp::Set;Crit.Value=FDWExprSource::Number(0);Trait.Modifiers.Add(Crit);
  Plan=Host.CompileSource(Neutral,Error);
  S->Character.RandomOverride=[](){return 1.;};return Plan>=0;
 }
 void Enemy(uint32 Id,FVector3d P)
 {
  const int32 O=Host.World.AddOwner(Id,1);Host.World.UpdateOwner(O,FVector3f(P),FVector3f(-1,0,0));Host.World.Targets().Add(FVector3f(P),.2f,1000,1,Id);
  FDWCombatEnemy E;E.Id=Id;E.Owner=O;E.Position=P;E.Health=E.MaxHealth=1000;E.Radius=.2;S->Combat.Enemies.Add(E);
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindRawTest,"DungeonWorldOpus.WindBreath.OriginalDamageBeforeDefense",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWindRawTest::RunTest(const FString&)
{
 FWindFixture F;if(!F.Init()){AddError(F.Error);return false;}
 F.Enemy(10,FVector3d(2,0,.6));F.Host.World.Targets().Resistance[0]=50;
 double Original=0,Taken=0;F.S->Combat.OnHeroHurt=[&](const FDWCharacterHit& H,const FDWTakenHit& T){Original=H.OriginalAmount;Taken=T.Taken;};
 const int32 Root=F.Host.World.OpenRoot(F.Plan,F.S->Combat.Enemies[0].Owner,19);FDWDamageInfo Info;
 const auto Hit=F.Host.World.HostHit(Root,1,100,uint8(EElement::Fire),Info,true);
 F.Host.World.ReleaseHeldRoot(Root);F.S->Combat.ConsumeRows();
 TestEqual(TEXT("player resistance halves damage"),Hit.Applied,50.f);TestEqual(TEXT("original before player resistance"),Original,100.);TestEqual(TEXT("resolved after resistance"),Taken,50.);
 F.S->Character.Base.Bypass=0;F.S->Character.MaxShield=1000;F.S->Character.ShieldCap=1000;F.S->Character.Shield=1000;
 FDWCharacterHit Shield;Shield.Amount=40;Shield.OriginalAmount=100;Shield.Kind.bAttack=true;
 const auto ShieldHit=F.S->Combat.HeroDamage(Shield);TestTrue(TEXT("shield-only landed hit remains observable"),ShieldHit.ShieldLost>0&&ShieldHit.LifeLost==0&&Original==100);
 F.S->Combat.CanReceiveHeroDamage=[](){return false;};Original=0;F.S->Combat.HeroDamage(Shield);TestEqual(TEXT("immune hits do not emit"),Original,0.);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindFixedTest,"DungeonWorldOpus.WindBreath.FixedReflectedDamage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWindFixedTest::RunTest(const FString&)
{
 FWindFixture F;if(!F.Init()){AddError(F.Error);return false;}
 for(int32 I=0;I<4;I++){const double A=I*UE_DOUBLE_PI/2;F.Enemy(10+I,FVector3d(FMath::Cos(A)*(I==3?9:2),FMath::Sin(A)*(I==3?9:2),.6));}
 // Use a real offensive plan: reflection must not be amplified even if the
 // caller supplies an ordinary skill plan instead of the adapter's neutral plan.
 const int32 Offensive=F.Host.CompileNamedBuild(TEXT("earth_wave"),{FName(TEXT("flat_physical")),FName(TEXT("inc_physical"))},F.Error);
 if(!TestTrue(TEXT("offensive plan"),Offensive>=0))return false;
 FDWStrikeOptions O;O.Plan=Offensive;O.Effect=TEXT("reflected_pulse");O.bArea=false;O.bSilent=true;O.Info.bReflected=true;
 const auto Hits=F.S->Combat.Strike(FVector3d(0,0,.6),5,80,3,O);
 TestEqual(TEXT("all cardinal directions, outside excluded"),Hits.Num(),3);
 for(const auto& H:Hits){TestEqual(TEXT("exact reflected damage"),H.Applied,80.f);TestFalse(TEXT("reflection cannot crit"),H.bCritical);O.Exclude.Add(H.Id);}
 TestEqual(TEXT("same wave hit set prevents repeated damage"),F.S->Combat.Strike(FVector3d(0,0,.6),5,80,3,O).Num(),0);
 const auto Events=F.S->Character.CombatEvents.Num();F.S->Combat.ConsumeRows();TestEqual(TEXT("no on-hit recursive character effects"),F.S->Character.CombatEvents.Num(),Events);
 return true;
}
#endif
