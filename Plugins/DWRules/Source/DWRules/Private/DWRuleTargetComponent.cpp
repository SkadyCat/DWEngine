#include "DWRuleTargetComponent.h"

#include "DWRuleWorldSubsystem.h"
#include "Engine/World.h"

UDWRuleTargetComponent::UDWRuleTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDWRuleTargetComponent::ResetHealth()
{
	Health = MaxHealth;
}

void UDWRuleTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Health <= 0.f || Health > MaxHealth)
	{
		Health = MaxHealth;
	}
	if (UDWRuleWorldSubsystem* Rules = GetWorld()->GetSubsystem<UDWRuleWorldSubsystem>())
	{
		Rules->RegisterActorTarget(this);
	}
}

void UDWRuleTargetComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld())
	{
		if (UDWRuleWorldSubsystem* Rules = World->GetSubsystem<UDWRuleWorldSubsystem>())
		{
			Rules->UnregisterActorTarget(this);
		}
	}
	Super::EndPlay(Reason);
}
