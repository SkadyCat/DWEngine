#include "DWRulePresetReader.h"
#include "DWRuleStateReader.h"
#include "DWRuleJson.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace DWRules;
bool FDWRulePresetReader::Catalogue(TArray<FDWRulePreset> &Out, FString &Error)
{
	FString Json;
	const FString Directory = FPaths::ProjectContentDir() / TEXT("Rules/Presets");
	TSharedPtr<FJsonValue> Root;
	if (!FFileHelper::LoadFileToString(Json, *(Directory / TEXT("index.json"))) ||
		!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root ||
		Root->Type != EJson::Array)
	{
		Error = TEXT("invalid preset catalogue");
		return false;
	}
	TArray<FDWRulePreset> Next;
	TSet<FName> Seen;
	for (const auto &V : Root->AsArray())
	{
		const auto O = V->Type == EJson::Object ? V->AsObject() : nullptr;
		if (!DWRulesJson::Fields(O, {TEXT("id"), TEXT("name"), TEXT("description"),
			TEXT("group"), TEXT("groupName"), TEXT("grade"), TEXT("gradeName"), TEXT("mechanism")}, Error))
			return false;
		FDWRulePreset P;
		FString Id;
		if (!DWRulesJson::String(O, TEXT("id"), Id, Error) || !DWRulesJson::Id(Id) ||
			!DWRulesJson::String(O, TEXT("name"), P.Name, Error) ||
			!DWRulesJson::String(O, TEXT("description"), P.Description, Error))
			return false;
		P.Id = FName(*Id);
		if (Seen.Contains(P.Id) ||
			!FFileHelper::LoadFileToString(P.Json, *(Directory / (Id + TEXT(".json")))))
		{
			Error = TEXT("duplicate or missing preset: ") + Id;
			return false;
		}
		Seen.Add(P.Id);
		Next.Add(MoveTemp(P));
	}
	Out = MoveTemp(Next);
	return true;
}
bool FDWRuleStateReader::ReadPreset(const FString &Json, const FDWRuleLibrary &L, FDWRuleProfile &Out,
									FString &Error)
{
	using namespace DWRulesJson;
	using FValue = TSharedPtr<FJsonValue>;
	FObject O;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) ||
		!Fields(O,
				{TEXT("skills"), TEXT("equipment"), TEXT("instances"), TEXT("belt"), TEXT("gems"),
				 TEXT("authored")},
				Error))
		return false;
	auto Root = MakeShared<FJsonObject>(), Owned = MakeShared<FJsonObject>(),
		 Skills = MakeShared<FJsonObject>(), Equipment = MakeShared<FJsonObject>(),
		 Items = MakeShared<FJsonObject>();
	Root->SetObjectField(TEXT("loadout"), O);
	const FObject *Instances = nullptr;
	if (O->TryGetObjectField(TEXT("instances"), Instances))
		Root->SetObjectField(TEXT("instances"), *Instances);
	const bool Rolled = Instances && !(*Instances)->Values.IsEmpty();
	const TArray<FValue> *Names;
	if (!O->TryGetArrayField(TEXT("skills"), Names))
	{
		Error = TEXT("preset needs skill slots");
		return false;
	}
	for (const auto &V : *Names)
		if (!Rolled && V->Type == EJson::String)
			Skills->SetBoolField(V->AsString(), true);
	const FObject *Gear;
	if (!O->TryGetObjectField(TEXT("equipment"), Gear))
	{
		Error = TEXT("preset needs equipment");
		return false;
	}
	for (const auto &P : (*Gear)->Values)
		if (P.Value->Type == EJson::String && (!Rolled || !(*Instances)->HasField(P.Value->AsString())))
			Equipment->SetNumberField(P.Value->AsString(), 1);
	auto RequireItem = [&](const FValue &V) {
		if (V->Type == EJson::String)
		{
			double Count = 0;
			Items->TryGetNumberField(V->AsString(), Count);
			Items->SetNumberField(V->AsString(), Count + 1);
		}
	};
	if (O->TryGetArrayField(TEXT("belt"), Names))
		for (const auto &V : *Names)
			RequireItem(V);
	const FObject *Gems;
	if (O->TryGetObjectField(TEXT("gems"), Gems))
		for (const auto &P : (*Gems)->Values)
			if (P.Value->Type == EJson::Array)
				for (const auto &V : P.Value->AsArray())
					RequireItem(V);
	Owned->SetObjectField(TEXT("skill"), Skills);
	Owned->SetObjectField(TEXT("equipment"), Equipment);
	Owned->SetObjectField(TEXT("item"), Items);
	Root->SetObjectField(TEXT("owned"), Owned);
	Root->SetBoolField(TEXT("initialized"), true);
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	FDWRuleProfile Next;
	if (!Read(Text, L, Next, Error))
		return false;
	const TArray<FValue> *Requested;
	O->TryGetArrayField(TEXT("skills"), Requested);
	for (int32 I = 0; I < Requested->Num(); ++I)
		if ((*Requested)[I]->Type == EJson::String &&
			Next.Loadout.Skills[I] != FName(*(*Requested)[I]->AsString()))
		{
			Error = TEXT("preset skill must be granted by its actual equipment");
			return false;
		}
	if (!Next.Loadout.Skills.ContainsByPredicate([](FName Id) { return !Id.IsNone(); }))
	{
		Error = TEXT("preset must grant an active skill");
		return false;
	}
	for (const auto &I : Next.Loadout.Instances)
	{
		const int32 N = I.Affixes.Num();
		const bool Valid = I.Rarity == TEXT("common")  ? N == 0
						   : I.Rarity == TEXT("magic") ? N >= 1 && N <= 2
						   : I.Rarity == TEXT("rare")
							   ? N >= 3 && N <= 6
							   : I.Rarity == TEXT("legendary") && N == 5 && I.Legendary.IsSet();
		if (!Valid || (I.Rarity != TEXT("legendary") && I.Legendary.IsSet()))
		{
			Error = TEXT("invalid preset rarity or affix count");
			return false;
		}
		TFunction<bool(const FDWTraitEntry &)> Rank;
		Rank = [&](const auto &E) {
			if (E.Tier > I.Tier)
				return false;
			for (const auto &C : E.With)
				if (!Rank(C))
					return false;
			return true;
		};
		for (const auto &A : I.Affixes)
			for (const auto &E : A.Traits)
				if (!Rank(E))
				{
					Error = TEXT("preset trait exceeds equipment tier");
					return false;
				}
	}
	Out = MoveTemp(Next);
	return true;
}
