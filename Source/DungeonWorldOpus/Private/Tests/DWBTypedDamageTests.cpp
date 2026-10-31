#include "CoreMinimal.h"
#include "DWRuleCompiler.h"
#include "DWRuleProgramReader.h"
#include "DWRuleWorld.h"
#include "DWRuleWorldSubsystem.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace DWRules;

namespace
{
	const TCHAR* const DamageNames[] = {
		TEXT("physical"), TEXT("fire"), TEXT("frost"), TEXT("lightning"), TEXT("poison"), TEXT("void")
	};
	const TCHAR* const ResistanceNames[] = { TEXT("fire"), TEXT("frost"), TEXT("lightning"), TEXT("poison") };

	FDWTraitSource LoadTrait(FAutomationTestBase& Test, const FString& Id)
	{
		FDWTraitSource Trait;
		FString Text, Name, Error;
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules/Traits"), Id + TEXT(".json"));
		if (Test.TestTrue(TEXT("read ") + Id, FFileHelper::LoadFileToString(Text, *Path)))
		{
			const bool bRead = FDWRuleProgramReader::ReadTraitText(Text, Trait, Name, Error);
			Test.TestTrue(TEXT("parse ") + Id + TEXT(": ") + Error, bRead);
		}
		return Trait;
	}

	FDWBuildSource Attack(FAutomationTestBase& Test, const FString& Element, double Amount = 100.0)
	{
		FDWBuildSource Source;
		Source.ActiveId = TEXT("create_ice_lance");
		FDWTraitSource Carrier = LoadTrait(Test, TEXT("create_ice_lance"));
		FDWModifierSource NoCrit;
		NoCrit.Stat = TEXT("owner.critical_chance");
		NoCrit.Op = EModOp::Set;
		NoCrit.Value = FDWExprSource::Number(0);
		Carrier.Modifiers.Add(NoCrit);
		Source.Traits.Add(MoveTemp(Carrier));
		FDWTraitSource Damage = LoadTrait(Test, Element + TEXT("_damage"));
		Damage.Params.Add(TEXT("amount"), Amount);
		Source.Traits.Add(MoveTemp(Damage));
		return Source;
	}

	FDWBuildSource Defense(FAutomationTestBase& Test, const FString& Element, double Value = 25.0)
	{
		FDWBuildSource Source;
		FDWTraitSource Trait = LoadTrait(Test, Element + TEXT("_resistance"));
		Trait.Params.Add(TEXT("resistance"), Value);
		Source.ActiveId = Trait.Id;
		Source.Traits.Add(MoveTemp(Trait));
		return Source;
	}

	int32 AddPlan(FAutomationTestBase& Test, FDWRuleWorld& World, const FDWBuildSource& Source)
	{
		FString Error;
		const TSharedPtr<FDWCompiledPlan> Plan = FDWRuleCompiler::Compile(Source, Error);
		return Test.TestTrue(TEXT("compile: ") + Error, Plan.IsValid()) ? World.AddPlan(Plan.ToSharedRef()) : INDEX_NONE;
	}

	void Fire(FDWRuleWorld& World, int32 Plan, int32 Owner)
	{
		World.Cast(Plan, Owner, FVector3f::ZeroVector, FVector3f(1, 0, 0), 17);
		for (int32 Frame = 0; Frame < 20; ++Frame)
		{
			World.Step(1.f / 60.f);
		}
	}

