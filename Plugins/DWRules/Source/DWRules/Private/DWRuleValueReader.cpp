#include "DWRuleValueReader.h"
#include "DWRuleLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

using namespace DWRules;
namespace
{
using FValueObject = TSharedPtr<FJsonObject>;
bool ValueFields(const FValueObject &O, const TSet<FString> &Allowed, FString &Error)
{
	if (!O)
	{
		Error = TEXT("expected value object");
		return false;
	}
	for (const auto &P : O->Values)
		if (!Allowed.Contains(FString(P.Key)))
		{
			Error = TEXT("unknown value field ") + FString(P.Key);
			return false;
		}
	return true;
}
} // namespace
bool FDWRuleValueReader::ReadText(const FString &Text, const FDWRuleLibrary &Library,
								  TMap<FName, FDWTraitValueProfile> &Out, FString &Error)
{
	FValueObject Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) ||
		!ValueFields(Root, {TEXT("version"), TEXT("tiers"), TEXT("traits")}, Error))
		return false;
	double Version, Tiers;
	const FValueObject *Traits = nullptr;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1 ||
		!Root->TryGetNumberField(TEXT("tiers"), Tiers) || Tiers != TraitTierCount ||
		!Root->TryGetObjectField(TEXT("traits"), Traits))
	{
		Error = TEXT("value catalog requires version 1 and 10 tiers");
		return false;
	}
	TMap<FName, FDWTraitValueProfile> Next;
	for (const auto &P : Library.Traits)
		if (P.Value.Source.Params.Num())
			Next.Add(P.Key, FDWTraitValues::Defaults(P.Value.Source));
	for (const auto &Trait : (*Traits)->Values)
	{
		const FName Id(*FString(Trait.Key));
		const auto *Source = Library.Traits.Find(Id);
		const auto Profile = Trait.Value->AsObject();
		if (!Source || !Source->Source.Params.Num())
		{
			Error = TEXT("value catalog references a missing or nonnumeric trait ") + Id.ToString();
			return false;
		}
		if (!ValueFields(Profile, {TEXT("params")}, Error))
			return false;
		const FValueObject *Params = nullptr;
		if (!Profile->TryGetObjectField(TEXT("params"), Params))
		{
			Error = TEXT("value profile requires params");
			return false;
		}
		for (const auto &Param : (*Params)->Values)
		{
			const FName Name(*FString(Param.Key));
			if (!Source->Source.Params.Contains(Name))
			{
				Error = Id.ToString() + TEXT(" has no parameter ") + Name.ToString();
				return false;
			}
			const auto Spec = Param.Value->AsObject();
			if (!ValueFields(Spec, {TEXT("step"), TEXT("tiers")}, Error))
				return false;
			FDWTierParameter Value;
			const TArray<TSharedPtr<FJsonValue>> *Bands = nullptr;
			if (!Spec->TryGetNumberField(TEXT("step"), Value.Step) || !FMath::IsFinite(Value.Step) || Value.Step <= 0 ||
				!Spec->TryGetArrayField(TEXT("tiers"), Bands) || Bands->Num() != TraitTierCount)
			{
				Error = TEXT("each numeric parameter requires a positive step and ten bands");
				return false;
			}
			for (const auto &Band : *Bands)
			{
				const TArray<TSharedPtr<FJsonValue>> *Ends = nullptr;
				double Min, Max;
				if (!Band->TryGetArray(Ends) || Ends->Num() != 2 || !(*Ends)[0]->TryGetNumber(Min) ||
					!(*Ends)[1]->TryGetNumber(Max) || !FMath::IsFinite(Min) || !FMath::IsFinite(Max) || Min > Max)
				{
					Error = TEXT("tier band requires finite [min, max]");
					return false;
				}
				for (double N : {Min, Max})
					if (FMath::Abs(N / Value.Step - FMath::FloorToDouble(N / Value.Step + .5)) > 1.e-6)
					{
						Error = TEXT("tier endpoint does not match step");
						return false;
					}
				Value.Bands.Add({Min, Max});
			}
			Next[Id].Params.Add(Name, MoveTemp(Value));
		}
	}
	Out = MoveTemp(Next);
	return true;
}
