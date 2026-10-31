#pragma once
#include "DWRuleProgram.h"

namespace DWRules
{
constexpr int32 TraitTierCount = 10;
struct FDWRuleLibrary;
struct FDWTierParameter
{
	double Step = 1;
	TArray<FVector2d> Bands;
};
struct FDWTraitValueProfile
{
	TMap<FName, FDWTierParameter> Params;
};
class DWRULESCORE_API FDWTraitValues
{
  public:
	static FDWTraitValueProfile Defaults(const FDWTraitSource &Source);
	static bool Profile(const FDWRuleLibrary &Library, FName Id, FDWTraitValueProfile &Out);
	static bool Roll(const FDWTraitValueProfile &Profile, int32 Tier, TFunctionRef<double()> Random,
					 TMap<FName, double> &Out, FString &Error);
	static bool Create(const FDWRuleLibrary &Library, FName Id, int32 Tier, TFunctionRef<double()> Random,
					   FDWTraitEntry &Out, FString &Error);
	static bool Validate(const FDWRuleLibrary &Library, const FDWTraitEntry &Entry, FString &Error,
						 bool CheckRange = true);
	static double Unit(double Random);
};
} // namespace DWRules
