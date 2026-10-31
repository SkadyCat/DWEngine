#include "CleanMotionAnimInstance.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "BoneControllers/AnimNode_FootPlacement.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "KismetAnimationLibrary.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedData.h"
#endif
UCleanMotionAnimInstance::UCleanMotionAnimInstance()
{
    bUseMultiThreadedAnimationUpdate = false;
    RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
}
void UCleanMotionAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
#if WITH_EDITOR
    // Reuse completed indexes across network pawns; DS only evaluates montage root motion.
    if (GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
    for (auto DB : Databases)
        if (DB)
            UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
                DB, UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest |
                        UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
#endif
    Matching = nullptr;
    WeaponPlayer = nullptr;
    History = nullptr;
    FootPlacement = nullptr;
    Trajectory.Samples.Reset();
    if (const IAnimClassInterface *I = IAnimClassInterface::GetFromClass(GetClass()))
        for (FStructProperty *P : I->GetAnimNodeProperties())
        {
            if (P->Struct == FAnimNode_SequencePlayer::StaticStruct())
                WeaponPlayer = P->ContainerPtrToValuePtr<FAnimNode_SequencePlayer>(this);
            if (P->Struct == FAnimNode_MotionMatching::StaticStruct())
                Matching = P->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(this);
            if (P->Struct == FAnimNode_PoseSearchHistoryCollector::StaticStruct())
                History = P->ContainerPtrToValuePtr<FAnimNode_PoseSearchHistoryCollector>(this);
            if (P->Struct == FAnimNode_FootPlacement::StaticStruct())
                FootPlacement = P->ContainerPtrToValuePtr<FAnimNode_FootPlacement>(this);
        }
}
void UCleanMotionAnimInstance::NativeUpdateAnimation(float Dt)
{
    Super::NativeUpdateAnimation(Dt);
    auto *C = Cast<ACharacter>(TryGetPawnOwner());
    if ((!C && !bExternalMotion) || Dt <= 0)
        return;
    auto *M = C ? C->GetCharacterMovement() : nullptr;
    const FVector ActualVelocity = bExternalMotion ? ExternalVelocity : C->GetVelocity();
    const FRotator BodyRotation = bExternalMotion ? ExternalBody.Rotator() : C->GetActorRotation();
    const float BodyScale = bExternalMotion ? FMath::Max(.01f, ExternalBody.GetScale3D().Z) : 1.f;
    // Montage root motion is not locomotion intent. Feeding a roll's displacement
    // into MM selects Walk_Stops (the action lock makes GetMaxSpeed() zero), then
    // switches to Idle during blend-out: two unrelated leg poses in one recovery.
    const bool FullBodyAction = IsCleanActionActive() && ActionMontage &&
        ActionMontage->SlotAnimTracks.ContainsByPredicate([](const FSlotAnimationTrack& Track)
        { return Track.SlotName == TEXT("CleanAction"); });
    const FVector Velocity = FullBodyAction ? FVector::ZeroVector : ActualVelocity / BodyScale;
    GroundSpeed = Velocity.Size2D();
    bCrouched = !bExternalMotion && C->bIsCrouched;
    bInAir = !bExternalMotion && M->IsFalling();
    // Fade out before changing clips, then fade in. Total switch time is 0.3 s.
    const bool Switching = PendingWeaponPose != WeaponPose;
    const float Target = Switching || bCrouched || bInAir ? 0.f : TargetWeaponWeight;
    WeaponLayerAlpha = FMath::FInterpConstantTo(WeaponLayerAlpha, Target, Dt, 1.f / .15f);
    if (Switching && WeaponLayerAlpha <= KINDA_SMALL_NUMBER) WeaponPose = PendingWeaponPose;
    EvaluatedWeaponPose = WeaponPlayer && WeaponPlayer->GetSequence() ? WeaponPlayer->GetSequence()->GetName() : TEXT("");
    // Grounded attacks/casts retain contact solving. The plant node releases
    // a swinging foot by height/speed; only rolling must bypass leg planting.
    const bool Rolling = IsCleanActionActive() && ActionMontage && ActionMontage->GetFName() == TEXT("AM_Roll");
    const bool MagePreview = IsCleanActionActive() && bActionUsesAuthoredFeet;
    bFootIKActive = GetWorld()->GetNetMode() != NM_DedicatedServer && bEnableFootIK && (bExternalMotion || M->IsMovingOnGround()) && !Rolling && !MagePreview;
    if (FootPlacement)
    {
        // While the capsule is stationary, a pose blend is not a reason to
        // unplant a foot that is still on the ground. Use height to release
        // lifted feet, and retain the stock speed gate for moving/root-motion
        // actions. Test real CMC velocity, not the MM-suppressed action speed.
        // Full-body actions own their foot trajectories. Retain ground alignment,
        // but never pin their stepping feet to a stationary locomotion contact.
        FootPlacement->PlantSettings.LockType = FullBodyAction ? EFootPlacementLockType::Unlocked
                                                              : EFootPlacementLockType::PivotAroundBall;
        const bool StationaryContact = bFootIKActive && !FullBodyAction && ActualVelocity.SizeSquared2D() < FMath::Square(20.f * BodyScale);
        FootPlacement->PlantSettings.SpeedThreshold = StationaryContact ? 10000.f : 60.f;
        FootPlacement->PlantSettings.UnalignmentSpeedThreshold = StationaryContact ? 10001.f : 200.f;
        FootPlacement->PlantSettings.DistanceToGround = StationaryContact ? 3.f : 10.f;
        FootPlacement->PlantSettings.UnplantRadius = StationaryContact ? 35.f : 20.f;
    }
    MovementDirection = UKismetAnimationLibrary::CalculateDirection(Velocity, BodyRotation);
    ++UpdateCount;
    if (!Matching || !History || GetWorld()->GetNetMode() == NM_DedicatedServer)
        return;
    // The stock predictor rotates input acceleration with controller yaw. Our WASD
    // basis is the fixed camera, while controller yaw is only the cursor aim.
    TrajectoryData.MaxControllerYawRate = bIndependentAimTrajectory ? 0.f : 70.f;
    FTransformTrajectory Generated, Collided;
    FPoseSearchTrajectory_WorldCollisionResults Collision;
    if (bExternalMotion)
    {
        // Keep evaluated world-space history; predict from the nav corridor intent.
        // Motion matching consumes component-space distances, including body scale.
        const FTransformTrajectory Previous = Trajectory;
        Generated.Samples.SetNum(46);
        const FVector At = GetSkelMeshComponent()->GetComponentLocation();
        const FVector Intent = FullBodyAction ? FVector::ZeroVector : ExternalIntent;
        for (int32 I = 0; I < Generated.Samples.Num(); ++I)
        {
            auto& Sample = Generated.Samples[I];
            const float Time = (I - 30) * .1f;
            if (Time < 0.f && !Previous.Samples.IsEmpty())
                UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(Previous, Time + Dt, Sample);
            else
            {
                const float T = FMath::Max(0.f, Time);
                Sample.Position = At + (FullBodyAction ? FVector::ZeroVector : ActualVelocity) * T +
                    (Intent - (FullBodyAction ? FVector::ZeroVector : ActualVelocity)) * (T - (1.f - FMath::Exp(-4.f * T)) / 4.f);
                Sample.Facing = GetSkelMeshComponent()->GetComponentQuat();
            }
            Sample.TimeInSeconds = Time;
        }
        Trajectory = MoveTemp(Generated);
    }
    else
    {
    UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(
        this, TrajectoryData, Dt, Trajectory, PreviousControllerYaw, Generated, -1.f, 30, 0.1f, 15);
    TArray<AActor *> Ignore{C};
    UPoseSearchTrajectoryLibrary::HandleTransformTrajectoryWorldCollisions(this, this, Generated, true, 0.01f,
                                                                  Collided, Collision, TraceTypeQuery1, false,
                                                                  Ignore, EDrawDebugTrace::None, true, 150.f);
    Trajectory = MoveTemp(Collided);
    }
    History->TransformTrajectory = Trajectory;
    TrajectorySamples = Trajectory.Samples.Num();
    FTransformTrajectorySample T04, T05;
    UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(Trajectory, .4f, T04);
    UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(Trajectory, .5f, T05);
    const FVector FutureVelocity = bExternalMotion && ExternalIntent.IsNearlyZero(.1f) ? FVector::ZeroVector :
        (T05.Position - T04.Position) * (10.f / BodyScale);
    PredictedVelocity = FutureVelocity;
    const bool Moving = !FutureVelocity.IsNearlyZero(.1f) && !Velocity.IsNearlyZero(.1f);
    OffsetTranslationMode =
        (!bInAir && Moving) ? EOffsetRootBoneMode::Interpolate : EOffsetRootBoneMode::Release;
    // A stationary locomotion pose must not drag both planted feet back toward
    // the capsule. Recover its residual offset when locomotion resumes instead.
    if (!bInAir && !FullBodyAction && GroundSpeed < 1.f)
        OffsetTranslationMode = EOffsetRootBoneMode::LockOffsetAndIgnoreAnimation;
    OffsetTranslationHalfLife = Moving ? .3f : .1f;
    const float MaxSpeed = bExternalMotion ? ExternalMaxSpeed / BodyScale : M->GetMaxSpeed();
    const int32 Gait = MaxSpeed > 550.f ? 2 : (MaxSpeed > 300.f ? 1 : 0);
    const auto &Search = Matching->GetMotionMatchingState().SearchResult;
    const auto *Current = Search.SelectedDatabase.Get();
    const bool WasPivot = Current && Current->Tags.Contains(TEXT("Pivots"));
    const bool Starting = Moving && (FutureVelocity.Size2D() >= GroundSpeed + 100.f ||
        (bExternalMotion && PreviousMovementState == 0)) && !WasPivot;
    const float PivotAngle =
        FMath::Abs(FMath::FindDeltaAngleDegrees(Velocity.Rotation().Yaw, FutureVelocity.Rotation().Yaw));
    const bool Pivoting = Moving && PivotAngle >= ((bExternalMotion || M->bOrientRotationToMovement) ? 60.f : 40.f);
    const float RootYaw = GetSkelMeshComponent()->GetSocketTransform(TEXT("root"), RTS_World).Rotator().Yaw + 90.f;
    const float RootError = FMath::Abs(FMath::FindDeltaAngleDegrees(RootYaw, BodyRotation.Yaw));
    const bool Spin = RootError >= 130.f && GroundSpeed >= 150.f && !WasPivot;
    const bool Turn = !FullBodyAction && RootError >= 50.f && (Starting || (!Moving && PreviousMovementState == 0));
    const bool Landed = bPreviousInAir && !bInAir;
    const bool Heavy = Landed && FMath::Abs(PreviousVerticalSpeed) >= FMath::Abs(HeavyLandSpeedThreshold);
    const bool Light = Landed && !Heavy;
    TArray<UPoseSearchDatabase *> Candidates;
    auto Add = [&](const FString &Name)
    {
        for (auto DB : Databases)
            if (DB && DB->GetName() == Name)
            {
                Candidates.AddUnique(DB);
                break;
            }
    };
    // The rows below are the original locomotion chooser rules; traversal rows are intentionally absent.
    if (bInAir)
    {
        Add(TEXT("PSD_Jump"));
        if (Collision.TimeToLand >= 1.2f)
            Add(TEXT("PSD_Jump_Far"));
    }
    else if (!Moving)
    {
        if (Light)
            Add(TEXT("PSD_Idle_Lands_Light"));
        else if (Heavy)
            Add(TEXT("PSD_Idle_Lands_Heavy"));
        else if (Turn)
            Add(TEXT("PSD_TurnInPlace"));
        else if (GroundSpeed < 20.f)
            Add(TEXT("PSD_Idle"));
        else
            Add(Gait == 0   ? TEXT("PSD_Walk_Stops")
                : Gait == 1 ? TEXT("PSD_Run_Stops")
                            : TEXT("PSD_Sprint_Stops"));
    }
    else if (Gait < 2)
    {
        FString Prefix = Gait == 0 ? TEXT("PSD_Walk") : TEXT("PSD_Run");
        if (!Light && !Heavy)
        {
            if (Starting)
                Add(Prefix + TEXT("_Starts"));
            Add(Prefix);
            if (Pivoting)
                Add(Prefix + TEXT("_Pivots"));
        }
        if (!Pivoting && Spin)
            Add(Prefix + TEXT("_SpinTransitions"));
        if (Light)
            Add(Prefix + TEXT("_Lands_Light"));
        if (Heavy)
            Add(Prefix + TEXT("_Lands_Heavy"));
    }
    else
    {
        if (!Pivoting)
        {
            Add(TEXT("PSD_Sprint_Loops"));
            if (Starting)
                Add(TEXT("PSD_Sprint_Starts"));
        }
        else
            Add(TEXT("PSD_Sprint_Pivots"));
        if (Light)
            Add(TEXT("PSD_Sprint_Lands_Light"));
        if (Heavy)
            Add(TEXT("PSD_Sprint_Lands_Heavy"));
    }
    if (Candidates.IsEmpty())
        Add(TEXT("PSD_Idle"));
    const int32 State = Moving ? 1 : 0;
    // Decelerating and fully stopped both have State==0. Without this edge the
    // search can keep playing a moving Stop clip after the capsule has stopped.
    const bool StoppedDatabaseChanged = !bInAir && GroundSpeed < 20.f && !Candidates.Contains(Current);
    const bool Changed = bPreviousInAir != bInAir || PreviousGait != Gait || PreviousMovementState != State || StoppedDatabaseChanged;
    Matching->SetDatabasesToSearch(Candidates, Changed ? EPoseSearchInterruptMode::InterruptOnDatabaseChange
                                                       : EPoseSearchInterruptMode::DoNotInterrupt);
    CandidateDatabases.Reset();
    for (auto *DB : Candidates)
        CandidateDatabases.Add(DB->GetName());
    MotionBlendTime = bInAir ? (Velocity.Z > 100.f ? .5f : .15f) : (bPreviousInAir ? .4f : .2f);
    SelectedDatabase = Current ? Current->GetName() : TEXT("");
    SelectedPose = Current ? Current->GetPoseIndex(Search.SelectedAnim, Search.SelectedTime, Search.bIsMirrored, Search.BlendParameters) : INDEX_NONE;
    SelectedTime = Search.SelectedTime;
    SelectedAnimation.Reset();
    SelectedLocalVelocity = FVector::ZeroVector;
    bSelectedMirrored = false;
    if (Current)
        if (const auto* IndexAsset = Current->GetSearchIndex().GetAssetForPoseSafe(SelectedPose))
            if (const auto* Entry = Current->GetDatabaseAnimationAsset(IndexAsset->GetSourceAssetIdx()))
                if (auto* Sequence = Cast<UAnimSequence>(Entry->GetAnimationAsset()))
                {
                    SelectedAnimation = Sequence->GetName();
                    bSelectedMirrored = IndexAsset->IsMirrored();
                    FDeltaTimeRecord RootDelta(.1f);
                    RootDelta.SetPrevious(Search.SelectedTime);
                    const FAnimExtractContext RootContext(Search.SelectedTime, true, RootDelta, IndexAsset->IsLooping());
                    FVector RootVelocity = Sequence->ExtractRootMotion(RootContext).GetTranslation() * 10.f;
                    // GASP's mirror table reflects skeleton X (character left/right).
                    if (bSelectedMirrored) RootVelocity.X *= -1.f;
                    SelectedLocalVelocity = (bExternalMotion ? FRotator(0,-90,0) : C->GetMesh()->GetRelativeRotation()).RotateVector(RootVelocity);
                }
    bPreviousInAir = bInAir;
    PreviousVerticalSpeed = Velocity.Z;
    PreviousGait = Gait;
    PreviousMovementState = State;
}

