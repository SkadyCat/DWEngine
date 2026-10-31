#pragma once
#include "DWRuleTraitValues.h"

class DWRULES_API FDWRuleValueReader
{
  public:
	static bool ReadText(const FString &Text, const DWRules::FDWRuleLibrary &Library,
						 TMap<FName, DWRules::FDWTraitValueProfile> &Out, FString &Error);
};
