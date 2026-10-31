#include "DWRuleCompiler.h"
#include "DWRuleSamples.h"
#include "DWRuleWorld.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace DWRules;

namespace
{
	FDWBuildSource Build(std::initializer_list<FDWTraitSource> Traits)
	{
		FDWBuildSource Source;
		Source.ActiveId = TEXT("ice_lance");
		for (const FDWTraitSource& Trait : Traits)
		{
			Source.Traits.Add(Trait);
		}
		return Source;
	}

	TSharedRef<const FDWCompiledPlan> MustCompile(FAutomationTestBase& Test, const FDWBuildSource& Source)
	{
		FString Error;
		TSharedPtr<FDWCompiledPlan> Plan = FDWRuleCompiler::Compile(Source, Error);
		Test.TestTrue(FString::Printf(TEXT("compile: %s"), *Error), Plan.IsValid());
		return Plan.IsValid() ? Plan.ToSharedRef() : MakeShared<FDWCompiledPlan>();
	}

	void LineOfTargets(FDWRuleWorld& World, int32 Count, float Health, float Spacing = 1.5f)
	{
		World.Targets().Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			World.Targets().Add(FVector3f(3.f + Spacing * Index, 0.f, 0.f), 0.4f, Health, 1, uint32(Index + 1));
		}
	}

	float RunCast(FDWRuleWorld& World, int32 Plan, int32 Frames, uint64 Seed = 7)
	{
		const int32 Owner = World.AddOwner(100, 0);
		World.Cast(Plan, Owner, FVector3f::ZeroVector, FVector3f(1.f, 0.f, 0.f), Seed);
		float Total = 0.f;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Step(1.f / 60.f);
			for (const FDWDamageRecord& Record : World.DamageLog())
			{
				Total += Record.Amount;
			}
			World.DamageLog().Reset();
		}
		return Total;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleStatsTest, "DWRules.Core.StatPartitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWRuleStatsTest::RunTest(const FString& Parameters)
{
	FDWStatBlock Stats;
	const uint16 Damage = StatId(EStat::ProjectileDamage);
	Stats.Apply(Damage, EModOp::Set, 10.f);
	Stats.Apply(Damage, EModOp::Add, 5.f);
	Stats.Apply(Damage, EModOp::IncreasedPercent, 50.f);
	Stats.Apply(Damage, EModOp::IncreasedPercent, 50.f);
	Stats.Apply(Damage, EModOp::Multiply, 2.f);
	Stats.Finalize();
	// (10 + 5) * (1 + 100%) * 2: increased adds within its partition, more multiplies.
	TestEqual(TEXT("partitioned damage"), Stats.Get(EStat::ProjectileDamage), 60.f);
	TestEqual(TEXT("untouched default"), Stats.Get(EStat::ProjectileCount), 1.f);
	uint16 Found = 0;
	TestTrue(TEXT("element path"), FindStat(TEXT("element.cold.damage_flat"), Found));
	TestEqual(TEXT("element id"), int32(Found), int32(ElementStatId(uint8(EElement::Cold), EElementStat::DamageFlat)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleCompileTest, "DWRules.Core.CompileComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWRuleCompileTest::RunTest(const FString& Parameters)
{
	FDWTraitSource Orphan = FDWRuleSamples::Pierce(3);
	Orphan.ParentId = TEXT("not_equipped");
	TSharedRef<const FDWCompiledPlan> Plan = MustCompile(*this, Build({ FDWRuleSamples::IceLance(), FDWRuleSamples::Multiple(2), Orphan }));
	TestEqual(TEXT("support stacks count"), Plan->Stats.Get(EStat::ProjectileCount), 3.f);
	TestEqual(TEXT("orphan child is inactive"), Plan->Stats.Get(EStat::ProjectilePierce), 0.f);
	TestTrue(TEXT("cast rules indexed"), Plan->HasRules(uint16(EBuiltinEvent::Cast)));

	FDWTraitSource Bad = FDWRuleSamples::ShatterOnKill();
	Bad.Params.Remove(TEXT("amount"));
	FString Error;
	TestFalse(TEXT("unbound parameter rejected"), FDWRuleCompiler::Compile(Build({ FDWRuleSamples::IceLance(), Bad }), Error).IsValid());
	TestTrue(TEXT("error names the parameter"), Error.Contains(TEXT("amount")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRulePierceTest, "DWRules.Core.EmitAndPierce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWRulePierceTest::RunTest(const FString& Parameters)
{
	FDWRuleWorld World;
	const int32 Plan = World.AddPlan(MustCompile(*this, Build({ FDWRuleSamples::IceLance(), FDWRuleSamples::Pierce(2) })));
	LineOfTargets(World, 5, 1000.f);
	RunCast(World, Plan, 120);
	int32 Damaged = 0;
	for (float Health : World.Targets().Health)
	{
		Damaged += Health < 1000.f ? 1 : 0;
	}
	// Pierce 2: hits three targets in a line, then ends on impact.
	TestEqual(TEXT("targets hit"), Damaged, 3);
	TestEqual(TEXT("carriers released"), World.Projectiles().Num, 0);
	TestEqual(TEXT("root released"), World.ActiveRoots(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleStackTest, "DWRules.Core.StackingMultiplies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWRuleStackTest::RunTest(const FString& Parameters)
{
	auto Measure = [this](const FDWBuildSource& Source)
	{
		FDWRuleWorld World;
		const int32 Plan = World.AddPlan(MustCompile(*this, Source));
		World.Targets().Reset();
		for (int32 Index = 0; Index < 40; ++Index)
		{
			const float Angle = float(Index) * 2.399963f;
			const float Distance = 3.f + 0.25f * float(Index);
			World.Targets().Add(FVector3f(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.f), 0.5f, 60.f, 1, uint32(Index + 1));
		}
		return RunCast(World, Plan, 180);
	};
	const float Single = Measure(Build({ FDWRuleSamples::IceLance() }));
	const float Stacked = Measure(FDWRuleSamples::StackedIceLance());
	AddInfo(FString::Printf(TEXT("single=%.1f stacked=%.1f ratio=%.1fx"), Single, Stacked, Single > 0.f ? Stacked / Single : 0.f));
	TestTrue(TEXT("single trait deals damage"), Single > 0.f);
	TestTrue(TEXT("stacked traits multiply output (>5x)"), Stacked > Single * 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleDeterminismTest, "DWRules.Core.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWRuleDeterminismTest::RunTest(const FString& Parameters)
{
	auto RunOnce = [this](bool bParallel)
	{
		FDWWorldConfig Config;
		Config.bParallel = bParallel;
		Config.ParallelChunk = 16;
		FDWRuleWorld World(Config);
		const int32 Plan = World.AddPlan(MustCompile(*this, FDWRuleSamples::StackedIceLance()));
		LineOfTargets(World, 30, 80.f, 0.7f);
		RunCast(World, Plan, 90, 1234);
		return World.StateHash();
	};
	TestEqual(TEXT("parallel and serial runs match"), RunOnce(true), RunOnce(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleBudgetTest, "DWRules.Core.BudgetDefersInsteadOfFailing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWRuleBudgetTest::RunTest(const FString& Parameters)
{
	FDWWorldConfig Config;
	Config.WorkPerFrame = 50;
	FDWRuleWorld World(Config);
	const int32 Plan = World.AddPlan(MustCompile(*this, FDWRuleSamples::StackedIceLance()));
	LineOfTargets(World, 60, 20.f, 0.5f);
	const int32 Owner = World.AddOwner(1, 0);
	for (int32 Cast = 0; Cast < 40; ++Cast)
	{
		World.Cast(Plan, Owner, FVector3f::ZeroVector, FVector3f(1.f, 0.02f * Cast, 0.f), uint64(Cast));
	}
	int32 MaxActions = 0;
	for (int32 Frame = 0; Frame < 900; ++Frame)
	{
		World.Step(1.f / 60.f);
		MaxActions = FMath::Max(MaxActions, World.GetStats().Actions);
		World.DamageLog().Reset();
	}
	// One event may finish its own actions past the cap; events beyond it are deferred.
	TestTrue(TEXT("per-frame work respects the cap"), MaxActions <= Config.WorkPerFrame + 64);
	TestEqual(TEXT("everything eventually drains"), World.ActiveRoots(), 0);
	return true;
}

#endif