void UCleanMotionAnimInstance::SetExternalMotion(const FTransform& Body, const FVector& Velocity, const FVector& Intent, float MaxSpeed)
{
    bExternalMotion = true;
    ExternalBody = Body;
    // Mass can stop in one physics step. Decay the visual velocity so the same
    // character chooser gets a real Stop pose instead of jumping straight to Idle.
    ExternalVelocity = FMath::VInterpTo(ExternalVelocity, Velocity, GetWorld()->GetDeltaSeconds(), 12.f);
    ExternalIntent = Intent;
    ExternalMaxSpeed = MaxSpeed;
    RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

void UCleanMotionAnimInstance::SetWeaponLayer(UAnimSequence* Pose, float Weight)
{
    PendingWeaponPose = Pose;
    TargetWeaponWeight = Pose ? FMath::Clamp(Weight, 0.f, 1.f) : 0.f;
}
// Replaying the same saved montage leaves its interrupted instance fading out.
// UE GetInstanceForMontage returns the first (oldest) match; action state must
// follow the newest play, including that play's own natural blend-out.
static FAnimMontageInstance* LatestActionInstance(const TArray<FAnimMontageInstance*>& Instances, const UAnimMontage* Montage)
{
    if (!Montage) return nullptr;
    for (int32 Index = Instances.Num() - 1; Index >= 0; --Index)
        if (auto* Instance = Instances[Index]; Instance && Instance->Montage == Montage && Instance->IsValid())
            return Instance;
    return nullptr;
}

static bool CleanSkeletonCompatible(const USkeleton* Source, const USkeleton* Target)
{
#if WITH_EDITORONLY_DATA
    return Source->IsCompatibleForEditor(Target);
#else
    // Matches the previous engine's runtime compatibility contract.
    return Target != nullptr;
#endif
}

bool UCleanMotionAnimInstance::PlayCleanAction(UAnimSequence *Sequence, float Rate, float StartTime, bool UpperBody)
{
    const auto* MeshAsset = GetSkelMeshComponent() ? GetSkelMeshComponent()->GetSkeletalMeshAsset() : nullptr;
    const auto* TargetSkeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
    if (!Sequence || Rate <= 0.f || !TargetSkeleton || !Sequence->GetSkeleton() ||
        !CleanSkeletonCompatible(Sequence->GetSkeleton(), TargetSkeleton))
        return false;
    UAnimMontage* Montage = nullptr;
    const FString Name = FString(UpperBody ? TEXT("AMU_") : TEXT("AM_")) + Sequence->GetName();
    for (auto Candidate : NetworkActionMontages)
        if (Candidate && Candidate->GetName() == Name) { Montage = Candidate; break; }
    if (!Montage) Montage = LoadObject<UAnimMontage>(nullptr, *(TEXT("/Game/Clean3C/Animation/Montages/") + Name + TEXT(".") + Name));
    if (!Montage) return false;
    // Saved assets have stable NetGUIDs; transient dynamic montages cannot replicate root motion.
    if (Montage_Play(Montage, Rate, EMontagePlayReturnType::MontageLength,
                     FMath::Clamp(StartTime, 0.f, FMath::Max(0.f, Montage->GetPlayLength() - .01f))) <= 0.f)
        return false;
    ActionMontage = Montage;
    // Both catalogs and direct skill casts share the authored animation directory.
    // Do not infer foot treatment from the old pack's montage naming convention.
    bActionUsesAuthoredFeet = Sequence->GetPathName().StartsWith(TEXT("/Game/Clean3C/Animation/Mage/"));
    // The capsule turns to the roll direction immediately for authoritative root
    // motion. Preserve the previous *world-space* pose while that facing changes.
    // Keep standard montage blending/root-motion weights; this request smooths
    // only the evaluated pose through the output Inertialization node.
    if (Sequence->GetFName() == TEXT("Roll"))
    {
        PoseContinuityTime = FMath::Clamp(RollFacingBlendTime, .06f, .4f);
        ++RollPoseSerial;
        RequestMontageInertialization(Montage, FMath::Clamp(RollFacingBlendTime, .06f, .4f));
    }
    // Upper-body actions must not displace the capsule using the source clip's root track.
    if (UpperBody)
        if (auto* Instance = LatestActionInstance(MontageInstances, Montage)) Instance->PushDisableRootMotion();
    return true;
}
void UCleanMotionAnimInstance::StopCleanAction(float BlendOut)
{
    if (ActionMontage) Montage_Stop(BlendOut, ActionMontage);
}
void UCleanMotionAnimInstance::CancelCleanActionForMovement()
{
    if (!ActionMontage || !IsCleanActionActive()) return;
    const float Duration = FMath::Clamp(MovementCancelBlendTime, 0.f, .3f);
    if (Duration > SMALL_NUMBER)
    {
        // Reuse the final-pose continuity node after IK as well as the slot's
        // inertialization, preserving hands/feet from the last evaluated frame.
        PoseContinuityTime = Duration;
        ++RollPoseSerial;
    }
    FMontageBlendSettings Settings(Duration);
    Settings.BlendMode = EMontageBlendMode::Inertialization;
    // UE stops montage evaluation/root extraction immediately, while the graph
    // decays the previous pose into newly evaluated locomotion over Duration.
    Montage_StopWithBlendSettings(Settings, ActionMontage);
    ConsumeExtractedRootMotion(1.f);
    if (auto* Character = Cast<ACharacter>(TryGetPawnOwner()))
        Character->GetCharacterMovement()->RootMotionParams.Clear();
}
bool UCleanMotionAnimInstance::IsCleanActionActive() const
{
    const auto* Instance = LatestActionInstance(MontageInstances, ActionMontage);
    return Instance && Instance->IsValid() && (Instance->IsActive() || Instance->GetWeight() > SMALL_NUMBER);
}
bool UCleanMotionAnimInstance::IsUpperBodyActionActive() const
{
    return IsCleanActionActive() && ActionMontage &&
        ActionMontage->SlotAnimTracks.ContainsByPredicate([](const FSlotAnimationTrack& Track)
        { return Track.SlotName == TEXT("CleanUpperBody"); });
}
float UCleanMotionAnimInstance::GetCleanActionProgress() const
{
    const auto* Instance = LatestActionInstance(MontageInstances, ActionMontage);
    return Instance && Instance->IsValid() && ActionMontage->GetPlayLength() > 0.f
               ? FMath::Clamp(Instance->GetPosition() / ActionMontage->GetPlayLength(), 0.f, 1.f)
               : 1.f;
}

bool UCleanMotionAnimInstance::IsAnyMontageActiveIncludingBlendOut() const
{
    for (const auto* Instance : MontageInstances)
        if (Instance && Instance->IsValid() && (Instance->IsActive() || Instance->GetWeight() > SMALL_NUMBER))
            return true;
    return false;
}
