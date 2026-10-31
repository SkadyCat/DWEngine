#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "Animation/TrajectoryTypes.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "CleanMotionAnimInstance.generated.h"
class UPoseSearchDatabase;
class UAnimSequence;
class UAnimMontage;
struct FAnimNode_MotionMatching;
struct FAnimNode_SequencePlayer;
struct FAnimNode_PoseSearchHistoryCollector;
struct FAnimNode_FootPlacement;
UCLASS(Transient, Blueprintable)
class FCS3C_API UCleanMotionAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
  public:
    UCleanMotionAnimInstance();
    // Engine montage adapter. Input, combo windows and action rules live in AngelScript.
    UFUNCTION(BlueprintCallable, Category = "Actions")
    bool PlayCleanAction(UAnimSequence *Sequence, float Rate = 1.f, float StartTime = 0.f, bool UpperBody = false);
    UFUNCTION(BlueprintCallable, Category = "Weapon Layer") void SetWeaponLayer(UAnimSequence* Pose, float Weight);
    UPROPERTY(BlueprintReadOnly, Category = "Weapon Layer") TObjectPtr<UAnimSequence> WeaponPose;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon Layer") float WeaponLayerAlpha = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon Layer") FString EvaluatedWeaponPose;
    UFUNCTION(BlueprintCallable, Category = "Actions") void StopCleanAction(float BlendOut = .1f);
    UFUNCTION(BlueprintCallable, Category = "Actions") void CancelCleanActionForMovement();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actions", meta=(ClampMin="0.06", ClampMax="0.3"))
    float MovementCancelBlendTime = .12f;
    UPROPERTY(EditDefaultsOnly, Category = "Actions") TArray<TObjectPtr<UAnimMontage>> NetworkActionMontages;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actions", meta=(ClampMin="0.06", ClampMax="0.4"))
    float RollFacingBlendTime = .24f;
    uint32 GetRollPoseSerial() const { return RollPoseSerial; }
    float GetPoseContinuityTime() const { return PoseContinuityTime; }
    UFUNCTION(BlueprintPure, Category = "Actions") bool IsCleanActionActive() const;
    UFUNCTION(BlueprintPure, Category = "Actions") bool IsUpperBodyActionActive() const;
    UFUNCTION(BlueprintPure, Category="Clean Actions") bool IsAnyMontageActiveIncludingBlendOut() const;
    UFUNCTION(BlueprintPure, Category = "Actions") float GetCleanActionProgress() const;
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    // Actorless clients feed the same character graph, chooser and montage slots.
    // Transform/velocity remain owned by their movement system, never by animation.
    void SetExternalMotion(const FTransform& Body, const FVector& Velocity, const FVector& Intent, float MaxSpeed);
    UPROPERTY(BlueprintReadOnly, Category = "3C") bool bExternalMotion = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot IK") bool bEnableFootIK = true;
    UPROPERTY(BlueprintReadOnly, Category = "Foot IK") bool bFootIKActive = false;
    UPROPERTY(BlueprintReadOnly, Category = "3C") float GroundSpeed = 0;
    UPROPERTY(BlueprintReadOnly, Category = "3C") float MovementDirection = 0;
    UPROPERTY(BlueprintReadOnly, Category = "3C") bool bCrouched = false;
    UPROPERTY(BlueprintReadOnly, Category = "3C") bool bInAir = false;
    UPROPERTY(BlueprintReadOnly, Category = "3C")
    EOffsetRootBoneMode OffsetTranslationMode = EOffsetRootBoneMode::Release;
    UPROPERTY(BlueprintReadOnly, Category = "3C") float OffsetTranslationHalfLife = .1f;
    UPROPERTY(BlueprintReadOnly, Category = "3C") float MotionBlendTime = 0.2f;
    // Screen-relative input does not turn with the mouse/controller yaw.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Matching") bool bIndependentAimTrajectory = true;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") FVector PredictedVelocity;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") FString SelectedAnimation;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") bool bSelectedMirrored = false;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") FVector SelectedLocalVelocity;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") FString SelectedDatabase;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") int32 SelectedPose = -1;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") float SelectedTime = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") int32 TrajectorySamples = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") int32 UpdateCount = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Motion Matching") TArray<FString> CandidateDatabases;
    UPROPERTY(EditDefaultsOnly, Category = "Motion Matching")
    TArray<TObjectPtr<UPoseSearchDatabase>> Databases;
    UPROPERTY(EditDefaultsOnly, Category = "3C") float HeavyLandSpeedThreshold = 0.f;

  private:
    FTransform ExternalBody = FTransform::Identity;
    FVector ExternalVelocity = FVector::ZeroVector, ExternalIntent = FVector::ZeroVector;
    float ExternalMaxSpeed = 0.f;
    uint32 RollPoseSerial = 0;
    float PoseContinuityTime = .24f;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> PendingWeaponPose;
    float TargetWeaponWeight = 0.f;
    FAnimNode_SequencePlayer* WeaponPlayer = nullptr;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActionMontage;
    bool bActionUsesAuthoredFeet = false;
    FAnimNode_MotionMatching *Matching = nullptr;
    FAnimNode_PoseSearchHistoryCollector *History = nullptr;
    FAnimNode_FootPlacement* FootPlacement = nullptr;
    FTransformTrajectory Trajectory;
    FPoseSearchTrajectoryData TrajectoryData;
    float PreviousControllerYaw = 0;
    bool bPreviousInAir = false;
    float PreviousVerticalSpeed = 0;
    int32 PreviousGait = 1;
    int32 PreviousMovementState = 0;
};
