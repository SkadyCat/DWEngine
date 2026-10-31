#pragma once

#include "DWRuleProgram.h"

namespace DWRules
{
	// Reference builds used by tests and the benchmark command. They exercise stacking:
	// multiple/pierce/ricochet supports, kill explosions and a threshold feedback loop.
	struct DWRULESCORE_API FDWRuleSamples
	{
		static FDWTraitSource IceLance();
		static FDWTraitSource Multiple(int32 Extra);
		static FDWTraitSource Pierce(int32 Count);
		static FDWTraitSource Ricochet(int32 Count);
		static FDWTraitSource ShatterOnKill();
		static FDWTraitSource FrostReservoir();
		static FDWTraitSource ColdMastery(float IncreasedPercent);
		// Ice lance with every sample support; the "absurd stack" scenario.
		static FDWBuildSource StackedIceLance();
	};
}
