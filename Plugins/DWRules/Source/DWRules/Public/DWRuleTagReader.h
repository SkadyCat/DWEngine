#pragma once
#include "DWRuleTags.h"

class DWRULES_API FDWRuleTagReader
{
  public:
	static bool ReadText(const FString &Text, DWRules::FDWTagDocument &Out, FString &Error);
};
