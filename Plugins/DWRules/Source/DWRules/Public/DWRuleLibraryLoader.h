#pragma once

#include "DWRuleLibrary.h"

// Cold-path file/JSON adapter. A failed load leaves the caller's previous library intact.
struct DWRULES_API FDWRuleLibraryLoader
{
	static bool LoadDirectory(const FString &AbsoluteRoot, DWRules::FDWRuleLibrary &Out, FString &Error);
	static bool ReadCollection(const FString &Text, DWRules::ECollectionKind Kind, DWRules::FDWCollection &Out,
							   FString &Error);
};
