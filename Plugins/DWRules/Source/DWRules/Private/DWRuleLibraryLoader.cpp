#include "DWRuleLibraryLoader.h"
#include "DWRuleProgramReader.h"
#include "DWRuleTagReader.h"
#include "DWRuleValueReader.h"
#include "DWRuleMonsterReader.h"
#include "DWRuleShopReader.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

using namespace DWRules;

namespace
{
using FObject = TSharedPtr<FJsonObject>;
using FValue = TSharedPtr<FJsonValue>;
bool Closed(const FObject &Obj, std::initializer_list<const TCHAR *> Fields, FString &Error)
{
	for (const auto &P : Obj->Values)
	{
		bool Found = false;
		for (const TCHAR *Field : Fields)
			if (P.Key == Field)
			{
				Found = true;
				break;
			}
		if (!Found)
		{
			Error = FString(TEXT("unknown field ")) + FString(P.Key);
			return false;
		}
	}
	return true;
}
FString String(const FObject &Obj, const TCHAR *Field, const TCHAR *Default = TEXT(""))
{
	FString Value;
	return Obj->TryGetStringField(Field, Value) ? Value : Default;
}
bool StringList(const FObject &Obj, const TCHAR *Field, TArray<FString> &Out, FString &Error)
{
	if (!Obj->HasField(Field))
		return true;
	const TArray<FValue> *Values;
	if (!Obj->TryGetArrayField(Field, Values))
	{
		Error = FString(Field) + TEXT(" must be an array");
		return false;
	}
	for (const auto &Value : *Values)
	{
		if (!Value || Value->Type != EJson::String)
		{
			Error = FString(Field) + TEXT(" must contain strings");
			return false;
		}
		Out.AddUnique(Value->AsString());
	}
	return true;
}
bool InList(const FString &Value, std::initializer_list<const TCHAR *> Allowed)
{
	for (const TCHAR *Text : Allowed)
		if (Value == Text)
			return true;
	return false;
}
bool ValidId(const FString &Id)
{
	if (Id.IsEmpty())
		return false;
	for (TCHAR C : Id)
		if (!(C >= 'a' && C <= 'z') && !(C >= '0' && C <= '9') && C != '_')
			return false;
	return true;
}
bool Integer(const FObject &Obj, const TCHAR *Key, int32 Min, int32 Max, int32 &Out, FString &Error)
{
	if (!Obj->HasField(Key))
		return true;
	double N;
	if (!Obj->TryGetNumberField(Key, N) || !FMath::IsFinite(N) || N < Min || N > Max || N != FMath::FloorToDouble(N))
	{
		Error = FString::Printf(TEXT("%s must be an integer in %d..%d"), Key, Min, Max);
		return false;
	}
	Out = int32(N);
	return true;
}
bool FilenameMatches(const FString &File, FName Id, FString &Error)
{
	if (FPaths::GetBaseFilename(File).Equals(Id.ToString(), ESearchCase::IgnoreCase))
		return true;
	Error = FString::Printf(TEXT("%s: filename must equal id %s"), *File, *Id.ToString());
	return false;
}
TArray<FString> FilesIn(const FString &Root, const TCHAR *Dir)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(Root, Dir), TEXT("*.json"), true, false);
	// Collection order also defines the equipment-drop random stream. Match the shared file walker.
    const bool Collection=FString(Dir)==TEXT("Equipment") || FString(Dir)==TEXT("Items");
    Files.Sort([Collection](const auto& A,const auto& B) { return Collection ? A.ToUpper().Compare(B.ToUpper(),ESearchCase::CaseSensitive)<0 : A.ToLower()<B.ToLower(); });
	return Files;
}
} // namespace

