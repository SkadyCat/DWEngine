#include "DWRuleAilments.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleMinions.h"
#include "DWRuleSession.h"
#include "DWRuleStateReader.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWSessionLifecycleTest, "DWRules.Migration.SessionLifecycle",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWSessionLifecycleTest::RunTest(const FString &Parameters)
{
	FDWRuleHost Host;
	FDWRuleLibrary Library;
	FString Error;
	if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir() / TEXT("Rules"), Library, Error) ||
		!Host.SetLibrary(Library, Error))
	{
		AddError(Error);
		return false;
	}
	FDWCollection Gem;Gem.Id=TEXT("gem_sentinel_fire_nova");Gem.Category=TEXT("gem");FDWTraitEntry Payload;Payload.Id=TEXT("inc_fire");Gem.Traits.Add(Payload);Library.Items.Add(Gem.Id,Gem);Host.Library.Items.Add(Gem.Id,Gem);
    const int32 Owner = Host.World.AddOwner(1, 0);
	FDWRuleSession Session(Host, Owner, 0x40000000);
	FDWRuleProfile Profile;
	TestTrue(TEXT("progression starts from authored weapon"), Profile.StartProgression(Library, Error));
	TestEqual(TEXT("one T1 common instance"), Profile.Loadout.Instances[0].Tier, 1);
	TestTrue(TEXT("three other active slots stay empty"), Profile.Loadout.Skills[1].IsNone() &&
															  Profile.Loadout.Skills[2].IsNone() &&
															  Profile.Loadout.Skills[3].IsNone());
	Profile.Loadout.Owned.Add(TEXT("gem_sentinel_fire_nova"), 1);
	Profile.Loadout.Owned.Add(TEXT("sentinel"), 1);
	TestTrue(TEXT("owned summon skill can occupy another slot"),
			 Profile.Loadout.ChangeSkill(Library, 1, TEXT("sentinel"), Error));
	TestTrue(TEXT("socket gem"),
			 Profile.Loadout.ChangeGem(Library, TEXT("sentinel"), 0, TEXT("gem_sentinel_fire_nova"), Error));
	TestFalse(TEXT("cannot duplicate an owned gem"),
			  Profile.Loadout.ChangeGem(Library, TEXT("frost_lance"), 0, TEXT("gem_sentinel_fire_nova"), Error));
	Error.Reset();
	const FString Save = FDWRuleStateReader::Write(Profile, Library);
	FDWRuleProfile Reloaded;
	if (!FDWRuleStateReader::Read(Save, Library, Reloaded, Error))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("save round trip keeps concrete rolled values"), FDWRuleStateReader::Write(Reloaded, Library), Save);
	if (!Session.SetLoadout(Reloaded.Loadout, Error))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("select empty slot"), Session.Select(3, Error));
	TestEqual(
		TEXT("empty slot cannot cast fallback plan"),
		Session.Cast(Host.CompileNamedBuild(TEXT("frost_lance"), {}, Error), {.6f, 0, .6f}, {1, 0, 0}, 30, 91, Error),
		INDEX_NONE);
	Error.Reset();
	Session.Select(0, Error);
	Session.Character.Life = 200;
	Session.Character.Shield = 0;
	TestTrue(TEXT("cold tonic is applied through the same host"),
			 Session.UseItem(TEXT("frost_tonic"), {.6f, 0, .6f}, {1, 0, 0}, 92, Error));
	TestEqual(TEXT("tonic has independent six/ten second groups"), Host.Grants().Num(), 2);
	FDWCombatEnemy Enemy;
	Enemy.Id = 100;
	Enemy.Health = Enemy.MaxHealth = 10000;
	Enemy.Position = {2, 0, .6};
	Session.Combat.Enemies.Add(Enemy);
	int32 Rows = 0;
	double LowestHealth = 10000;
	for (int32 Frame = 0; Frame < 660; ++Frame)
	{
		Session.TickCharacter(1. / 60, false);
		if (Frame % 45 == 0 && Frame < 180)
			Session.Cast(Session.Combat.Hero.Plan, {.6f, 0, .6f}, {1, 0, 0}, 30, 100 + Frame, Error);
		Host.World.Targets().Reset();
		auto &M = Session.Combat.Enemies[0];
		Host.World.Targets().Add(FVector3f(M.Position), .4f, float(M.Health + M.Shield), 1, M.Id);
		Host.World.Targets().Add({0, 0, .6f}, .45f, float(Session.Character.Life + Session.Character.Shield), 0,
								 Session.TargetId);
		Session.BeforeWorld(1. / 60);
		Host.Step(1.f / 60);
		Session.AfterWorld();
		Rows += Host.World.DamageLog().Num();
		LowestHealth = FMath::Min(LowestHealth, M.Health);
		Host.World.DamageLog().Reset();
		Session.DrainEffects();
	}
	TestTrue(TEXT("session casts deliver real damage"), Rows > 0 && LowestHealth < 10000);
	TestTrue(TEXT("character recovery remains active"), Session.Character.Life > 200);
	const int32 Attacker = Host.World.AddOwner(100, 1);
	Host.World.SetOwnerStatusId(Attacker, 100);
	Session.Combat.Enemies[0].Owner = Attacker;
	const double BeforeHit = Session.Character.Life + Session.Character.Shield;
	const int32 Claw = Host.CompileNamedBuild(TEXT("monster_claw"), {}, Error);
	Host.World.Cast(Claw, Attacker, {.1f, 0, .6f}, {-1, 0, 0}, 9999, 10);
	Host.World.Targets().Reset();
	Host.World.Targets().Add({0, 0, .6f}, .45f, float(BeforeHit), 0, Session.TargetId);
	Host.World.Targets().Add({2, 0, .6f}, .4f, 10000, 1, 100);
	Session.BeforeWorld(1. / 60);
	Host.Step(1.f / 60);
	Session.AfterWorld();
	Host.World.DamageLog().Reset();
	TestTrue(TEXT("monster casts resolve character resources"),
			 Session.Character.Life + Session.Character.Shield < BeforeHit);
	TestEqual(TEXT("all tonic traits expire"), Host.Grants().Num(), 0);
	Session.Select(1, Error);
	if (Session.Combat.Hero.Plan < 0)
	{
		AddError(TEXT("configured summon slot is empty: ") + Error);
		return false;
	}
	Session.Cast(Session.Combat.Hero.Plan, {.6f, 0, .6f}, {1, 0, 0}, 30, 1000, Error);
	Host.World.Targets().Reset();
	Host.World.Targets().Add({2, 0, .6f}, .4f, 10000, 1, 100);
	Session.BeforeWorld(1. / 60);
	Host.Step(1.f / 60);
	Session.AfterWorld();
	TestTrue(TEXT("summon gem is present in compiled plan"),
			 Host.World.GetPlan(Session.Combat.Hero.Plan).TraitIds.Contains(TEXT("inc_fire")));
	TestTrue(TEXT("summons participate in native runtime"), Session.Combat.Minions->Alive().Num() > 0);
	Session.Combat.Ailments->ApplyPoison(100, {30});
	Session.Forget(100);
	TestEqual(TEXT("recycling target clears layered ailments"), Session.Combat.Ailments->Layers(100, TEXT("poison")),
			  0);
	Session.Character.TakeHit({1.e9, EElement::Physical});
	TestTrue(TEXT("native lethal hit"), Session.Character.bDead);
	Session.Respawn();
	TestFalse(TEXT("native respawn clears death"), Session.Character.bDead);
	TestEqual(TEXT("respawn restores life"), Session.Character.Life, Session.Character.MaxLife);
	FDWTraitEntry Channel;
	Channel.Id = TEXT("trig_channel");
	Channel.Params.Add(TEXT("seconds"), .1);
	FDWTraitEntry Mounted;
	Mounted.Id = TEXT("create_flame_burst");
	Channel.With.Add(Mounted);
	Host.GrantTraits(Session.TargetId, {Channel}, 2, TEXT("channel test"), Error);
	Session.Combat.Triggers->bCasting = true;
	const int32 BeforeTriggers = Session.Combat.Triggers->Fired;
	for (int32 I = 0; I < 20; ++I)
	{
		Session.TickCharacter(1. / 60, false);
		Host.World.Targets().Reset();
		Session.BeforeWorld(1. / 60);
		Host.Step(1.f / 60);
		Session.AfterWorld();
		Host.World.DamageLog().Reset();
		Session.DrainEffects();
	}
	TestTrue(TEXT("held casting drives channel triggers"), Session.Combat.Triggers->Fired > BeforeTriggers);
	Session.Combat.Triggers->bCasting = false;
	const int32 ReleasedTriggers = Session.Combat.Triggers->Fired;
	for (int32 I = 0; I < 20; ++I)
	{
		Session.TickCharacter(1. / 60, false);
		Host.World.Targets().Reset();
		Session.BeforeWorld(1. / 60);
		Host.Step(1.f / 60);
		Session.AfterWorld();
		Host.World.DamageLog().Reset();
		Session.DrainEffects();
	}
	TestEqual(TEXT("released casting stops channel accumulation"), Session.Combat.Triggers->Fired, ReleasedTriggers);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWGeometryTest, "DWRules.Migration.Geometry",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWGeometryTest::RunTest(const FString &Parameters)
{
	FDWRuleHost Host;
	FDWRuleLibrary Library;
	FString Error;
	if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir() / TEXT("Rules"), Library, Error) ||
		!Host.SetLibrary(Library, Error))
	{
		AddError(Error);
		return false;
	}
	const int32 Owner = Host.World.AddOwner(1, 0), Plan = Host.CompileNamedBuild(TEXT("frost_lance"), {}, Error);
	Host.World.SweepGeometry = [](const FVector3f &Start, const FVector3f &End, float, float &T) {
		if (Start.X < 1 && End.X >= 1)
		{
			T = (1 - Start.X) / (End.X - Start.X);
			return true;
		}
		return false;
	};
	Host.World.Cast(Plan, Owner, {0, 0, 0}, {1, 0, 0}, 9, 30);
	Host.World.Targets().Add({2, 0, 0}, .4f, 1000, 1, 100);
	for (int32 I = 0; I < 120; ++I)
		Host.Step(1.f / 60);
	TestEqual(TEXT("target behind wall receives no projectile hit"), Host.World.DamageLog().Num(), 0);
	TestEqual(TEXT("wall ends carrier"), Host.World.Projectiles().Num, 0);
	Host.World.RetireOwner(Owner);
	Host.Step(1.f / 60);
	TestEqual(TEXT("retired owner cannot cast while unused"),
			  Host.World.Cast(Plan, Owner, {0, 0, 0}, {1, 0, 0}, 10, 30), INDEX_NONE);
	TestEqual(TEXT("inactive owner slot is reused after roots finish"), Host.World.AddOwner(9, 1), Owner);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWTargetRefreshTest, "DWRules.Migration.TargetRefresh",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWTargetRefreshTest::RunTest(const FString &Parameters)
{
	FDWRuleHost Host;
	FDWRuleLibrary Library;
	FString Error;
	if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir() / TEXT("Rules"), Library, Error) ||
		!Host.SetLibrary(Library, Error))
	{
		AddError(Error);
		return false;
	}
	const int32 Owner = Host.World.AddOwner(1, 0), Plan = Host.CompileNamedBuild(TEXT("monster_claw"), {}, Error);
	for (int32 I = 0; I < 3; ++I)
		Host.World.Targets().Add({float(I) * 2, 0, 0}, .4f, 100, 1, 100 + I);
	Host.Step(1.f / 60);
	Host.World.Targets().Reset();
	Host.World.Targets().Add({1000, 0, 0}, .4f, 100, 1, 999);
	Host.World.Cast(Plan, Owner, {1000, 0, 0}, {1, 0, 0}, 1, 10);
	Host.Step(0);
	TestEqual(TEXT("queued area uses current smaller target export"), Host.World.DamageLog().Num(), 1);
	if (Host.World.DamageLog().Num())
		TestEqual(TEXT("new target receives hit"), Host.World.DamageLog()[0].TargetExternalId, 999u);
	Host.World.DamageLog().Reset();
	Host.World.Targets().Reset();
	Host.World.Cast(Plan, Owner, {1000, 0, 0}, {1, 0, 0}, 2, 10);
	Host.Step(0);
	TestEqual(TEXT("removing all targets cannot leave stale grid indices"), Host.World.DamageLog().Num(), 0);
	return true;
}
#endif
