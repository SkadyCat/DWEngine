#include "DWRuleProcessors.h"

#include "DWRuleTargets.h"
#include "DWRuleWorldSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"

void UDWRuleTargetTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment_GetRef<FDWRuleTargetFragment>() = Defaults;
}

UDWRuleFrameProcessor::UDWRuleFrameProcessor()
	: Query(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = int32(EProcessorExecutionFlags::AllNetModes);
	bRequiresGameThreadExecution = true;
}

void UDWRuleFrameProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FDWRuleTargetFragment>(EMassFragmentAccess::ReadWrite);
}

void UDWRuleFrameProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UDWRuleWorldSubsystem* Rules = World ? World->GetSubsystem<UDWRuleWorldSubsystem>() : nullptr;
	if (!Rules)
	{
		return;
	}
	Rules->BeginTargets();
	Query.ForEachEntityChunk(Context, [Rules](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FTransformFragment> Transforms = Chunk.GetFragmentView<FTransformFragment>();
		const TArrayView<FDWRuleTargetFragment> Targets = Chunk.GetMutableFragmentView<FDWRuleTargetFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			Rules->AddMassTarget(Chunk.GetEntity(Index), Transforms[Index].GetTransform().GetLocation(), Targets[Index]);
		}
	});
	Rules->StepFrame(Context.GetDeltaTimeSeconds(), &EntityManager);
}
