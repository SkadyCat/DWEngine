#pragma once

#include "DWRuleProgram.h"

namespace DWRules
{
enum class ESheetCombine : uint8
{
	Sum,
	More,
	Max
};
enum class EStitchRole : uint8
{
	None,
	Cast,
	Grant
};
struct FDWSheetTriggerSpec
{
	TArray<FName> Params;
	EStitchRole Stitched = EStitchRole::None;
};
DWRULESCORE_API const TMap<FName, ESheetCombine> &SheetStatCatalog();
DWRULESCORE_API const TMap<FName, FDWSheetTriggerSpec> &SheetTriggerCatalog();
DWRULESCORE_API bool ValidateSheet(const FDWTraitSource &Trait, FString &Error);
DWRULESCORE_API EStitchRole SheetStitchRole(const FDWTraitSource &Trait);
DWRULESCORE_API bool IsStitchable(const FDWTraitSource &Trait);

struct FDWHeldTrait
{
	FDWTraitSource Source;
	bool bEquipment = false;
	FName Slot;
	FString Label;
};
struct FDWSheetTrigger
{
	FName Name;
	FName Trait;
	FString Label;
	TMap<FName, double> Params;
	TArray<FDWTraitSource> Stitched;
	double Cooldown = 0.0;
	double Accumulator = 0.0;
	double Param(FName Key) const
	{
		const double *V = Params.Find(Key);
		return V ? *V : 0.0;
	}
};
struct DWRULESCORE_API FDWSheet
{
	TMap<FName, double> Stats;
	TArray<FDWSheetTrigger> Triggers;
	bool Compute(const TArray<FDWHeldTrait> &Entries, FString &Error);
	double Stat(FName Path) const;
};
} // namespace DWRules
