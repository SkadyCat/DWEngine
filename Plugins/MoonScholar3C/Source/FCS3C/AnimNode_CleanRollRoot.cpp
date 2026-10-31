#include "AnimNode_CleanRollRoot.h"
#include "CleanMotionAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"

void FAnimNode_CleanRollRoot::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    FAnimNode_Base::Initialize_AnyThread(Context);
    Source.Initialize(Context);
    ResetDynamics(ETeleportType::ResetPhysics);
}

void FAnimNode_CleanRollRoot::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
    Source.CacheBones(Context);
}

void FAnimNode_CleanRollRoot::PreUpdate(const UAnimInstance* InAnimInstance)
{
    // UObject reads stay on the game thread, before parallel evaluation.
    if (const auto* Anim = Cast<UCleanMotionAnimInstance>(InAnimInstance))
    {
        RequestedSerial = Anim->GetRollPoseSerial();
        Duration = FMath::Clamp(Anim->GetPoseContinuityTime(), .06f, .4f);
    }
}

void FAnimNode_CleanRollRoot::Update_AnyThread(const FAnimationUpdateContext& Context)
{
    DeltaTime += Context.GetDeltaTime();
    Source.Update(Context);
}

void FAnimNode_CleanRollRoot::Evaluate_AnyThread(FPoseContext& Output)
{
    Source.Evaluate(Output);
    if (Output.Pose.GetNumBones() == 0) return;

    const FTransform Component = Output.AnimInstanceProxy->GetComponentTransform();
    auto& Root = Output.Pose[FCompactPoseBoneIndex(0)];
    const FVector RawRoot = Root.GetTranslation();
    const int32 NumBones = Output.Pose.GetNumBones();
    if (PreviousPose.Num() != NumBones) bHasHistory = false;
    // Teleports or long gaps must not carry an old visual offset into a new pose.
    if (DeltaTime > .25f || (bHasHistory &&
        FVector::DistSquared(Component.GetLocation(), PreviousComponent.GetLocation()) > FMath::Square(100.f)))
        bHasHistory = false;

    if (!bHasHistory)
    {
        PreviousPose.SetNum(NumBones);
        PoseTranslationCorrection.Init(FVector::ZeroVector, NumBones);
        PoseRotationCorrection.Init(FQuat::Identity, NumBones);
        InitialWorldCorrection = WorldCorrection = FVector::ZeroVector;
        Elapsed = Duration;
    }
    else if (RequestedSerial != EvaluatedSerial)
    {
        // Preserve the previous visual root vector relative to the capsule in
        // WORLD space. Keep this frame's real capsule/root-motion displacement.
        // TransformVector includes mesh scale but deliberately excludes location.
        InitialWorldCorrection = WorldCorrection +
            PreviousComponent.TransformVector(PreviousRawRoot) - Component.TransformVector(RawRoot);
        WorldCorrection = InitialWorldCorrection;
        Elapsed = 0.f; // Apply the whole correction on the action's first frame.
        // IK is solved after normal pose blending. On roll entry, preserve its
        // final local pose as contacts fade out, not the pre-IK FK pose.
        for (int32 I = 1; I < NumBones; ++I)
        {
            const auto& Current = Output.Pose[FCompactPoseBoneIndex(I)];
            PoseTranslationCorrection[I] = PreviousPose[I].GetTranslation() - Current.GetTranslation();
            PoseRotationCorrection[I] = (PreviousPose[I].GetRotation() * Current.GetRotation().Inverse()).GetNormalized();
        }
    }
    else
    {
        Elapsed = FMath::Min(Elapsed + DeltaTime, Duration);
        const float T = Elapsed / Duration;
        // Quintic decay has zero velocity/acceleration at both ends.
        const float Weight = 1.f - T * T * T * (10.f + T * (-15.f + 6.f * T));
        WorldCorrection = InitialWorldCorrection * Weight;
    }

    Root.AddToTranslation(Component.InverseTransformVector(WorldCorrection));
    const float T = Elapsed / Duration;
    const float PoseWeight = 1.f - T * T * T * (10.f + T * (-15.f + 6.f * T));
    for (int32 I = 1; I < NumBones; ++I)
    {
        auto& Bone = Output.Pose[FCompactPoseBoneIndex(I)];
        if (PoseWeight > 0.f)
        {
            Bone.AddToTranslation(PoseTranslationCorrection[I] * PoseWeight);
            Bone.SetRotation((FQuat::Slerp(FQuat::Identity, PoseRotationCorrection[I], PoseWeight) * Bone.GetRotation()).GetNormalized());
        }
        PreviousPose[I] = Bone;
    }
    PreviousRawRoot = RawRoot; // Never feed our correction back into its own history.
    PreviousComponent = Component;
    EvaluatedSerial = RequestedSerial;
    bHasHistory = true;
    DeltaTime = 0.f;
}

void FAnimNode_CleanRollRoot::ResetDynamics(ETeleportType InTeleportType)
{
    bHasHistory = false;
    DeltaTime = 0.f;
    InitialWorldCorrection = WorldCorrection = FVector::ZeroVector;
    PreviousPose.Reset();
    PoseTranslationCorrection.Reset();
    PoseRotationCorrection.Reset();
}

void FAnimNode_CleanRollRoot::GatherDebugData(FNodeDebugData& DebugData)
{
    DebugData.AddDebugItem(DebugData.GetNodeName(this));
    Source.GatherDebugData(DebugData);
}
