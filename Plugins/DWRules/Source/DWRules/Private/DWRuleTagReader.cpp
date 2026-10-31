#include "DWRuleTagReader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

using namespace DWRules;
namespace
{
using FTagObject = TSharedPtr<FJsonObject>;
bool TagFields(const FTagObject &O, const TSet<FString> &Allowed, FString &Error)
{
	for (const auto &Pair : O->Values)
		if (!Allowed.Contains(FString(Pair.Key)))
		{
			Error = TEXT("unknown tag field ") + FString(Pair.Key);
			return false;
		}
	return true;
}
bool TagId(const FString &Text)
{
	if (Text.IsEmpty())
		return false;
	for (TCHAR C : Text)
		if (!(C >= 'a' && C <= 'z') && !(C >= '0' && C <= '9') && C != '_')
			return false;
	return true;
}
FString TagText(const FTagObject &O, const TCHAR *Key, const FString &Default = FString())
{
	FString Value;
	return O->TryGetStringField(Key, Value) && !Value.IsEmpty() ? Value : Default;
}
} // namespace
bool FDWRuleTagReader::ReadText(const FString &Text, FDWTagDocument &Out, FString &Error)
{
	FTagObject Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		Error = TEXT("invalid tag JSON");
		return false;
	}
	if (!TagFields(Root, {TEXT("families"), TEXT("tags"), TEXT("traits")}, Error))
		return false;
	const TArray<TSharedPtr<FJsonValue>> *Families = nullptr;
	if (!Root->TryGetArrayField(TEXT("families"), Families) || !Families->Num())
	{
		Error = TEXT("families must be a non-empty array");
		return false;
	}
	FDWTagDocument Next;
	TSet<FName> FamilyIds;
	for (const auto &Value : *Families)
	{
		const auto O = Value->AsObject();
		if (!O || !TagFields(O, {TEXT("id"), TEXT("name"), TEXT("description")}, Error))
			return false;
		const FString Id = TagText(O, TEXT("id"));
		if (!TagId(Id) || FamilyIds.Contains(FName(*Id)))
		{
			Error = TEXT("invalid or duplicate family ") + Id;
			return false;
		}
		FamilyIds.Add(FName(*Id));
		Next.Families.Add({FName(*Id), TagText(O, TEXT("name"), Id), TagText(O, TEXT("description"))});
	}
	const TSharedPtr<FJsonObject> *Tags = nullptr;
	if (!Root->TryGetObjectField(TEXT("tags"), Tags))
	{
		Error = TEXT("tags must be an object");
		return false;
	}
	for (const auto &Pair : (*Tags)->Values)
	{
		const FString Id(Pair.Key);
		const auto O = Pair.Value->AsObject();
		if (!TagId(Id) || FamilyIds.Contains(FName(*Id)) || !O || !TagFields(O, {TEXT("name"), TEXT("family")}, Error))
		{
			if (Error.IsEmpty())
				Error = TEXT("invalid tag ") + Id;
			return false;
		}
		const FName Family(*TagText(O, TEXT("family")));
		if (!FamilyIds.Contains(Family))
		{
			Error = TEXT("unknown family for tag ") + Id;
			return false;
		}
		Next.Tags.Add(FName(*Id), {FName(*Id), Family, TagText(O, TEXT("name"), Id)});
	}
	if (Root->HasField(TEXT("traits")))
	{
		const TSharedPtr<FJsonObject> *Traits = nullptr;
		if (!Root->TryGetObjectField(TEXT("traits"), Traits))
		{
			Error = TEXT("traits must be a tag map");
			return false;
		}
		for (const auto &Pair : (*Traits)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>> *List = nullptr;
			if (!Pair.Value->TryGetArray(List))
			{
				Error = TEXT("trait tags must be arrays");
				return false;
			}
			TArray<FName> Names;
			for (const auto &Value : *List)
			{
				FString Name;
				if (!Value->TryGetString(Name))
				{
					Error = TEXT("tag name must be a string");
					return false;
				}
				Names.Add(FName(*Name));
			}
			Next.TraitTags.Add(FName(*FString(Pair.Key)), MoveTemp(Names));
		}
	}
	Out = MoveTemp(Next);
	return true;
}