	TArray<FDWDamageRecord> Shoot(FAutomationTestBase& Test, const FDWBuildSource& Offense,
		const FDWBuildSource* Defenses = nullptr, const float* BaseResistance = nullptr)
	{
		FDWRuleWorld World;
		const int32 Plan = AddPlan(Test, World, Offense);
		World.Targets().Add(FVector3f(3, 0, 0), 0.45f, 1000.f, 1, 7, BaseResistance);
		if (Defenses)
		{
			const int32 DefensePlan = AddPlan(Test, World, *Defenses);
			Test.TestTrue(TEXT("bind defense plan"), DefensePlan != INDEX_NONE && World.SetTargetDefensePlan(7, DefensePlan));
		}
		Fire(World, Plan, World.AddOwner(100, 0));
		return World.DamageLog();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWSixDamageTraitsTest, "DWB.Rules.SixDamageTraits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDWSixDamageTraitsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : DamageNames)
	{
		EElement Element;
		if (!TestTrue(FString(TEXT("damage type ")) + Name, ParseElement(Name, Element)))
		{
			continue;
		}
		const TArray<FDWDamageRecord> Hits = Shoot(*this, Attack(*this, Name, 10));
		if (TestEqual(FString(Name) + TEXT(" produces one base hit and one bonus; no recursion"), Hits.Num(), 2))
		{
			TestEqual(FString(Name) + TEXT(" damage type"), Hits.Last().Element, uint8(Element));
			TestEqual(FString(Name) + TEXT(" JSON amount"), Hits.Last().Amount, 10.f);
		}
	}

	FDWBuildSource All = Attack(*this, TEXT("physical"), 10);
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(DamageNames); ++Index)
	{
		All.Traits.Add(LoadTrait(*this, FString(DamageNames[Index]) + TEXT("_damage")));
	}
	const TArray<FDWDamageRecord> Stacked = Shoot(*this, All);
	TestEqual(TEXT("six damage traits coexist without cross-triggering"), Stacked.Num(), 7);
	float Total = 0.f;
	for (const FDWDamageRecord& Hit : Stacked) Total += Hit.Amount;
	TestEqual(TEXT("base 16 plus six JSON bonuses of 10"), Total, 76.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWFourResistanceTraitsTest, "DWB.Rules.FourResistanceTraits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDWFourResistanceTraitsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Resistance : ResistanceNames)
	{
		const FDWBuildSource Defenses = Defense(*this, Resistance);
		for (const TCHAR* Damage : DamageNames)
		{
			const TArray<FDWDamageRecord> Hits = Shoot(*this, Attack(*this, Damage), &Defenses);
			const FString Label = FString(Resistance) + TEXT(" resistance vs ") + Damage;
			if (TestEqual(Label + TEXT(" hit count"), Hits.Num(), 2))
			{
				TestEqual(Label, Hits.Last().Amount, FString(Damage) == Resistance ? 75.f : 100.f);
			}
		}
	}

	uint16 Stat = 0;
	TestFalse(TEXT("physical resistance is not an exposed stat"), FindStat(TEXT("element.physical.resistance"), Stat));
	TestFalse(TEXT("void resistance is not an exposed stat"), FindStat(TEXT("element.void.resistance"), Stat));
	TestTrue(TEXT("legacy cold stat spelling remains valid"), FindStat(TEXT("element.cold.resistance"), Stat));
	TestEqual(TEXT("cold aliases frost resistance"), Stat, ElementStatId(uint8(EElement::Frost), EElementStat::Resistance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWResistancePartitionsTest, "DWB.Rules.ResistancePartitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDWResistancePartitionsTest::RunTest(const FString& Parameters)
{
	const FDWBuildSource Offense = Attack(*this, TEXT("fire"));
	auto Check = [&](const FString& Label, const FDWBuildSource& Attacker, const FDWBuildSource& Defender,
		float Expected, const float* Base = nullptr)
	{
		const TArray<FDWDamageRecord> Hits = Shoot(*this, Attacker, &Defender, Base);
		if (TestEqual(Label + TEXT(" hit count"), Hits.Num(), 2)) TestEqual(Label, Hits.Last().Amount, Expected);
	};
	Check(TEXT("resistance is capped at 75 percent"), Offense, Defense(*this, TEXT("fire"), 150), 25.f);
	Check(TEXT("negative resistance amplifies damage"), Offense, Defense(*this, TEXT("fire"), -25), 125.f);

	FDWBuildSource Stacked = Defense(*this, TEXT("fire"));
	FDWTraitSource Extra = Stacked.Traits[0];
	Extra.Id = TEXT("equipment_fire_resistance");
	Stacked.Traits.Add(Extra);
	Check(TEXT("resistance traits stack across sources"), Offense, Stacked, 50.f);
	const float BaseResistance[NumResistances] = { 10, 0, 0, 0 };
	Check(TEXT("defense modifiers apply to the target base resistance"), Offense, Defense(*this, TEXT("fire")), 65.f, BaseResistance);

	FDWBuildSource Penetrating = Offense;
	FDWModifierSource Modifier;
	Modifier.Stat = TEXT("element.fire.penetration");
	Modifier.Op = EModOp::Add;
	Modifier.Value = FDWExprSource::Number(20);
	Penetrating.Traits[0].Modifiers.Add(Modifier);
	Check(TEXT("penetration reduces positive resistance"), Penetrating, Defense(*this, TEXT("fire")), 95.f);
	Check(TEXT("penetration does not push positive resistance below zero"), Penetrating, Defense(*this, TEXT("fire"), 10), 100.f);
	Check(TEXT("penetration does not amplify existing negative resistance"), Penetrating, Defense(*this, TEXT("fire"), -25), 125.f);

	FDWBuildSource RaisedCap = Defense(*this, TEXT("fire"), 95);
	Modifier.Stat = TEXT("element.fire.max_resistance");
	Modifier.Op = EModOp::Set;
	Modifier.Value = FDWExprSource::Number(90);
	RaisedCap.Traits[0].Modifiers.Add(Modifier);
	Check(TEXT("maximum resistance can be changed by a rule modifier"), Offense, RaisedCap, 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWDefenseBindingTest, "DWB.Rules.DefenseBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDWDefenseBindingTest::RunTest(const FString& Parameters)
{
	FDWRuleWorld World;
	const int32 Offense = AddPlan(*this, World, Attack(*this, TEXT("fire")));
	const int32 Defenses = AddPlan(*this, World, Defense(*this, TEXT("fire")));
	const int32 Owner = World.AddOwner(100, 0);
	TestTrue(TEXT("bind compiled defense traits"), World.SetTargetDefensePlan(7, Defenses));
	TestFalse(TEXT("invalid plan rejected without replacing the binding"), World.SetTargetDefensePlan(7, 99999));
	TestFalse(TEXT("invalid external id rejected"), World.SetTargetDefensePlan(InvalidId, Defenses));
	for (int32 Shot = 0; Shot < 3; ++Shot)
	{
		if (Shot == 2) TestTrue(TEXT("unequip defense traits"), World.SetTargetDefensePlan(7, INDEX_NONE));
		World.Targets().Reset();
		World.Targets().Add(FVector3f(3, 0, 0), 0.45f, 1000.f, 1, 7);
		World.DamageLog().Reset();
		Fire(World, Offense, Owner);
		if (TestEqual(TEXT("binding test hit count"), World.DamageLog().Num(), 2))
		{
			TestEqual(TEXT("binding survives target export and can be removed"), World.DamageLog().Last().Amount, Shot == 2 ? 100.f : 75.f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWDefenseTraitLifecycleTest, "DWB.Rules.DefenseTraitLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDWDefenseTraitLifecycleTest::RunTest(const FString& Parameters)
{
	// Exercise the public JSON binding API without starting an arena or rendering.
	UWorld* Host = NewObject<UWorld>();
	UDWRuleWorldSubsystem* Rules = NewObject<UDWRuleWorldSubsystem>(Host);
	Rules->Configure(nullptr);
	FString Error;
	if (!TestTrue(TEXT("load JSON library"), Rules->LoadRuleLibrary(TEXT("Rules"), Error))) return false;
	const int32 Plan = Rules->CompileSource(Attack(*this, TEXT("fire")), Error);
	if (!TestTrue(TEXT("compile attack through subsystem"), Plan != INDEX_NONE)) return false;
	const int32 Owner = Rules->RegisterOwner(100, 0);
	const int32 Target = Rules->AddStaticTarget(FVector(300, 0, 0), 45, 1000, 1);
	auto CheckDamage = [&](const TCHAR* Label, float Expected)
	{
		FDWRuleWorld& Runtime = Rules->RuleWorld();
		Runtime.Targets().Reset();
		Runtime.Targets().Add(FVector3f(3, 0, 0), 0.45f, 1000.f, 1, uint32(Target));
		Runtime.DamageLog().Reset();
		Fire(Runtime, Plan, Owner);
		if (TestEqual(FString(Label) + TEXT(" hit count"), Runtime.DamageLog().Num(), 2))
		{
			TestEqual(Label, Runtime.DamageLog().Last().Amount, Expected);
		}
	};
	TestTrue(TEXT("equip JSON resistance trait"), Rules->SetTargetDefenseTraits(Target, { TEXT("fire_resistance") }, Error));
	TestFalse(TEXT("unknown trait rejected"), Rules->SetTargetDefenseTraits(Target, { TEXT("missing_trait") }, Error));
	CheckDamage(TEXT("failed replacement preserves existing defense"), 75.f);
	TestTrue(TEXT("unequip via empty list"), Rules->SetTargetDefenseTraits(Target, {}, Error));
	CheckDamage(TEXT("unequipped defense stops reducing damage"), 100.f);
	TestTrue(TEXT("reequip JSON resistance trait"), Rules->SetTargetDefenseTraits(Target, { TEXT("fire_resistance") }, Error));
	Rules->ForgetTarget(uint32(Target));
	CheckDamage(TEXT("recycled Mass/Actor target ids forget defense"), 100.f);
	TestTrue(TEXT("equip static target defense"), Rules->SetTargetDefenseTraits(Target, { TEXT("fire_resistance") }, Error));
	Rules->ClearStaticTargets();
	TestEqual(TEXT("static target id recycled"), Rules->AddStaticTarget(FVector(300, 0, 0), 45, 1000, 1), Target);
	CheckDamage(TEXT("static id recycling clears defense"), 100.f);
	return true;
}

#endif
