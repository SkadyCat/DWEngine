#pragma once
#include "DWRuleMonsters.h"
class DWRULES_API FDWRuleMonsterReader
{
  public:
	static bool ReadText(const FString &Text, DWRules::FDWMonsterDefinition &Out, FString &Error);
};
