#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_CleanRollRoot.generated.h"

// Visual-only correction after UE's rotation-only inertialization. Never changes
// extracted root motion, the mesh component transform, or the collision capsule.
USTRUCT(BlueprintInternalUseOnly)
struct FCS3C_API FAnimNode_CleanRollRoot : public FAnimNode_Base
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = Links) FPoseLink Source;

    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
    virtual void Evaluate_AnyThread(FPoseContext& Output) override;
    virtual void GatherDebugData(FNodeDebugData& DebugData) override;
    virtual bool HasPreUpdate() const override { return true; }
    virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
    virtual bool NeedsDynamicReset() const override { return true; }
    virtual void ResetDynamics(ETeleportType InTeleportType) override;

private:
    uint32 RequestedSerial = 0;
    uint32 EvaluatedSerial = 0;
    float Duration = .24f;
    float Elapsed = 0.f;
    float DeltaTime = 0.f;
    bool bHasHistory = false;
    FTransform PreviousComponent = FTransform::Identity;
    FVector PreviousRawRoot = FVector::ZeroVector;
    FVector InitialWorldCorrection = FVector::ZeroVector;
    FVector WorldCorrection = FVector::ZeroVector;
    TArray<FTransform> PreviousPose;
    TArray<FVector> PoseTranslationCorrection;
    TArray<FQuat> PoseRotationCorrection;
};
