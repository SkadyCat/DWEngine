#include "DWRuleWorldSubsystem.h"
#include "DWRuleStateReader.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleEquipmentBasesTest, "DWRules.Equipment.Bases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWRuleEquipmentBasesTest::RunTest(const FString& Parameters)
{
	using namespace DWRules;
	auto* World = NewObject<UWorld>();
	auto* Rules = NewObject<UDWRuleWorldSubsystem>(World);
	Rules->Configure(nullptr);
	FString Error;
	if (!TestTrue(TEXT("load authored base types"), Rules->LoadRuleLibrary(TEXT("Rules"), Error))) { AddError(Error); return false; }
	const auto& Library = Rules->Library();
	FDWEquipmentRandom Random(42);
	int32 Typed = 0;
	for (const auto& Pair : Library.Equipment)
	{
		const auto& Template = Pair.Value;
		if (!Template.Bases.Num()) continue;
		++Typed;
		for (FName Rarity : {FName(TEXT("common")), FName(TEXT("magic")), FName(TEXT("rare")), FName(TEXT("legendary"))})
		{
			FDWEquipmentInstance Item;
			if (!TestTrue(TEXT("roll typed equipment"), FDWEquipmentRules::Roll(Library, Template, Rarity, [&]() { return Random.Next(); }, TEXT("bases_native"), Item, Error, 5))) { AddError(Error); return false; }
			TestEqual(TEXT("authored legendary bases or two ordinary bases"), Item.Bases.Num(), Template.Rarity == TEXT("legendary") ? Template.Bases.Num() : 2);
			TestEqual(TEXT("first base matches authored type"), Item.Bases[0].Traits[0].Id, Template.Bases[0].Id);
			TestTrue(TEXT("base rank is bounded by equipment T"), Item.Bases[0].Tier == 0 || (Item.Bases[0].Tier >= 1 && Item.Bases[0].Tier <= Item.Tier));
			const auto Before = Item.Bases;
			if (Item.Affixes.Num())
			{
				FDWEquipmentInstance Changed;
				TestTrue(TEXT("reroll quality affixes"), FDWEquipmentRules::Reroll(Library, Item, [&]() { return Random.Next(); }, Changed, Error));
				TestEqual(TEXT("reroll preserves base count"), Changed.Bases.Num(), Before.Num());
				for (int32 I=0; I<Before.Num(); ++I) TestEqual(TEXT("reroll preserves fixed base"), Changed.Bases[I].Id, Before[I].Id);
			}
		}
	}
	TestEqual(TEXT("current weapon and defensive bases"), Typed, 66);
	{
		const auto& Template = Library.Equipment.FindChecked(TEXT("embers_echo_5_weapon"));
		FDWEquipmentInstance Item;
		if (!TestTrue(TEXT("roll authored T10 legendary"), FDWEquipmentRules::Roll(Library, Template, TEXT("legendary"), [&]() { return Random.Next(); }, TEXT("embers_fixed_native"), Item, Error, 10))) { AddError(Error); return false; }
		TestTrue(TEXT("authored legendary is present"), Item.Legendary.IsSet());
		if (!Item.Legendary.IsSet()) return false;
		TestEqual(TEXT("low recovery threshold keeps its authored tier on T10 gear"), Item.Legendary.GetValue().Traits[0].Tier, 1);
		TestEqual(TEXT("low recovery threshold keeps its authored value"), Item.Legendary.GetValue().Traits[0].Params.FindRef(TEXT("per")), 30.0);
		TestEqual(TEXT("mounted skill keeps its authored tier"), Item.Legendary.GetValue().Traits[0].With[0].Tier, 1);
		TestEqual(TEXT("five ordinary random lines"), Item.Affixes.Num(), 5);
		for (int32 I=0; I<Template.Bases.Num(); ++I)
		{
			TestEqual(TEXT("fixed bases keep their authored tier"), Item.Bases[I].Traits[0].Tier, Template.Bases[I].Tier);
			for (const auto& Param : Template.Bases[I].Params) TestEqual(TEXT("fixed bases keep their authored values"), Item.Bases[I].Traits[0].Params.FindRef(Param.Key), Param.Value);
		}
	}
	Rules->RegisterOwner(1, 0);
	if (!TestTrue(TEXT("enable live player"), Rules->EnablePlayer(0, Error)) || !TestTrue(TEXT("starter uses ordinary base generation"), Rules->StartPlayerProgression(Error))) return false;
	Rules->RespawnPlayer();
	TestTrue(TEXT("weapon flat frost enters actual offense"), Rules->PlayerSession()->Character.Offense().Flat[uint8(EElement::Frost)] > 0);
	auto Profile = Rules->PlayerInventory();
	for (const TCHAR* Id : {TEXT("bloodthirst_gauntlets"), TEXT("warden_boots"), TEXT("cinder_guard_chest")})
	{
		FDWEquipmentInstance Item;
		if (!FDWEquipmentRules::Roll(Library, Library.Equipment.FindChecked(Id), TEXT("common"), [&]() { return Random.Next(); }, FString(Id) + TEXT("_native"), Item, Error, 1)) { AddError(Error); return false; }
		Profile.Loadout.Instances.Add(Item); Profile.Loadout.Equipment.Add(Item.Slot, FName(*Item.Uid));
	}
	if (!TestTrue(TEXT("import profile with concrete bases"), Rules->ImportPlayerState(FDWRuleStateReader::Write(Profile, Library), Error))) return false;
	const auto& C = Rules->PlayerSession()->Character;
	TestTrue(TEXT("base armour participates in combat"), C.ArmourValue() > 0);
	TestTrue(TEXT("base evasion participates in combat"), C.EvasionValue() > 0);
	TestTrue(TEXT("base shield participates in combat"), C.MaxShield > 0);
	for (auto& Item : Profile.Loadout.Instances) Item.Bases.Reset();
	if (!TestTrue(TEXT("older inventory gains bases without rerolling quality affixes"), Rules->ImportPlayerState(FDWRuleStateReader::Write(Profile, Library), Error))) return false;
	for (const auto& Item : Rules->PlayerInventory().Loadout.Instances) TestEqual(TEXT("upgraded bases persist in profile"), Item.Bases.Num(), 2);
	return true;
}
#endif
