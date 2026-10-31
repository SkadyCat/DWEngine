#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleSession.h"
#include "DWRuleMinions.h"
#include "DWRuleWorldSubsystem.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWSeekRangeTest, "DWO.AI.SeekRangeTraits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWSeekRangeTest::RunTest(const FString&)
{
 FString Error;
 auto* Rules = NewObject<UDWRuleWorldSubsystem>(NewObject<UWorld>());
 Rules->Configure(nullptr);
 if (!TestTrue(TEXT("whole current authoring catalog loads"), Rules->LoadRuleLibrary(TEXT("Rules"), Error))) { AddError(Error); return false; }
 if (!TestTrue(TEXT("register first monster"), Rules->RegisterMonster(101, TEXT("claw_beast"), FVector::ZeroVector, Error) >= 0) ||
     !TestTrue(TEXT("register second monster"), Rules->RegisterMonster(102, TEXT("claw_beast"), FVector::ZeroVector, Error) >= 0)) { AddError(Error); return false; }
 TestEqual(TEXT("authored base is twenty meters"), Rules->GetMonsterSeekRange(101, 3), 20.f);
 if (!TestTrue(TEXT("set individual range"), Rules->SetMonsterTraits(101, TEXT("[{\"id\":\"monster_seek_range\",\"params\":{\"n\":4}}]"), Error))) return false;
 TestEqual(TEXT("first monster reads own trait"), Rules->GetMonsterSeekRange(101), 4.f);
 TestEqual(TEXT("second monster unaffected"), Rules->GetMonsterSeekRange(102), 20.f);
 Rules->SetMonsterTraits(101, TEXT("[{\"id\":\"monster_seek_range\",\"params\":{\"n\":0}}]"), Error);
 TestEqual(TEXT("explicit zero never uses fallback"), Rules->GetMonsterSeekRange(101), 0.f);
 Rules->SetMonsterTraits(101, TEXT("[]"), Error);
 TestEqual(TEXT("legacy monster uses configured default"), Rules->GetMonsterSeekRange(101, 3), 3.f);

 FDWRuleLibrary L; FDWRuleHost Host;
 if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"), L, Error) || !Host.SetLibrary(L, Error)) { AddError(Error); return false; }
 const int32 Owner = Host.World.AddOwner(1, 0);
 FDWRuleCharacter C; FDWCombatRuntime R(Host, C); R.Hero.Owner = Owner;
 const int32 Plan = Host.CompileNamedBuild(TEXT("sentinel"), {}, Error);
 if (!TestTrue(TEXT("summon plan compiled"), Plan >= 0)) return false;
 FDWSummon Source; Source.Root = Host.World.OpenRoot(Plan, Owner, 1); Source.Kind = 0; Source.Interval = 1; Source.Range = 10; Source.Expire = 20; Source.Id = 900;
 auto* St = R.Minions->Copy(Source);
 if (!TestNotNull(TEXT("real native summon"), St)) return false;
 auto& Sm = R.Minions->SummonOf(*St);
 FDWCombatEnemy M; M.Id = 301; M.Position = FVector3d(6, 0, 0); M.Radius = 0; M.Health = M.MaxHealth = 100;
 R.Enemies.Add(M);
 Host.World.Targets().Add(FVector3f(M.Position), 0, 100, 1, M.Id, M.Resistance);
 TestTrue(TEXT("legacy summon acquires within original ten meters"), R.Minions->Target(Sm, INDEX_NONE) >= 0);
 auto Apply = [&](double Base, double Percent) {
  TArray<FDWHeldTrait> Entries;
  auto BaseSource = L.Traits.FindChecked(TEXT("minion_seek_range")).Source; BaseSource.Params.Add(TEXT("n"), Base);
  auto PercentSource = L.Traits.FindChecked(TEXT("minion_seek")).Source; PercentSource.Params.Add(TEXT("n"), Percent);
  FDWHeldTrait A; A.Source = BaseSource; Entries.Add(A); A.Source = PercentSource; Entries.Add(A);
  return C.SetSheet(Entries, Error);
 };
 if (!TestTrue(TEXT("base range sheet compiled"), Apply(4, 0))) return false;
 TestEqual(TEXT("smaller range drops target"), R.Minions->Target(Sm, INDEX_NONE), INDEX_NONE);
 TestEqual(TEXT("actual native summon range updates"), Sm.Range, 4.f);
 Apply(4, 50);
 TestTrue(TEXT("existing percentage extends actual acquisition"), R.Minions->Target(Sm, INDEX_NONE) >= 0);
 TestEqual(TEXT("percentage applied once"), Sm.Range, 6.f);
 Apply(0, 50); R.Enemies[0].Position = FVector3d(Sm.Position);
 TestEqual(TEXT("zero disables even overlapping enemy"), R.Minions->Target(Sm, 0), INDEX_NONE);
 TestEqual(TEXT("stale target cleared"), St->Target, InvalidId);
 Apply(-4, -200); R.Minions->Target(Sm, 0);
 TestEqual(TEXT("negative base cannot become positive through negative multiplier"), Sm.Range, 0.f);
 return true;
}
#endif
