#pragma once
#include "CoreMinimal.h"
struct FDWRulePreset
{
	FName Id;
	FString Name, Description, Json;
};
// Reads data only. Applying a preset remains a transaction in DWRulesCore.
class DWRULES_API FDWRulePresetReader
{
  public:
	static bool Catalogue(TArray<FDWRulePreset> &Out, FString &Error);
};
