#include "DWRuleAilments.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleLibraryLoader.h"
#include "DWRulePawnEquipment.h"
#include "DWRulePresetReader.h"
#include "DWRuleSession.h"
#include "DWRuleStateReader.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
namespace
{
bool CurrentLibrary(FAutomationTestBase &T, FDWRuleHost &H)
{
	FDWRuleLibrary L;
	FString Error;
	if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir() / TEXT("Rules"), L, Error) ||
		!H.SetLibrary(MoveTemp(L), Error))
	{
		T.AddError(Error);
		return false;
	}
	return true;
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWCurrentPawn, "DWRules.Migration.CurrentPawnEquipment",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWCurrentPawn::RunTest(const FString &Parameters)
{
	FDWRuleHost H;
	if (!CurrentLibrary(*this, H))
		return false;
	FString Error;
	for (int32 Floor : {1, 2, 3})
	{
		auto D = H.Library.Monsters[TEXT("claw_beast")];
		TestTrue(TEXT("tower overrides its equipment document"),
				 FDWPawnEquipmentRules::SetTower(H.Library, D.Equipment, Floor, Error));
		FDWCompiledMonster M;
		if (!FDWMonsterRules::Compile(H, D, M, Error))
		{
			AddError(Error);
			return false;
		}
		const auto &Stats = H.World.GetPlan(M.DefensePlan).Stats;
		TestTrue(TEXT("life growth is equipment aggregation"),
				 FMath::IsNearlyEqual(double(Stats.From(EStat::OwnerMaxLife, 80)),
									  80 * FMath::Pow(2., Floor - 1), 1.e-4));
		TestEqual(
			TEXT("shared tower remains floor one"),
			H.Library.Equipment[TEXT("monster_tower_equipment")].Traits[0].Params.FindRef(TEXT("level")), 1.);
	}
	auto Bad = H.Library.Monsters[TEXT("claw_beast")];
	FDWTraitEntry E;
	E.Id = TEXT("monster_tower_level");
	Bad.Traits.Add(E);
	FDWCompiledMonster M;
	TestFalse(TEXT("removed direct tower path is rejected"), FDWMonsterRules::Compile(H, Bad, M, Error));
	Error.Reset();
	int32 Gated = 0;
	for (const auto &P : H.Library.Monsters)
		if (P.Value.MinFloor == 11)
			++Gated;
	TestEqual(TEXT("six dangerous lineages start after floor ten"), Gated, 6);
	FDWRuleProfile P;
	TestTrue(TEXT("new starter weapon"), P.StartProgression(H.Library, Error));
	TestEqual(TEXT("starter binding"), P.Loadout.Instances[0].BaseId, FName(TEXT("frostthorn_staff")));
	const auto Parts = P.Loadout.EquipmentParts(H.Library);
	TestTrue(TEXT("player tower fixed slot"),
			 Parts.ContainsByPredicate([](const auto &V) { return V.Collection.Slot == TEXT("tower"); }));
	TestTrue(TEXT("player character fixed slot"),
			 Parts.ContainsByPredicate([](const auto &V) { return V.Collection.Slot == TEXT("character"); }));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWCurrentParabolic, "DWRules.Migration.CurrentParabolic",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWCurrentParabolic::RunTest(const FString &Parameters)
{
	FDWRuleHost H;
	if (!CurrentLibrary(*this, H))
		return false;
	FString Error;
	const int32 Plan = H.CompileTraits(TEXT("throw_poison_flask"), {TEXT("throw_poison_flask")}, Error);
	TestTrue(TEXT("compile generic trajectory fields"), Plan >= 0);
	if (Plan < 0)
		return false;
	const int32 Owner = H.World.AddOwner(1, 0);
	H.World.UpdateOwner(Owner, {0, 0, .6f}, {1, 0, 0});
	H.World.SetOwnerTargetPoint(Owner, {6, 0, 0});
	const int32 Root = H.World.Cast(Plan, Owner, {0, 0, .6f}, {1, 0, 0}, 11, 8, FVector3f(6, 0, 0));
	H.World.SetOwnerTargetPoint(Owner, {0, 9, 0});
	for (int32 I = 0; I < 16; ++I)
		H.Step(1.f / 60);
	const auto &C = H.World.Projectiles();
	TestEqual(TEXT("single generic emit"), C.Num, 1);
	if (!C.Num)
		return false;
	TestTrue(TEXT("cast snapshots original aim"),
			 H.World.GetRoot(Root).TargetPoint.GetValue().Equals({6, 0, 0}));
	TestTrue(TEXT("arc rises above endpoints"), C.Position[0].Z > 1.4f);
	TestTrue(TEXT("target distance determines horizontal speed"),
			 FMath::IsNearlyEqual(C.Position[0].X, 6.f * (16.f / 60) / .55f, .002f));
	TestTrue(TEXT("gravity remains attached to the carrier"),
			 C.Gravity[0] < 0 && (C.Flags[0] & FDWProjectiles::Parabolic));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWCurrentCraft, "DWRules.Migration.CurrentCraftingGrades",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWCurrentCraft::RunTest(const FString &Parameters)
{
	FDWRuleHost H;
	if (!CurrentLibrary(*this, H))
		return false;
	FString Error;
	FDWRuleProfile P;
	if (!P.StartProgression(H.Library, Error))
	{
		AddError(Error);
		return false;
	}
	P.Loadout.Owned.Add(TEXT("quad_affix_orb"), 3);
	TestTrue(
		TEXT("roll four on common equipment"),
		P.Craft(H.Library, TEXT("starter_weapon"), TEXT("quad_affix_orb"), NAME_None, INDEX_NONE, Error));
	P.Loadout.Owned.Add(TEXT("chaos_orb"), 10);
	P.Loadout.Owned.Add(TEXT("spirit_stone_mid"), 10);
	P.Loadout.Owned.Add(TEXT("spirit_stone_high"), 10);
	const auto Unbound = FDWRuleStateReader::Write(P, H.Library);
	TestFalse(TEXT("cannot skip the first binding"), P.Craft(H.Library, TEXT("starter_weapon"), TEXT("spirit_stone_mid"), NAME_None, 0, Error));
	TestEqual(TEXT("rejected grade spends nothing including RNG"), FDWRuleStateReader::Write(P, H.Library), Unbound);
	const FName Grades[] = {TEXT("chaos_orb"), TEXT("spirit_stone_mid"), TEXT("spirit_stone_high")};
	const int32 Indices[] = {3, 0, 2};
	for (int32 I = 0; I < 3; ++I)
	{
		TestTrue(TEXT("bind grades in chosen line order"), P.Craft(H.Library, TEXT("starter_weapon"), Grades[I], NAME_None, Indices[I], Error));
		TestEqual(TEXT("binding records arbitrary selected line"), P.Loadout.Instances[0].ReforgeTargets[I], Indices[I]);
	}
	const auto Bound = FDWRuleStateReader::Write(P, H.Library);
	TestFalse(TEXT("bound currency cannot change line"), P.Craft(H.Library, TEXT("starter_weapon"), TEXT("chaos_orb"), NAME_None, 1, Error));
	TestEqual(TEXT("wrong line preserves state and RNG"), FDWRuleStateReader::Write(P, H.Library), Bound);
	FDWRuleProfile BoundReload;
	TestTrue(TEXT("bindings round trip"), FDWRuleStateReader::Read(Bound, H.Library, BoundReload, Error));
	TestTrue(TEXT("all three binding indices preserved"), BoundReload.Loadout.Instances[0].ReforgeTargets == P.Loadout.Instances[0].ReforgeTargets);
	TestTrue(TEXT("bound line can reroll again"), P.Craft(H.Library, TEXT("starter_weapon"), TEXT("chaos_orb"), NAME_None, 3, Error));
	TestTrue(
		TEXT("replace the four random affixes again"),
		P.Craft(H.Library, TEXT("starter_weapon"), TEXT("quad_affix_orb"), NAME_None, INDEX_NONE, Error));
	TestEqual(TEXT("exactly four fresh affixes"), P.Loadout.Instances[0].Affixes.Num(), 4);
	TestTrue(TEXT("replacement clears old currency bindings"), P.Loadout.Instances[0].ReforgeTargets.IsEmpty());
	TestEqual(TEXT("keeps rarity"), P.Loadout.Instances[0].Rarity, FName(TEXT("common")));
	FDWRuleProfile Reloaded;
	TestTrue(TEXT("crafted item round trips"),
			 FDWRuleStateReader::Read(FDWRuleStateReader::Write(P, H.Library), H.Library, Reloaded, Error));
	const auto Before = FDWRuleStateReader::Write(P, H.Library);
	TestFalse(TEXT("replacement rejects single-line mode"),
			  P.Craft(H.Library, TEXT("starter_weapon"), TEXT("quad_affix_orb"), NAME_None, 0, Error));
	TestEqual(TEXT("failed crafting preserves state and RNG"), FDWRuleStateReader::Write(P, H.Library),
			  Before);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWCurrentPresets, "DWRules.Migration.CurrentBuildPresets",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWCurrentPresets::RunTest(const FString &Parameters)
{
	TArray<FDWRulePreset> Presets;
	FString Error;
	if (!FDWRulePresetReader::Catalogue(Presets, Error))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("current shared BD catalogue"), Presets.Num(), 21);
	FDWRuleHost H;
	if (!CurrentLibrary(*this, H))
		return false;
	FDWRuleProfile Player;
	Player.StartProgression(H.Library, Error);
	TSet<FString> Granted;for(const auto& I:Player.Loadout.Instances)Granted.Add(I.Uid);
    Player.Stones = 321;
	Player.Loadout.Owned.Add(TEXT("chaos_orb"), 9);
	for (const auto &P : Presets)
	{
		FDWRuleProfile Prepared;
		if (!FDWRuleStateReader::ReadPreset(P.Json, H.Library, Prepared, Error) ||
			!Player.ApplyPreset(H.Library, Prepared, Error) ||
			!Player.ApplyPreset(H.Library, Prepared, Error))
		{
			AddError(P.Id.ToString() + TEXT(": ") + Error);
			return false;
		}
		for(const auto& I:Prepared.Loadout.Instances)Granted.Add(I.Uid);
	}
	TestEqual(TEXT("all distinct gear granted only once"), Player.Loadout.Instances.Num(), Granted.Num());
	TestEqual(TEXT("existing currency preserved"), Player.Loadout.Owned.FindRef(TEXT("chaos_orb")), 9);
	TestEqual(TEXT("existing stones preserved"), Player.Stones, int64(321));
	for (const auto &P : Presets)
	{
		FDWRuleHost World;
		if (!CurrentLibrary(*this, World))
			return false;
		FDWRuleProfile Gear;
		if (!FDWRuleStateReader::ReadPreset(P.Json, World.Library, Gear, Error))
			return false;
		const int32 Owner = World.World.AddOwner(1, 0);
		World.World.UpdateOwner(Owner, {0, 0, .6f}, {1, 0, 0});
		World.World.SetOwnerTargetPoint(Owner, {6, 0, 0});
		FDWRuleSession S(World, Owner, 0x40000000);
		if (!S.SetLoadout(Gear.Loadout, Error))
		{
			AddError(P.Id.ToString() + TEXT(": ") + Error);
			return false;
		}
		FDWCombatEnemy M;
		M.Id = 100;
		M.Health = M.MaxHealth = 1.e9;
		M.Position = {6, 0, .6};
		S.Combat.Enemies.Add(M);
		const int32 Startup = P.Id == TEXT("autocast_interval") || P.Id == TEXT("autocast_still") ||
									  P.Id == TEXT("autocast_moving") || P.Id == TEXT("autocast_shield")
								  ? 0
							  : P.Id == TEXT("autocast_mana_restored") ? 2
																	   : 1;
		int32 Casts = 0, Rows = 0, TriggersAtTen = 0;
		double DamageDealt=0;
		for (int32 F = 0; F < 1200; ++F)
		{
			if (F == 600)
				TriggersAtTen = S.Character.Totals.Triggers;
			if (P.Id == TEXT("autocast_moving"))
				World.World.UpdateOwner(
					Owner, {float(FMath::Sin(F / 60.) * .5), float(FMath::Cos(F / 60.) * .5), .6f},
					{1, 0, 0});
			S.TickCharacter(1. / 60, P.Id == TEXT("autocast_moving"));
			if ((Casts < Startup || (!P.Id.ToString().StartsWith(TEXT("autocast_"))&&F%45==0)) && S.Combat.Cooldown <= 0)
				if (S.Cast(S.Combat.Hero.Plan, {.6f, 0, .6f}, {1, 0, 0}, 30, 100 + F, Error) >= 0)
					++Casts;
			World.World.Targets().Reset();
			auto &Enemy = S.Combat.Enemies[0];
			World.World.Targets().Add(FVector3f(Enemy.Position), .45f, float(Enemy.Health), 1, 100);
			World.World.Targets().Add({0, 0, .6f}, .45f, float(S.Character.Life + S.Character.Shield), 0,
									  S.TargetId);
			S.BeforeWorld(1. / 60);
			World.Step(1.f / 60);
			S.AfterWorld();
			Rows += World.World.DamageLog().Num();
			for (const auto& Row:World.World.DamageLog()) if (Row.TargetExternalId==100) DamageDealt+=Row.Amount;
			World.World.DamageLog().Reset();
			S.DrainEffects();
		}
		AddInfo(FString::Printf(TEXT("PresetRuntime id=%s rows=%d manual=%d triggers=%d life=%.2f "
									 "shield=%.2f maxShield=%.2f cap=%.2f restored=%.2f"),
								*P.Id.ToString(), Rows, Casts, S.Character.Totals.Triggers, S.Character.Life,
								S.Character.Shield, S.Character.MaxShield, S.Character.ShieldCap,
								S.Character.Totals.ShieldRecovered));
		TestTrue(P.Id.ToString() + TEXT(" deals damage with manual input released"),
				 Rows > 0 && DamageDealt>0);
		if(P.Id.ToString().StartsWith(TEXT("autocast_")))TestTrue(P.Id.ToString() + TEXT(" continues triggering after ten seconds"),
				 S.Character.Totals.Triggers > TriggersAtTen);
		TestFalse(P.Id.ToString() + TEXT(" stays alive without god mode"), S.Character.bDead);
	}
	return true;
}
#endif
