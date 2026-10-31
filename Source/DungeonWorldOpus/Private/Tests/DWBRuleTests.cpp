#include "CoreMinimal.h"
#include "DWRuleCompiler.h"
#include "DWRuleProgramReader.h"
#include "DWRuleWorld.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace DWRules;

namespace
{
	struct FLibrary
	{
		TMap<FName, FDWTraitSource> Traits;
		TMap<FName, FDWRuleBuildDef> Builds;
		FString Error;

		bool Load()
		{
			const FString Root = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"));
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(Root, TEXT("Traits")), TEXT("*.json"), true, false);
			for (const FString& File : Files)
			{
				FString Text;
				FString Name;
				FDWTraitSource Trait;
				if (!FFileHelper::LoadFileToString(Text, *File) || !FDWRuleProgramReader::ReadTraitText(Text, Trait, Name, Error))
				{
					Error = File + TEXT(": ") + Error;
					return false;
				}
				Traits.Add(Trait.Id, MoveTemp(Trait));
			}
			Files.Reset();
			IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(Root, TEXT("Builds")), TEXT("*.json"), true, false);
			for (const FString& File : Files)
			{
				FString Text;
				FDWRuleBuildDef Build;
				if (!FFileHelper::LoadFileToString(Text, *File) || !FDWRuleProgramReader::ReadBuildText(Text, Build, Error))
				{
					Error = File + TEXT(": ") + Error;
					return false;
				}
				Builds.Add(Build.Id, MoveTemp(Build));
			}
			return Traits.Num() > 0;
		}

		TSharedPtr<FDWCompiledPlan> Compile(FName BuildId, const TArray<FName>& Extra)
		{
			const FDWRuleBuildDef* Build = Builds.Find(BuildId);
			if (!Build)
			{
				Error = TEXT("missing build");
				return nullptr;
			}
			FDWBuildSource Source;
			Source.ActiveId = Build->ActiveId;
			TArray<FName> Ids = Build->Traits;
			Ids.Append(Extra);
			for (FName Id : Ids)
			{
				if (const FDWTraitSource* Trait = Traits.Find(Id))
				{
					Source.Traits.Add(*Trait);
				}
			}
			return FDWRuleCompiler::Compile(Source, Error);
		}
	};

	struct FRun
	{
		float Damage = 0.f;
		int32 Kills = 0;
		int32 Chilled = 0;
		int32 Hits = 0;
	};

	// One cast down a lane of 12 targets, 2 m apart, 1.5 s of simulation.
	FRun Simulate(const TSharedPtr<FDWCompiledPlan>& Plan, float Health)
	{
		FDWRuleWorld World;
		const int32 PlanIndex = World.AddPlan(Plan.ToSharedRef());
		const int32 Owner = World.AddOwner(1, 0);
		World.UpdateOwner(Owner, FVector3f::ZeroVector, FVector3f(1.f, 0.f, 0.f));
		TArray<float> HealthById;
		HealthById.Init(Health, 13);
		const uint64 Chill = StatusKey(TEXT("chill"));
		World.Cast(PlanIndex, Owner, FVector3f::ZeroVector, FVector3f(1.f, 0.f, 0.f), 7);
		FRun Run;
		for (int32 Frame = 0; Frame < 90; ++Frame)
		{
			FDWTargets& Targets = World.Targets();
			Targets.Reset();
			for (int32 Index = 1; Index <= 12; ++Index)
			{
				if (HealthById[Index] > 0.f)
				{
					const float Lateral = (Index % 3 - 1) * 0.6f;
					Targets.Add(FVector3f(2.f * Index, Lateral, 0.f), 0.45f, HealthById[Index], 1, uint32(Index));
				}
			}
			World.Step(1.f / 60.f);
			for (const FDWDamageRecord& Record : World.DamageLog())
			{
				HealthById[Record.TargetExternalId] -= Record.Amount;
				Run.Damage += Record.Amount;
				Run.Kills += Record.bKilled ? 1 : 0;
				++Run.Hits;
			}
			World.DamageLog().Reset();
		}
		for (int32 Index = 1; Index <= 12; ++Index)
		{
			Run.Chilled += World.GetStatusRemaining(uint32(Index), Chill) > 0.f ? 1 : 0;
		}
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWBFrostLanceTest, "DWB.Rules.FrostLanceIsCreatePlusChill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDWBFrostLanceTest::RunTest(const FString& Parameters)
{
	FLibrary Library;
	if (!TestTrue(TEXT("library loads: ") + Library.Error, Library.Load()))
	{
		return false;
	}
	const TSharedPtr<FDWCompiledPlan> Plan = Library.Compile(TEXT("frost_lance"), {});
	if (!TestTrue(TEXT("frost_lance compiles: ") + Library.Error, Plan.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("composed of create_ice_lance + chill"), Plan->TraitIds.Contains(FName(TEXT("create_ice_lance"))) && Plan->TraitIds.Contains(FName(TEXT("chill"))));
	const FRun Run = Simulate(Plan, 1000.f);
	TestEqual(TEXT("one lance hits the first target only"), Run.Hits, 1);
	TestEqual(TEXT("the hit target is chilled"), Run.Chilled, 1);

	// Without the chill trait the same lance applies no status.
	FDWBuildSource Bare;
	Bare.ActiveId = TEXT("create_ice_lance");
	Bare.Traits.Add(Library.Traits[TEXT("create_ice_lance")]);
	FString Error;
	const TSharedPtr<FDWCompiledPlan> BarePlan = FDWRuleCompiler::Compile(Bare, Error);
	TestEqual(TEXT("create_ice_lance alone does not chill"), Simulate(BarePlan, 1000.f).Chilled, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWBTraitStackTest, "DWB.Rules.TraitsStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDWBTraitStackTest::RunTest(const FString& Parameters)
{
	FLibrary Library;
	if (!TestTrue(TEXT("library loads: ") + Library.Error, Library.Load()))
	{
		return false;
	}
	const TArray<FName> All = { TEXT("multiple_projectiles"), TEXT("pierce"), TEXT("ricochet"), TEXT("brittle"), TEXT("shatter"), TEXT("cold_mastery") };
	const FRun Base = Simulate(Library.Compile(TEXT("frost_lance"), {}), 60.f);
	const TSharedPtr<FDWCompiledPlan> Stacked = Library.Compile(TEXT("frost_lance"), All);
	if (!TestTrue(TEXT("stacked build compiles: ") + Library.Error, Stacked.IsValid()))
	{
		return false;
	}
	const FRun Full = Simulate(Stacked, 60.f);
	AddInfo(FString::Printf(TEXT("frost_lance: damage=%.1f hits=%d kills=%d chilled=%d | all traits: damage=%.1f hits=%d kills=%d chilled=%d (x%.1f)"),
		Base.Damage, Base.Hits, Base.Kills, Base.Chilled, Full.Damage, Full.Hits, Full.Kills, Full.Chilled, Full.Damage / FMath::Max(1.f, Base.Damage)));
	TestTrue(TEXT("stacked traits multiply damage at least 5x"), Full.Damage > Base.Damage * 5.f);
	TestTrue(TEXT("stacked traits kill"), Full.Kills > 0);
	TestTrue(TEXT("stacked traits chill more targets than the bare skill"), Full.Chilled > Base.Chilled);
	TestTrue(TEXT("stacked traits hit many more times"), Full.Hits >= Base.Hits * 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWBMonsterTowerLevelTest, "DWB.Rules.MonsterTowerLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWBMonsterTowerLevelTest::RunTest(const FString& Parameters)
{
	FLibrary Library;
	if (!TestTrue(TEXT("library loads: ") + Library.Error, Library.Load())) return false;
	const FDWTraitSource* Tower = Library.Traits.Find(TEXT("monster_tower_level"));
	const FDWTraitSource* Claw = Library.Traits.Find(TEXT("monster_claw"));
	if (!TestNotNull(TEXT("authored tower trait"), Tower) || !TestNotNull(TEXT("authored claw"), Claw)) return false;
	for (int32 Level : { 1, 2, 5 })
	{
		FDWBuildSource Source;
		Source.ActiveId = Claw->Id;
		Source.Traits.Add(*Claw);
		FDWTraitSource Growth = *Tower;
		Growth.Params.Add(TEXT("level"), double(Level));
		FDWModifierSource NoCrit;
		NoCrit.Stat = TEXT("owner.critical_chance"); NoCrit.Op = EModOp::Set; NoCrit.Value = FDWExprSource::Number(0);
		Growth.Modifiers.Add(NoCrit);
		Source.Traits.Add(Growth);
		FString Error;
		const TSharedPtr<FDWCompiledPlan> Plan = FDWRuleCompiler::Compile(Source, Error);
		if (!TestTrue(TEXT("tower and claw compile: ") + Error, Plan.IsValid())) return false;
		TestEqual(TEXT("health evaluated against authored base"), Plan->Stats.From(EStat::OwnerMaxLife, 80.f), 80.f * FMath::Pow(2.f, float(Level - 1)));
		const float DamageScale = FMath::Pow(1.2f, float(Level - 1));
		TestTrue(TEXT("damage power folded"), FMath::IsNearlyEqual(Plan->Stats.Get(EStat::OwnerDamageMultiplier), DamageScale, 0.00001f));
		FDWRuleWorld World;
		const int32 PlanIndex = World.AddPlan(Plan.ToSharedRef());
		const int32 Owner = World.AddOwner(2, 1);
		World.Targets().Add(FVector3f(1.f, 0.f, 0.f), 0.4f, 10000.f, 0, 100);
		World.Step(1.f / 60.f);
		World.Cast(PlanIndex, Owner, FVector3f::ZeroVector, FVector3f(1.f, 0.f, 0.f), 7);
		World.Step(1.f / 60.f);
		if (!TestEqual(TEXT("fixed claw actually hit"), World.DamageLog().Num(), 1)) return false;
		TestTrue(TEXT("fixed claw damage grows independently of cast power"), FMath::IsNearlyEqual(World.DamageLog()[0].Amount, 7.f * DamageScale, 0.0001f));
	}
	return true;
}

#endif
