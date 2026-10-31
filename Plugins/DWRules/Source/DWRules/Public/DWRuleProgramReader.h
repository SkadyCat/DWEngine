#pragma once

#include "CoreMinimal.h"
#include "DWRuleProgram.h"
#include "DWRuleWorld.h"
#include "DWRuleLibrary.h"

class UDWTraitDefinition;
class FJsonValue;

// A named skill: an ordered set of trait ids whose first entry (or Active) is the active trait.
using FDWRuleBuildDef = DWRules::FDWBuildDefinition;

// Translates SkillStudio-style JSON programs into the core source model. Unknown fields and
// unsupported operators are errors, mirroring the closed JS contract (public/affix-framework).
struct DWRULES_API FDWRuleProgramReader
{
	static bool ReadTrait(const UDWTraitDefinition& Asset, DWRules::FDWTraitSource& OutTrait, FString& OutError);
	// {"modifiers":[...],"rules":[...]} into an existing trait (id/params already set).
	static bool ReadProgramText(const FString& Json, DWRules::FDWTraitSource& InOutTrait, FString& OutError);
	// Whole trait file: {"id","parent","name","params":{},"modifiers":[],"rules":[]}.
	static bool ReadTraitText(const FString& Json, DWRules::FDWTraitSource& OutTrait, FString& OutDisplayName, FString& OutError);
	// Skill file: {"id","name","active","traits":[ids]}.
	static bool ReadBuildText(const FString& Json, FDWRuleBuildDef& OutBuild, FString& OutError);
	// Status file: {"id","name","description","category","stacking","maxStacks","cap","effects":[...]}.
	static bool ReadStatusText(const FString& Json, DWRules::FDWStatusDef& OutStatus, FString& OutDisplayName, FString& OutError);
	static bool ReadTraitEntry(const TSharedPtr<FJsonValue>& Value, DWRules::FDWTraitEntry& OutEntry, FString& OutError);
};
