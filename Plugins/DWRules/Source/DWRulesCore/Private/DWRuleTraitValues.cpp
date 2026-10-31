#include "DWRuleTraitValues.h"
#include "DWRuleLibrary.h"
#include <limits>

namespace DWRules
{
namespace
{
double CleanValue(double V)
{
	return FMath::RoundToDouble(V * 1.e8) / 1.e8;
}
double Endpoint(FName Name, double Base, double Factor, double Step)
{
	if (Base == 0)
		return 0;
	double V = Base * Factor;
	const FString Key = Name.ToString().ToLower();
	if (Key.Contains(TEXT("chance")) || Key.Contains(TEXT("percent")) || Key.Contains(TEXT("slow")))
		V = FMath::Min(100., V);
	V = FMath::FloorToDouble(V / Step + .5) * Step;
	return CleanValue(V != 0 ? V : FMath::Sign(Base) * Step);
}
} // namespace
double FDWTraitValues::Unit(double Random)
{
	if (!FMath::IsFinite(Random))
		Random = 0;
	return FMath::Clamp(Random, 0., 1 - std::numeric_limits<double>::epsilon());
}
FDWTraitValueProfile FDWTraitValues::Defaults(const FDWTraitSource &Source)
{
	FDWTraitValueProfile Profile;
	for (const auto &Pair : Source.Params)
	{
		FDWTierParameter P;
		P.Step = FMath::FloorToDouble(Pair.Value) == Pair.Value ? 1 : .01;
		const bool Selector = Source.Sheet.ContainsByPredicate([&](const auto &E) {
			return ((Pair.Key == TEXT("source") || Pair.Key == TEXT("element")) &&
					(E.Trigger == TEXT("extra_damage") || E.Trigger == TEXT("convert_damage"))) ||
				   (Pair.Key == TEXT("status") &&
					(E.Trigger == TEXT("proj_prefer_status") || E.Trigger == TEXT("minion_prefer_status")));
		});
		if (Selector)
		{
			for (int32 I = 0; I < TraitTierCount; ++I)
				P.Bands.Add({Pair.Value, Pair.Value});
			Profile.Params.Add(Pair.Key, MoveTemp(P));
			continue;
		}
		const double First = Endpoint(Pair.Key, Pair.Value, .5, P.Step),
					 Last = Endpoint(Pair.Key, Pair.Value, 2, P.Step), Direction = Last < First ? -1 : 1;
		const double Count = FMath::FloorToDouble(FMath::Abs(Last - First) / P.Step + .5) + 1;
		for (int32 I = 0; I < TraitTierCount; ++I)
		{
			const double Start = Count >= TraitTierCount ? FMath::FloorToDouble(I * Count / TraitTierCount)
														 : FMath::FloorToDouble(I * (Count - 1) / (TraitTierCount - 1));
			const double End =
				Count >= TraitTierCount ? FMath::FloorToDouble((I + 1) * Count / TraitTierCount) - 1 : Start;
			const double A = CleanValue(First + Direction * Start * P.Step),
						 B = CleanValue(First + Direction * End * P.Step);
			P.Bands.Add({FMath::Min(A, B), FMath::Max(A, B)});
		}
		Profile.Params.Add(Pair.Key, MoveTemp(P));
	}
	return Profile;
}
bool FDWTraitValues::Profile(const FDWRuleLibrary &Library, FName Id, FDWTraitValueProfile &Out)
{
	const auto *Trait = Library.Traits.Find(Id);
	if (!Trait || Trait->Source.Params.IsEmpty())
		return false;
	if (const auto *Existing = Library.ValueProfiles.Find(Id))
		Out = *Existing;
	else
		Out = Defaults(Trait->Source);
	return true;
}
bool FDWTraitValues::Roll(const FDWTraitValueProfile &Profile, int32 Tier, TFunctionRef<double()> Random,
						  TMap<FName, double> &Out, FString &Error)
{
	if (Tier < 1 || Tier > TraitTierCount)
	{
		Error = TEXT("特性阶级必须是 1–10 的整数");
		return false;
	}
	TMap<FName, double> Next;
	for (const auto &Pair : Profile.Params)
	{
		if (Pair.Value.Bands.Num() != TraitTierCount || !(Pair.Value.Step > 0))
		{
			Error = TEXT("invalid tier profile");
			return false;
		}
		const auto Band = Pair.Value.Bands[Tier - 1];
		const double Steps = FMath::FloorToDouble((Band.Y - Band.X) / Pair.Value.Step + .5);
		Next.Add(Pair.Key, CleanValue(Band.X + FMath::FloorToDouble(Unit(Random()) * (Steps + 1)) * Pair.Value.Step));
	}
	Out = MoveTemp(Next);
	return true;
}
bool FDWTraitValues::Create(const FDWRuleLibrary &Library, FName Id, int32 Tier, TFunctionRef<double()> Random,
							FDWTraitEntry &Out, FString &Error)
{
	const auto *Trait = Library.Traits.Find(Id);
	if (!Trait)
	{
		Error = TEXT("unknown trait ") + Id.ToString();
		return false;
	}
	FDWTraitEntry Next;
	Next.Id = Trait->Source.Id;
	FDWTraitValueProfile Value;
	if (!Profile(Library, Id, Value))
	{
		if (Tier != 0)
		{
			Error = Id.ToString() + TEXT(" 没有数值参数，不需要分阶");
			return false;
		}
		Out = MoveTemp(Next);
		return true;
	}
	if (!Tier)
		Tier = 1 + FMath::FloorToInt(Unit(Random()) * TraitTierCount);
	Next.Tier = Tier;
	if (!Roll(Value, Tier, Random, Next.Params, Error))
		return false;
	Out = MoveTemp(Next);
	return true;
}
bool FDWTraitValues::Validate(const FDWRuleLibrary &Library, const FDWTraitEntry &Entry, FString &Error,
							  bool CheckRange)
{
	if (Entry.Tier == 0)
		return true;
	if (Entry.Tier < 1 || Entry.Tier > TraitTierCount)
	{
		Error = TEXT("特性阶级必须是 1–10 的整数");
		return false;
	}
	FDWTraitValueProfile P;
	if (!Profile(Library, Entry.Id, P))
	{
		Error = Entry.Id.ToString() + TEXT(" 没有数值参数，不需要分阶");
		return false;
	}
	for (const auto &Pair : P.Params)
	{
		const double *Value = Entry.Params.Find(Pair.Key);
		if (!Value || !FMath::IsFinite(*Value))
		{
			Error = Entry.Id.ToString() + TEXT(" 缺少实际参数 ") + Pair.Key.ToString();
			return false;
		}
		const auto Band = Pair.Value.Bands[Entry.Tier - 1];
		const double Step = Pair.Value.Step;
		if (CheckRange && (*Value < Band.X - 1.e-8 || *Value > Band.Y + 1.e-8 ||
						   FMath::Abs(*Value / Step - FMath::FloorToDouble(*Value / Step + .5)) > 1.e-6))
		{
			Error = Entry.Id.ToString() + TEXT(" 参数超出分阶范围：") + Pair.Key.ToString();
			return false;
		}
	}
	return true;
}
} // namespace DWRules
