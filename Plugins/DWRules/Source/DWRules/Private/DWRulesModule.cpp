#include "DWRuleCompiler.h"
#include "DWRuleSamples.h"
#include "DWRuleWorld.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDWRulesBench, Log, All);

using namespace DWRules;

namespace
{
	// Headless core benchmark: N casts of the stacked ice lance into a target field.
	// Measures rule runtime cost only (no rendering, Mass export or GE).
	void RunBench(const TArray<FString>& Args)
	{
		const int32 Casts = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 100;
		const int32 TargetCount = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 100;
		const int32 Frames = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 600;
		FString Error;
		TSharedPtr<FDWCompiledPlan> Plan = FDWRuleCompiler::Compile(FDWRuleSamples::StackedIceLance(), Error);
		if (!Plan)
		{
			UE_LOG(LogDWRulesBench, Error, TEXT("compile failed: %s"), *Error);
			return;
		}
		FDWWorldConfig Config;
		FDWRuleWorld World(Config);
		const int32 PlanIndex = World.AddPlan(Plan.ToSharedRef());
		const int32 Owner = World.AddOwner(1, 0);
		World.UpdateOwner(Owner, FVector3f::ZeroVector, FVector3f(1.f, 0.f, 0.f));
		TArray<float> Times;
		Times.Reserve(Frames);
		int32 PeakProjectiles = 0;
		int64 TotalEvents = 0;
		int64 TotalHits = 0;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			FDWTargets& Targets = World.Targets();
			Targets.Reset();
			for (int32 Index = 0; Index < TargetCount; ++Index)
			{
				const float Angle = float(Index) * 2.399963f;
				const float Distance = 4.f + 0.35f * float(Index);
				// Respawn-in-place keeps the field full so kill chains keep firing.
				Targets.Add(FVector3f(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.f), 0.45f, 120.f, 1, uint32(Index + 1));
			}
			if (Frame % 12 == 0)
			{
				for (int32 Cast = 0; Cast < Casts / 5; ++Cast)
				{
					const float Yaw = float(Cast) * 0.37f + float(Frame) * 0.05f;
					World.Cast(PlanIndex, Owner, FVector3f::ZeroVector, FVector3f(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f), uint64(Frame * 1000 + Cast));
				}
			}
			const double Start = FPlatformTime::Seconds();
			World.Step(1.f / 60.f);
			Times.Add(float((FPlatformTime::Seconds() - Start) * 1000.0));
			World.DamageLog().Reset();
			PeakProjectiles = FMath::Max(PeakProjectiles, World.Projectiles().Num);
			TotalEvents += World.GetStats().Events;
			TotalHits += World.GetStats().Hits;
		}
		TArray<float> Sorted = Times;
		Sorted.Sort();
		double Sum = 0.0;
		for (float Time : Times)
		{
			Sum += Time;
		}
		const float P99 = Sorted[FMath::Clamp(int32(Sorted.Num() * 0.99f), 0, Sorted.Num() - 1)];
		UE_LOG(LogDWRulesBench, Display,
			TEXT("DWRules.Bench casts/12f=%d targets=%d frames=%d peakProjectiles=%d avg=%.3fms p99=%.3fms max=%.3fms events=%lld hits=%lld hash=%llx"),
			Casts / 5, TargetCount, Frames, PeakProjectiles, Sum / Times.Num(), P99, Sorted.Last(), TotalEvents, TotalHits, World.StateHash());
	}

	FAutoConsoleCommand BenchCommand(
		TEXT("DWRules.Bench"),
		TEXT("DWRules.Bench [casts] [targets] [frames]: headless rule runtime benchmark."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunBench));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, DWRules);
