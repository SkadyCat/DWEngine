#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleProjectileAffixes.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWHomingTest, "DWRules.Opus.ProjectileHoming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWHomingTest::RunTest(const FString&)
{
 FDWRuleLibrary Library; FString Error;
 if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),Library,Error)){AddError(Error);return false;}
 auto Sheet=[&](FDWRuleCharacter& Character){
  TArray<FDWHeldTrait> Held;
  for(const FName Id:{FName(TEXT("proj_retarget")),FName(TEXT("proj_homing_turn")),FName(TEXT("proj_homing_range"))}){
   FDWHeldTrait H;H.Source=Library.Traits.FindChecked(Id).Source;H.Source.Params.Add(TEXT("n"),Id==TEXT("proj_retarget")?1.:31.);Held.Add(H);
  }
  return Character.SetSheet(Held,Error);
 };
 for(const int32 FPS:{30,60,120})for(const double Direction:{0.,-UE_DOUBLE_PI/2}){
  FDWRuleHost Host;if(!Host.SetLibrary(Library,Error)){AddError(Error);return false;}
  FDWRuleCharacter Character;if(!Sheet(Character)){AddError(Error);return false;}
  FDWCombatRuntime R(Host,Character,19);R.Hero.Owner=Host.World.AddOwner(1,0);R.Hero.Plan=Host.CompileNamedBuild(TEXT("frost_lance"),{},Error);
  if(!TestTrue(TEXT("real projectile plan compiles: ")+Error,R.Hero.Plan>=0))return false;
  auto& M=R.Enemies.AddDefaulted_GetRef();M.Id=101;M.Position={0,1,.6};M.Radius=.1;M.Health=M.MaxHealth=10000;
  FDWProjectileFire F;F.Position={0,0,.6};F.Direction=Direction;F.Speed=24;F.Life=1.2;F.Radius=.1;F.Damage=10;R.Projectiles->Fire(F);
  const auto& C=Host.World.Projectiles();const uint32 Id=C.Id[0];const FVector3f Position=C.Position[0];const float Life=C.Life[0];
  R.Projectiles->OnMotion(0,1./FPS);
  TestTrue(TEXT("guidance never teleports"),C.Position[0]==Position);
  TestEqual(TEXT("guidance never extends lifetime"),C.Life[0],Life);
  for(int32 Frame=0;Frame<FPS*1.2&&M.Health==M.MaxHealth;++Frame){
   Host.World.Targets().Reset();Host.World.Targets().Add(FVector3f(M.Position),float(M.Radius),float(M.Health),1,M.Id);
   R.BeforeStep(1./FPS);Host.World.Step(1.f/FPS);R.ConsumeRows();
  }
  TestTrue(*FString::Printf(TEXT("%d FPS direction %.3f hits through real sweep within original lifetime"),FPS,Direction),M.Health<M.MaxHealth);
  if(const auto* St=R.Projectiles->States.Find(Id))TestTrue(TEXT("launch origin is immutable"),(*St)->Origin.Equals(FVector3d(Position),1.e-6));
 }
 {
  FDWRuleHost Host;if(!Host.SetLibrary(Library,Error))return false;FDWRuleCharacter Character;if(!Sheet(Character))return false;
  FDWCombatRuntime R(Host,Character);R.Hero.Owner=Host.World.AddOwner(1,0);R.Hero.Plan=Host.CompileNamedBuild(TEXT("frost_lance"),{},Error);
  for(uint32 Id:{101u,102u}){auto& M=R.Enemies.AddDefaulted_GetRef();M.Id=Id;M.Position=Id==101?FVector3d(1,0,.6):FVector3d(0,1.5,.6);M.Radius=.1;M.Health=M.MaxHealth=10000;}
  FDWProjectileFire F;F.Position={0,0,.6};F.Speed=24;F.Life=1.2;F.Exclude=101;R.Projectiles->Fire(F);
  const auto& C=Host.World.Projectiles();auto& St=*R.Projectiles->States.FindChecked(C.Id[0]);const auto Origin=St.Origin;
  R.Projectiles->OnMotion(0,1./60);
  TestEqual(TEXT("previously hit target is excluded"),St.Target,102u);TestEqual(TEXT("initial acquisition consumes one budget"),St.Retargets,1);
  TestTrue(TEXT("close turn brakes"),C.Velocity[0].Size()<24);
  const int32 Copy=R.Projectiles->Clone(0,C.Root[0]);
  TestTrue(TEXT("split does not inherit temporary braking"),Copy>=0&&FMath::Abs(C.Velocity[Copy].Size()-24)<1.e-3);
  const auto Heading=C.Velocity[0].GetSafeNormal();R.Enemies[1].Position=FVector3d(C.Position[0]+Heading*2);
  R.Projectiles->OnMotion(0,1./60);TestTrue(TEXT("alignment restores original speed"),FMath::Abs(C.Velocity[0].Size()-24)<1.e-3);
  R.Enemies[1].Position={15,0,.6};R.Projectiles->OnMotion(0,1./60);
  TestEqual(TEXT("out of range target is released"),St.Target,InvalidId);TestEqual(TEXT("failed acquisition costs no budget"),St.Retargets,1);
  TestTrue(TEXT("retargeting preserves launch origin and current position"),St.Origin==Origin&&C.Position[0].Equals(FVector3f(Origin),1.e-6f));
 }
 return true;
}
#endif
