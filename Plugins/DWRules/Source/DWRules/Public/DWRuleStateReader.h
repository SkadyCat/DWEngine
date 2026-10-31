#pragma once
#include "DWRuleProfile.h"

// Browser player.v2 saves and UE saves use the same ids, sockets and concrete rolled parameters.
class DWRULES_API FDWRuleStateReader
{
  public:
	static bool Read(const FString &Json, const DWRules::FDWRuleLibrary &Library, DWRules::FDWRuleProfile &Out,
					 FString &Error);
	static bool ReadPreset(const FString &Json, const DWRules::FDWRuleLibrary &Library,
						   DWRules::FDWRuleProfile &Out, FString &Error);
	static FString Write(const DWRules::FDWRuleProfile &Profile, const DWRules::FDWRuleLibrary &Library);
	static bool ReadEntries(const FString &Json, const DWRules::FDWRuleLibrary &Library,
							TArray<DWRules::FDWTraitEntry> &Out, FString &Error);
};
