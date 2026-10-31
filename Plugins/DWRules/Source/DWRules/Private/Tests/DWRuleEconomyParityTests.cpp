#include "DWRuleEquipment.h"
#include "DWRuleHost.h"
#include "DWRuleLibraryLoader.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
namespace
{
using FEconomyObject = TSharedPtr<FJsonObject>;
using FEconomyValue = TSharedPtr<FJsonValue>;
FString EconomyName(FName Name)
{
	return Name.IsNone() ? FString() : Name.ToString().ToLower();
}
FEconomyValue EconomyEntry(const FDWTraitEntry &Entry)
{
	auto O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("id"), EconomyName(Entry.Id));
	auto Params = MakeShared<FJsonObject>();
	for (const auto &P : Entry.Params)
		Params->SetNumberField(EconomyName(P.Key), P.Value);
	O->SetObjectField(TEXT("params"), Params);
	O->SetNumberField(TEXT("tier"), Entry.Tier);
	O->SetNumberField(TEXT("duration"), Entry.Duration);
	TArray<FEconomyValue> With;
	for (const auto &Child : Entry.With)
		With.Add(EconomyEntry(Child));
	O->SetArrayField(TEXT("with"), With);
	return MakeShared<FJsonValueObject>(O);
}
FEconomyValue EconomyAffix(const FDWEquipmentAffix &Affix)
{
	auto O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("id"), EconomyName(Affix.Id));
	O->SetStringField(TEXT("name"), Affix.Name);
	O->SetNumberField(TEXT("tier"), Affix.Tier);
	TArray<FEconomyValue> Traits;
	for (const auto &E : Affix.Traits)
		Traits.Add(EconomyEntry(E));
	O->SetArrayField(TEXT("traits"), Traits);
	return MakeShared<FJsonValueObject>(O);
}
FEconomyValue EconomyItem(const FDWEquipmentInstance &Item)
{
	auto O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("uid"), Item.Uid);
	O->SetStringField(TEXT("baseId"), EconomyName(Item.BaseId));
	O->SetStringField(TEXT("name"), Item.Name);
	O->SetStringField(TEXT("slot"), EconomyName(Item.Slot));
	O->SetStringField(TEXT("rarity"), EconomyName(Item.Rarity));
	O->SetStringField(TEXT("skill"), EconomyName(Item.Skill));
	O->SetNumberField(TEXT("level"), Item.Level);
	O->SetNumberField(TEXT("tier"), Item.Tier);
	TArray<FEconomyValue> Affixes;
	for (const auto &A : Item.Affixes)
		Affixes.Add(EconomyAffix(A));
	O->SetArrayField(TEXT("affixes"), Affixes);
	TArray<FEconomyValue> Bases;
	for (const auto& Base : Item.Bases) Bases.Add(EconomyAffix(Base));
	O->SetArrayField(TEXT("bases"), Bases);
	O->SetField(TEXT("legendary"),
				Item.Legendary.IsSet() ? EconomyAffix(Item.Legendary.GetValue()) : MakeShared<FJsonValueNull>());
	return MakeShared<FJsonValueObject>(O);
}
TArray<FEconomyValue> EconomyNames(const TArray<FName> &Names)
{
	TArray<FEconomyValue> Out;
	for (FName Name : Names)
		Out.Add(MakeShared<FJsonValueString>(EconomyName(Name)));
	return Out;
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWEconomyParityTest, "DWRules.Migration.EconomyParity",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWEconomyParityTest::RunTest(const FString &Parameters)
{
	FString Text, Error;
	const FString Folder = FPaths::ProjectSavedDir() / TEXT("RulesMigration");
	if (!FFileHelper::LoadFileToString(Text, *(Folder / TEXT("economy-parity.json"))))
	{
		AddError(TEXT("Run make-native-economy-parity.mjs first"));
		return false;
	}
	FEconomyObject Input;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Input))
	{
		AddError(TEXT("invalid economy fixture"));
		return false;
	}
	FDWRuleHost Host;
	FDWRuleLibrary Library;
	if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir() / TEXT("Rules"), Library, Error) ||
		!Host.SetLibrary(Library, Error))
	{
		AddError(Error);
		return false;
	}
	auto Result = MakeShared<FJsonObject>(), Profiles = MakeShared<FJsonObject>(), Tags = MakeShared<FJsonObject>();
	for (const auto &Pair : Library.Traits)
	{
		FDWTraitValueProfile Profile;
		if (FDWTraitValues::Profile(Library, Pair.Key, Profile))
		{
			auto O = MakeShared<FJsonObject>(), Params = MakeShared<FJsonObject>();
			for (const auto &P : Profile.Params)
			{
				auto V = MakeShared<FJsonObject>();
				V->SetNumberField(TEXT("step"), P.Value.Step);
				TArray<FEconomyValue> Bands;
				for (const auto &Band : P.Value.Bands)
					Bands.Add(MakeShared<FJsonValueArray>(TArray<FEconomyValue>{MakeShared<FJsonValueNumber>(Band.X),
																				MakeShared<FJsonValueNumber>(Band.Y)}));
				V->SetArrayField(TEXT("tiers"), Bands);
				Params->SetObjectField(EconomyName(P.Key), V);
			}
			O->SetObjectField(TEXT("params"), Params);
			Profiles->SetObjectField(EconomyName(Pair.Key), O);
		}
		auto T = MakeShared<FJsonObject>();
		T->SetArrayField(TEXT("tags"), EconomyNames(Library.Tags.TraitTags.FindRef(Pair.Key)));
		T->SetArrayField(TEXT("families"), EconomyNames(Library.Tags.FamiliesOf(Pair.Key)));
		Tags->SetObjectField(EconomyName(Pair.Key), T);
	}
	Result->SetObjectField(TEXT("profiles"), Profiles);
	Result->SetObjectField(TEXT("tags"), Tags);
	TArray<FEconomyValue> Pool;
	for (const auto &A : FDWEquipmentRules::Pool(Library))
		Pool.Add(MakeShared<FJsonValueString>(EconomyName(A.Id)));
	Result->SetArrayField(TEXT("pool"), Pool);
	TArray<FEconomyValue> Rolls;
	for (const auto &Value : Input->GetArrayField(TEXT("inputs")))
	{
		const auto O = Value->AsObject();
		const auto *Template = Library.Equipment.Find(FName(*O->GetStringField(TEXT("template"))));
		if (!Template)
		{
			AddError(TEXT("missing equipment template"));
			return false;
		}
		FDWEquipmentRandom Stream(uint32(O->GetNumberField(TEXT("seed"))));
		auto Random = [&]() { return Stream.Next(); };
		FDWEquipmentInstance Item;
		if (!FDWEquipmentRules::Roll(Library, *Template, FName(*O->GetStringField(TEXT("rarity"))), Random,
									 O->GetStringField(TEXT("uid")), Item, Error,
									 int32(O->GetNumberField(TEXT("tier")))))
		{
			AddError(Error);
			return false;
		}
		auto Output = MakeShared<FJsonObject>();
		Output->SetField(TEXT("item"), EconomyItem(Item));
		if (Item.Affixes.Num())
		{
			FDWEquipmentRandom RerollStream(uint32(O->GetNumberField(TEXT("rerollSeed"))));
			auto RerollRandom = [&]() { return RerollStream.Next(); };
			FDWEquipmentInstance Next;
			if (!FDWEquipmentRules::Reroll(Library, Item, RerollRandom, Next, Error, INDEX_NONE,
										   FName(*O->GetStringField(TEXT("family")))))
			{
				AddError(Error);
				return false;
			}
			TestEqual(TEXT("reroll preserves item identity"), Next.Uid, Item.Uid);
			Output->SetField(TEXT("rerolled"), EconomyItem(Next));
		}
		else
			Output->SetField(TEXT("rerolled"), MakeShared<FJsonValueNull>());
		Rolls.Add(MakeShared<FJsonValueObject>(Output));
	}
	Result->SetArrayField(TEXT("rolls"), Rolls);
	TArray<FEconomyValue> Shop;
	for (const auto &Listing : FDWShopRules::Listings(Library))
	{
		auto O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("key"), Listing.Key);
		O->SetStringField(TEXT("kind"), EconomyName(Listing.Kind));
		O->SetStringField(TEXT("id"), EconomyName(Listing.Id));
		O->SetStringField(TEXT("name"), Listing.Name);
		O->SetStringField(TEXT("description"), Listing.Description);
		O->SetStringField(TEXT("rarity"), EconomyName(Listing.Rarity));
		O->SetNumberField(TEXT("price"), double(Listing.Price));
		O->SetBoolField(TEXT("custom"), Listing.bCustom);
		Shop.Add(MakeShared<FJsonValueObject>(O));
	}
	Result->SetArrayField(TEXT("shop"), Shop);
	for (const auto &Pair : Library.Monsters)
	{
		FDWCompiledMonster Monster;
		if (!FDWMonsterRules::Compile(Host, Pair.Value, Monster, Error))
		{
			AddError(Error);
			return false;
		}
		if (Monster.Skills.Num())
		{
			FDWMonsterCooldowns State;
			TestEqual(TEXT("monster skill priority follows authored order"),
					  FDWMonsterRules::SelectSkill(Monster, State, .1, 1), 0);
			const double Shared = State.Shared;
			FDWMonsterRules::SelectSkill(Monster, State, 10, 1, 1, true);
			TestEqual(TEXT("freeze pauses monster countdowns"), State.Shared, Shared);
		}
	}
	FString Json;
	FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
	if (!FFileHelper::SaveStringToFile(Json, *(Folder / TEXT("economy-native.json")),
									   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddError(TEXT("cannot export native economy results"));
		return false;
	}
	AddInfo(FString::Printf(TEXT("Economy native export: %d equipment rolls, %d monster definitions"), Rolls.Num(),
							Library.Monsters.Num()));
	return true;
}
#endif
