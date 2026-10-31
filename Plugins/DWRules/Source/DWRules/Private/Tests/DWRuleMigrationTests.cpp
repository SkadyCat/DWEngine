#include "DWRuleCompiler.h"
#include "DWRuleHost.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleLoadout.h"
#include "DWRuleProgramReader.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;

namespace
{
bool Load(FAutomationTestBase &Test, FDWRuleHost &Host)
{
	FDWRuleLibrary Library;
	FString Error;
	const bool Loaded =
		FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir() / TEXT("Rules"), Library, Error);
	Test.TestTrue(TEXT("whole native library loads: ") + Error, Loaded);
	return Loaded && Host.SetLibrary(MoveTemp(Library), Error);
}
FDWBuildSource GrantBuild(const FDWTraitSource &Payload, bool Stitched)
{
	FDWTraitSource Wrapper;
	Wrapper.Id = TEXT("grant_test");
	FDWRuleSource Rule;
	Rule.Id = TEXT("grant_on_cast");
	Rule.Event = TEXT("cast");
	FDWActionSource Action;
	Action.Op = EOp::GrantTraits;
	Action.Origin = EOrigin::Owner;
	Action.bHasGrant = true;
	Action.bGrantStitched = Stitched;
	Action.Set(TEXT("duration"), 0.2);
	if (Stitched)
		Wrapper.Stitched.Add(Payload);
	else
	{
		FDWTraitEntry Entry;
		Entry.Id = Payload.Id;
		Entry.Params = Payload.Params;
		Action.GrantEntries.Add(Entry);
	}
	Rule.Actions.Add(Action);
	Wrapper.Rules.Add(Rule);
	FDWBuildSource Source;
	Source.ActiveId = Wrapper.Id;
	Source.Traits.Add(Wrapper);
	return Source;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWMigrationLibrary, "DWRules.Migration.Library",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWMigrationLibrary::RunTest(const FString &Parameters)
{
	FDWRuleHost Host;
	if (!Load(*this, Host))
		return false;
	int32 FileCount = 0;
	for (const TCHAR *Folder : {TEXT("Traits"), TEXT("WebTraits"), TEXT("Timed"), TEXT("Affixes")})
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *(FPaths::ProjectContentDir() / TEXT("Rules") / Folder),
											   TEXT("*.json"), true, false);
		FileCount += Files.Num();
	}
	TestEqual(TEXT("every trait file loaded, including sheet and timed"), Host.Library.Traits.Num(), FileCount);
	TestEqual(TEXT("same sheet stat contract"), SheetStatCatalog().Num(), 654);
	TestEqual(TEXT("same trigger parameter contract"), SheetTriggerCatalog().Num(), 147);
	TestTrue(TEXT("items and equipment loaded"), Host.Library.Equipment.Num() > 0 && Host.Library.Items.Num() > 0);
	FString Error;
	for (const auto &Pair : Host.Library.Builds)
		TestTrue(TEXT("compile build ") + Pair.Key.ToString(), Host.CompileNamedBuild(Pair.Key, {}, Error) >= 0);
	const FDWRuleLibrary Previous = Host.Library;
	FDWRuleLibrary Bad = Previous;
	Bad.Traits.Remove(TEXT("create_ice_lance"));
	TestFalse(TEXT("reject invalid replacement"), Host.SetLibrary(MoveTemp(Bad), Error));
	TestEqual(TEXT("failed replacement is atomic"), Host.Library.Traits.Num(), Previous.Traits.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWMigrationGrants, "DWRules.Migration.Grants",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWMigrationGrants::RunTest(const FString &Parameters)
{
	FDWRuleHost Host;
	if (!Load(*this, Host))
		return false;
	FString Error;
	TArray<FDWTraitSource> Held;
	const int32 Owner = Host.World.AddOwner(77, 0);
	FDWTraitSource Payload = Host.Library.Traits[TEXT("frost_damage")].Source;
	Payload.Params.Add(TEXT("amount"), 23.0);
	FDWBuildSource Source = GrantBuild(Payload, false);
	const int32 Plan = Host.CompileSource(Source, Error);
	TestTrue(TEXT("fixed grant compiles: ") + Error, Plan >= 0);
	if (Plan < 0)
		return false;
	Host.World.Cast(Plan, Owner, FVector3f::ZeroVector, FVector3f(1, 0, 0), 7);
	Host.Step(1.f / 60);
	Host.HeldSources(77, Held);
	TestEqual(TEXT("fixed grant applied"), Held.Num(), 1);
	if (!Held.IsEmpty())
		TestEqual(TEXT("parameter override preserved"), Held[0].Params[TEXT("amount")], 23.0);
	Host.World.Cast(Plan, Owner, FVector3f::ZeroVector, FVector3f(1, 0, 0), 8);
	Host.Step(1.f / 60);
	TestEqual(TEXT("same rule grant refreshes without stacking"), Host.Grants().Num(), 1);
	const int32 Base =
		Host.CompileTraits(TEXT("create_ice_lance"), {TEXT("create_ice_lance"), TEXT("frost_damage")}, Error);
	const int32 Effective = Host.PlanWithGrantedTraits(Base, Owner, Error);
	TestTrue(TEXT("timed copy replaces base parameter"),
			 Effective >= 0 && Host.PlanSource(Effective)->Traits.Last().Params[TEXT("amount")] == 23.0);
	for (int32 I = 0; I < 20; ++I)
		Host.Step(1.f / 60);
	Host.HeldSources(77, Held);
	TestEqual(TEXT("expires and clears held sources"), Held.Num(), 0);
	TestEqual(TEXT("base plan restored"), Host.PlanWithGrantedTraits(Base, Owner, Error), Base);
	Source = GrantBuild(Payload, true);
	const int32 Stitched = Host.CompileSource(Source, Error);
	TestTrue(TEXT("stitched plan differs from fixed payload"), Stitched != Plan);
	Host.World.Cast(Stitched, Owner, FVector3f::ZeroVector, FVector3f(1, 0, 0), 9);
	Host.Step(1.f / 60);
	Host.HeldSources(77, Held);
	TestEqual(TEXT("stitched payload executed"), Held.Num(), 1);
	Host.ForgetTarget(77);
	Host.HeldSources(77, Held);
	TestEqual(TEXT("recycled id cannot retain grants"), Held.Num(), 0);
	Source.Traits[0].Stitched[0].Params[TEXT("amount")] = 99.0;
	TestTrue(TEXT("payload parameters affect plan identity"), Host.CompileSource(Source, Error) != Stitched);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWMigrationComposition, "DWRules.Migration.Composition",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWMigrationComposition::RunTest(const FString &Parameters)
{
	FDWRuleHost Host;
	if (!Load(*this, Host))
		return false;
	FString Error;
	FDWCollection Collection;
	const FString Text = TEXT(
		R"({"id":"test_item","name":"test","category":"gem","traits":[{"id":"timed_on_kill","params":{"duration":2},"with":[{"id":"frost_damage","params":{"amount":17}}]},"multiple_projectiles",{"id":"chill","params":{"slow":45}}]})");
	TestTrue(TEXT("read collection: ") + Error,
			 FDWRuleLibraryLoader::ReadCollection(Text, ECollectionKind::Item, Collection, Error));
	FDWCollectionPart Part;
	Part.Collection = Collection;
	FDWBuildSource Source;
	TArray<FDWContribution> Rows;
	TestTrue(TEXT("aggregate collection: ") + Error,
			 Host.Library.Aggregate(TEXT("frost_lance"), {Part, Part}, 0, Source, Rows, Error));
	TestTrue(TEXT("stat traits stack"),
			 Source.Traits.ContainsByPredicate([](const auto &T) { return T.Id == TEXT("multiple_projectiles~2"); }));
	TestTrue(TEXT("stitched wrappers remain independent"),
			 Source.Traits.ContainsByPredicate([](const auto &T) { return T.Id == TEXT("timed_on_kill~2"); }));
	const auto *Chill = Source.Traits.FindByPredicate([](const auto &T) { return T.Id == TEXT("chill"); });
	TestTrue(TEXT("rule parameter override merges"), Chill && Chill->Params[TEXT("slow")] == 45.0);
	TestTrue(TEXT("aggregate compiles"), Host.CompileSource(Source, Error) >= 0);
	FDWSheet Sheet;
	TArray<FDWHeldTrait> Entries;
	for (const auto &Row : Rows)
		if (!Row.bMerged && !Row.bExpired)
			Entries.Add(Row.Held);
	TestTrue(TEXT("sheet calculation accepts composed contributions"), Sheet.Compute(Entries, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWLoadoutMigrationTest, "DWRules.Migration.Loadout",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWLoadoutMigrationTest::RunTest(const FString &Parameters)
{
	FDWRuleHost Host;
	if (!Load(*this, Host))
		return false;
	FString Error;
	FDWRuleLoadout L;
	TestEqual(TEXT("four stable empty skill slots"), L.Skills.Num(), 4);
	TestTrue(TEXT("all slots start empty"), !L.Skills.ContainsByPredicate([](FName Id) { return !Id.IsNone(); }));
	L.Owned.Add(TEXT("frost_lance"), 1);
	L.Owned.Add(TEXT("sentinel"), 1);
	FDWCollection Gem;Gem.Id=TEXT("gem_sentinel_fire_nova");Gem.Category=TEXT("gem");FDWTraitEntry Payload;Payload.Id=TEXT("inc_fire");Gem.Traits.Add(Payload);Host.Library.Items.Add(Gem.Id,Gem);
    L.Owned.Add(Gem.Id,1);
	TestTrue(TEXT("select slot four"), L.ChangeSkill(Host.Library, 3, TEXT("sentinel"), Error));
	TestTrue(TEXT("move skill to slot two"), L.ChangeSkill(Host.Library, 1, TEXT("sentinel"), Error));
	TestTrue(TEXT("old skill slot stays empty"), L.Skills[3].IsNone());
	TestFalse(TEXT("unowned skill rejected"), L.ChangeSkill(Host.Library, 0, TEXT("flame_burst"), Error));
	TestTrue(TEXT("socket sixth gem position"),
			 L.ChangeGem(Host.Library, TEXT("sentinel"), 5, TEXT("gem_sentinel_fire_nova"), Error));
	if(!TestTrue(TEXT("gem reservation exists"),L.Gems.Num()>0))return false;
    TestEqual(TEXT("six stable gem slots"), L.Gems[0].Slots.Num(), 6);
	TestFalse(TEXT("same owned copy cannot be reserved twice"),
			  L.ChangeGem(Host.Library, TEXT("frost_lance"), 0, TEXT("gem_sentinel_fire_nova"), Error));
	FDWBuildSource Source;
	TArray<FDWContribution> Contributions;
	TestTrue(TEXT("socket contributes to native plan"), L.Compose(Host.Library, 1, Source, Contributions, Error));
	TestTrue(TEXT("gem's constituent trait is present"),
			 Source.Traits.Num() > Host.Library.Builds[TEXT("sentinel")].Traits.Num());
	TestTrue(TEXT("socketed build compiles"), Host.CompileSource(Source, Error) >= 0);
	TestTrue(TEXT("unsocket releases reservation"), L.ChangeGem(Host.Library, TEXT("sentinel"), 5, NAME_None, Error));
	TestEqual(TEXT("gem becomes available"), L.AvailableGems(TEXT("gem_sentinel_fire_nova")), 1);
	FDWCollection Grant;Grant.Id=TEXT("glacier_heart");Grant.Slot=TEXT("chest");Grant.Skills.Add(TEXT("frost_lance"));Host.Library.Equipment.Add(Grant.Id,Grant);L.Equipment.Add(Grant.Slot,Grant.Id);
	L.Owned.Remove(TEXT("frost_lance"));
	TestTrue(TEXT("equipment-granted skills are selectable"),
			 L.ChangeSkill(Host.Library, 2, TEXT("frost_lance"), Error));
	TestTrue(TEXT("native trait families loaded"), Host.Library.Tags.TraitFamilies.Num() == Host.Library.Traits.Num());
	TestTrue(TEXT("display tag resolves to a family"), Host.Library.Tags.CollectionFamilies({TEXT("冰霜")}).Num() > 0);
	return true;
}
#endif
