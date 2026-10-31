#pragma once

#include "DWRulePlan.h"
#include "DWRuleProgram.h"

namespace DWRules
{
	struct FDWCompileOptions
	{
		int32 DefaultMaxEvents = 16384;
	};

	// Composes a build (parent closure), folds parameters/stats into constants,
	// pre-aggregates modifiers and flattens rule trees. Rejects unsupported semantics.
	class DWRULESCORE_API FDWRuleCompiler
	{
	public:
		static TSharedPtr<FDWCompiledPlan> Compile(const FDWBuildSource& Build, FString& OutError,
			const FDWCompileOptions& Options = FDWCompileOptions());
	};
}