bool FDWRuleLibraryLoader::ReadCollection(const FString &Text, ECollectionKind Kind, FDWCollection &Out, FString &Error)
{
	FObject Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		Error = TEXT("collection is not valid JSON");
		return false;
	}
	const bool Equipment = Kind == ECollectionKind::Equipment;
	if (Equipment ? !Closed(Root,
							{TEXT("id"), TEXT("name"), TEXT("description"), TEXT("slot"), TEXT("rarity"),
							 TEXT("level"), TEXT("tags"), TEXT("skills"), TEXT("traits"), TEXT("tier"),
							 TEXT("legendary"), TEXT("bases")},
							Error)
				  : !Closed(Root,
							{TEXT("id"), TEXT("name"), TEXT("description"), TEXT("category"), TEXT("rarity"),
							 TEXT("stack"), TEXT("tags"), TEXT("skill"), TEXT("traits"), TEXT("craft"), TEXT("tier")},
							Error))
		return false;
	FDWCollection Doc;
	const FString Id = String(Root, TEXT("id"));
	if (!ValidId(Id))
	{
		Error = TEXT("invalid collection id");
		return false;
	}
	Doc.Id = FName(*Id);
	Doc.Name = String(Root, TEXT("name"));
	Doc.Description = String(Root, TEXT("description"));
	const FString Rarity = String(Root, TEXT("rarity"), TEXT("magic"));
	if (!InList(Rarity, {TEXT("common"), TEXT("magic"), TEXT("rare"), TEXT("legendary"), TEXT("unique")}))
	{
		Error = TEXT("unknown rarity ") + Rarity;
		return false;
	}
	Doc.Rarity = FName(*Rarity);
	if (!StringList(Root, TEXT("tags"), Doc.Tags, Error))
		return false;
	if (Equipment)
	{
		const FString Slot = String(Root, TEXT("slot"), TEXT("weapon"));
		if (!InList(Slot, {TEXT("weapon"), TEXT("offhand"), TEXT("helmet"), TEXT("chest"), TEXT("gloves"),
						   TEXT("boots"), TEXT("belt"), TEXT("amulet"), TEXT("ring"), TEXT("lineage"),
						   TEXT("rarity"), TEXT("tower"), TEXT("character"), TEXT("default"), TEXT("map")}))
		{
			Error = TEXT("unknown slot ") + Slot;
			return false;
		}
		Doc.Slot = FName(*Slot);
		if (Root->HasField(TEXT("bases")))
		{
			const TArray<TSharedPtr<FJsonValue>>* Bases;
			if (!Root->TryGetArrayField(TEXT("bases"), Bases) || !Bases->Num() || (Doc.Rarity != TEXT("legendary") && Bases->Num() != 1))
			{ Error = TEXT("ordinary bases require one type; legendary bases list their fixed traits"); return false; }
			for (const auto& Value : *Bases)
			{
				FDWTraitEntry Entry;
				if (!FDWRuleProgramReader::ReadTraitEntry(Value, Entry, Error)) return false;
				if (Entry.Duration > 0 || Entry.With.Num()) { Error = TEXT("base cannot be timed or stitched"); return false; }
				if (Doc.Bases.ContainsByPredicate([&](const auto& Existing) { return Existing.Id == Entry.Id; })) { Error = TEXT("duplicate fixed base trait"); return false; }
				Doc.Bases.Add(MoveTemp(Entry));
			}
		}
		if (Root->HasField(TEXT("legendary")))
		{
			FDWTraitEntry E;
			if (!FDWRuleProgramReader::ReadTraitEntry(Root->TryGetField(TEXT("legendary")), E, Error))
				return false;
			Doc.Legendary = MoveTemp(E);
		}
		if (!Integer(Root, TEXT("level"), 1, 100, Doc.Level, Error))
			return false;
		if (!Integer(Root, TEXT("tier"), 1, 10, Doc.Tier, Error)) return false;
		TArray<FString> Skills;
		if (!StringList(Root, TEXT("skills"), Skills, Error))
			return false;
		for (const auto &Skill : Skills)
			Doc.Skills.Add(FName(*Skill));
	}
	else
	{
		const FString Category = String(Root, TEXT("category"), TEXT("rune"));
		if (!InList(Category, {TEXT("consumable"), TEXT("rune"), TEXT("gem"), TEXT("material"), TEXT("quest"),
							   TEXT("currency"), TEXT("omen"), TEXT("map")}))
		{
			Error = TEXT("unknown category ") + Category;
			return false;
		}
		Doc.Category = FName(*Category);
		if (Category == TEXT("map"))
		{
			if (!Integer(Root, TEXT("tier"), 1, 16, Doc.Tier, Error)) return false;
			if (!InList(Rarity, {TEXT("common"), TEXT("magic"), TEXT("rare")})) { Error=TEXT("map items support common, magic or rare rarity"); return false; }
		}
		else if (Root->HasField(TEXT("tier"))) { Error=TEXT("item tier is only valid for maps"); return false; }
		if (Root->HasField(TEXT("craft")))
		{
			const FObject *Craft;
			if (Category != TEXT("currency") || !Root->TryGetObjectField(TEXT("craft"), Craft) ||
				!Closed(*Craft, {TEXT("op"), TEXT("count"), TEXT("grade")}, Error))
				return false;
			Doc.CraftOp = FName(*String(*Craft, TEXT("op")));
			if (Doc.CraftOp == TEXT("add_affixes") || Doc.CraftOp == TEXT("roll_affixes") || Doc.CraftOp == TEXT("remove_affixes"))
			{
				if (!(*Craft)->HasField(TEXT("count")) ||
					!Integer(*Craft, TEXT("count"), 1, MAX_int32, Doc.CraftCount, Error))
				{
					Error = TEXT("invalid random affix count");
					return false;
				}
			}
			else if (Doc.CraftOp != TEXT("reroll") || (*Craft)->HasField(TEXT("count")))
			{
				Error = TEXT("invalid currency craft operation");
				return false;
			}
			if ((*Craft)->HasField(TEXT("grade")) &&
				(Doc.CraftOp != TEXT("reroll") || !Integer(*Craft, TEXT("grade"), 1, 3, Doc.CraftGrade, Error)))
			{
				Error = TEXT("craft.grade requires reroll and an integer from 1 to 3");
				return false;
			}
		}
		if (!Integer(Root, TEXT("stack"), 1, 9999, Doc.Stack, Error))
			return false;
		const FName Skill(*String(Root, TEXT("skill")));
		if (Category == TEXT("map") && !Skill.IsNone()) { Error=TEXT("map items cannot cast skills"); return false; }
		if (!Skill.IsNone())
			Doc.Skills.Add(Skill);
	}
	if (Root->HasField(TEXT("traits")))
	{
		const TArray<FValue> *Values;
		if (!Root->TryGetArrayField(TEXT("traits"), Values))
		{
			Error = TEXT("traits must be an array");
			return false;
		}
		TSet<FName> Seen;
		for (const auto &Value : *Values)
		{
			FDWTraitEntry Entry;
			if (!FDWRuleProgramReader::ReadTraitEntry(Value, Entry, Error))
				return false;
			if (Entry.With.IsEmpty())
			{
				if (Seen.Contains(Entry.Id))
				{
					Error = TEXT("duplicate trait ") + Entry.Id.ToString();
					return false;
				}
				Seen.Add(Entry.Id);
			}
			Doc.Traits.Add(MoveTemp(Entry));
		}
	}
	if (Doc.Category == TEXT("map") && !Doc.Traits.IsEmpty()) { Error=TEXT("map item templates cannot contain fixed affixes"); return false; }
	Out = MoveTemp(Doc);
	return true;
}

