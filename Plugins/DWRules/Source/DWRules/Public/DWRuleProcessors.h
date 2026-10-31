#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "DWRuleProcessors.generated.h"

// The single per-frame entry of the rule runtime: exports hittable Mass entities, steps the
// world, writes authoritative health back. Runs after monster movement.
UCLASS()
class DWRULES_API UDWRuleFrameProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UDWRuleFrameProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};