bool FDWRuleLibraryLoader::LoadDirectory(const FString &Root, FDWRuleLibrary &Out, FString &Error)
{
	FDWRuleLibrary Next;
	TArray<FString> TraitFiles;
	for (const TCHAR *Dir : {TEXT("Traits"), TEXT("WebTraits"), TEXT("Timed"), TEXT("Affixes")})
		TraitFiles.Append(FilesIn(Root, Dir));
	TraitFiles.Sort([](const auto &A, const auto &B) { return A.ToLower() < B.ToLower(); });
	for (const FString &File : TraitFiles)
	{
		FString Text;
		FDWLibraryTrait Trait;
		if (!FFileHelper::LoadFileToString(Text, *File) ||
			!FDWRuleProgramReader::ReadTraitText(Text, Trait.Source, Trait.DisplayName, Error))
		{
			Error = File + TEXT(": ") + Error;
			return false;
		}
		if (!FilenameMatches(File, Trait.Source.Id, Error))
			return false;
		if (Next.Traits.Contains(Trait.Source.Id))
		{
			Error = TEXT("duplicate trait id ") + Trait.Source.Id.ToString();
			return false;
		}
		Trait.Path = File;
		FString Relative=File, RootFolder=Root; FPaths::NormalizeDirectoryName(RootFolder); RootFolder+=TEXT("/"); FPaths::MakePathRelativeTo(Relative,*RootFolder); Relative.ReplaceInline(TEXT("\\"),TEXT("/"));
		Trait.bHosted=Relative.StartsWith(TEXT("Timed/"),ESearchCase::IgnoreCase) || Relative.StartsWith(TEXT("Affixes/"),ESearchCase::IgnoreCase);
		TFunction<bool(const TArray<FDWActionSource>&)> HasGrant=[&](const auto& Actions) { for (const auto& A : Actions) if (A.Op==EOp::GrantTraits || HasGrant(A.Then)) return true; return false; };
        const bool ExtendedSource=Relative.StartsWith(TEXT("WebTraits/"),ESearchCase::IgnoreCase);
		if (!Trait.bHosted && !ExtendedSource && (Trait.Source.Sheet.Num() || Trait.Source.Rules.ContainsByPredicate([&](const auto& R) { return HasGrant(R.Actions); })))
		{ Error=File+TEXT(": sheet/grant_traits must be authored under Affixes or Timed"); return false; }
		FObject Obj;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Obj))
			Trait.Description = String(Obj, TEXT("description"));
		Next.Traits.Add(Trait.Source.Id, MoveTemp(Trait));
	}
    TArray<FString> StatusFiles=FilesIn(Root,TEXT("Statuses"));StatusFiles.Append(FilesIn(Root,TEXT("WebStatuses")));
	for (const FString &File : StatusFiles)
	{
		FString Text, Name;
		FDWStatusDef Status;
		if (!FFileHelper::LoadFileToString(Text, *File) ||
			!FDWRuleProgramReader::ReadStatusText(Text, Status, Name, Error))
		{
			Error = File + TEXT(": ") + Error;
			return false;
		}
		if (!FilenameMatches(File, Status.Id, Error))
			return false;
		if (Next.Statuses.ContainsByPredicate([&](const auto &S) { return S.Key == Status.Key; }))
		{
			Error = TEXT("duplicate status id");
			return false;
		}
		Next.Statuses.Add(MoveTemp(Status));
	}
	TArray<FString> BuildFiles=FilesIn(Root,TEXT("Builds"));BuildFiles.Append(FilesIn(Root,TEXT("WebBuilds")));
    for (const FString &File : BuildFiles)
	{
		FString Text;
		FDWRuleBuildDef Build;
		if (!FFileHelper::LoadFileToString(Text, *File) || !FDWRuleProgramReader::ReadBuildText(Text, Build, Error))
		{
			Error = File + TEXT(": ") + Error;
			return false;
		}
		if (!FilenameMatches(File, Build.Id, Error))
			return false;
		if (Next.Builds.Contains(Build.Id))
		{
			Error = TEXT("duplicate build id");
			return false;
		}
		Next.Builds.Add(Build.Id, MoveTemp(Build));
	}
	for (const TCHAR *Dir : {TEXT("Equipment"), TEXT("Items")})
	{
		const bool Equipment = FString(Dir) == TEXT("Equipment");
		for (const FString &File : FilesIn(Root, Dir))
		{
			FString Text;
			FDWCollection Collection;
			if (!FFileHelper::LoadFileToString(Text, *File) ||
				!ReadCollection(Text, Equipment ? ECollectionKind::Equipment : ECollectionKind::Item, Collection,
								Error))
			{
				Error = File + TEXT(": ") + Error;
				return false;
			}
			if (!FilenameMatches(File, Collection.Id, Error))
				return false;
			auto &Map = Equipment ? Next.Equipment : Next.Items;
			if (Map.Contains(Collection.Id))
			{
				Error = TEXT("duplicate collection id");
				return false;
			}
			Map.Add(Collection.Id, MoveTemp(Collection));
		}
	}
	auto TagFiles = FilesIn(Root, TEXT("Tags"));
    // RuleStudio3D classification is editor/presentation metadata, not a second family document.
    TagFiles.RemoveAll([](const FString& F){return FPaths::GetCleanFilename(F)==TEXT("affix_categories.json")||FPaths::GetCleanFilename(F)==TEXT("affix_positions.json");});
	if (TagFiles.Num() > 1)
	{
		Error = TEXT("Tags must contain only families.json");
		return false;
	}
	if (TagFiles.Num())
	{
		FString Text;
		if (FPaths::GetCleanFilename(TagFiles[0]) != TEXT("families.json") ||
			!FFileHelper::LoadFileToString(Text, *TagFiles[0]) ||
			!FDWRuleTagReader::ReadText(Text, Next.TagDefinitions, Error) ||
			!Next.Tags.Build(Next, Next.TagDefinitions, Error))
			return false;
	}
	const auto ValueFiles = FilesIn(Root, TEXT("Values"));
	if (ValueFiles.Num() > 1) { Error = TEXT("Values must contain only trait_tiers.json"); return false; }
	if (ValueFiles.Num())
	{
		FString Text;
		if (FPaths::GetCleanFilename(ValueFiles[0]) != TEXT("trait_tiers.json") || !FFileHelper::LoadFileToString(Text, *ValueFiles[0]) ||
			!FDWRuleValueReader::ReadText(Text, Next, Next.ValueProfiles, Error)) return false;
	}
	for (const FString& File : FilesIn(Root, TEXT("Monsters")))
	{
		FString Text; FDWMonsterDefinition Monster;
		if (!FFileHelper::LoadFileToString(Text, *File) || !FDWRuleMonsterReader::ReadText(Text, Monster, Error) || !FilenameMatches(File, Monster.Id, Error)) { Error = File + TEXT(": ") + Error; return false; }
		if (Next.Monsters.Contains(Monster.Id)) { Error = TEXT("duplicate monster id ") + Monster.Id.ToString(); return false; }
		Next.Monsters.Add(Monster.Id, MoveTemp(Monster));
	}
	const auto ShopFiles = FilesIn(Root, TEXT("Shop"));
	if (ShopFiles.Num() > 1) { Error = TEXT("Shop must contain only catalog.json"); return false; }
	if (ShopFiles.Num())
	{
		FString Text;
		if (FPaths::GetCleanFilename(ShopFiles[0]) != TEXT("catalog.json") || !FFileHelper::LoadFileToString(Text, *ShopFiles[0]) || !FDWRuleShopReader::ReadText(Text, Next.Shop, Error)) return false;
	}
	if (!Next.Validate(Error))
		return false;
	Out = MoveTemp(Next);
	return true;
}
