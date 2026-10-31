#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CleanNetworkCharacter.generated.h"

// Engine prediction/replication adapter. Action rules and input remain in AS.
UCLASS()
class FCS3C_API UCleanNetworkMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    bool bWantsWalk = false;
    bool bWantsSprint = false;
    float WalkSpeed = 300.f;
    float RunSpeed = 500.f;
    float SprintSpeed = 700.f;
    // Generic translation transform; script owns direction and lifetime.
    float RootMotionTranslationYaw = 0.f;
    FTransform TransformRootMotionTranslation(const FTransform& Motion, UCharacterMovementComponent* Movement, float DeltaSeconds);
    virtual float GetMaxSpeed() const override;
    virtual float GetMaxAcceleration() const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    virtual void PhysicsRotation(float DeltaTime) override;
    virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
};

UCLASS()
class FCS3C_API ACleanNetworkCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ACleanNetworkCharacter(const FObjectInitializer& ObjectInitializer);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintImplementableEvent) void InitializeCleanCharacter();
    UFUNCTION(BlueprintImplementableEvent) void TickCleanCharacter(float DeltaSeconds);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION(BlueprintCallable, Category="3C Network")
    void SetNetworkLocomotion(bool Walk, bool Sprint, float WalkSpeed, float RunSpeed, float SprintSpeed);
    UFUNCTION(BlueprintCallable, Category="3C Network") void SetNetworkActionLock(bool Locked, float Yaw, bool LockMovement = true);
    UFUNCTION(BlueprintCallable, Category="3C Network") void BeginNetworkRollRecovery(float TurnRate);
    UFUNCTION(BlueprintCallable, Category="Root Motion") void SetRootMotionTranslationYaw(float WorldDeltaYaw);
    UFUNCTION(BlueprintCallable, Category="Root Motion") void SetCleanRootMotionScale(float Scale) { SetAnimRootMotionTranslationScale(Scale); }
    UFUNCTION(BlueprintPure, Category="3C Network") float GetNetworkTime() const;
    UFUNCTION(BlueprintPure, Category="3C Network") float GetNetworkMaxSpeed() const;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="3C Network") float NetworkGaitSpeed = 500.f;
    // Script supplies the authoritative gameplay scale on every replica; saved moves only carry gait flags.
    UPROPERTY(BlueprintReadWrite, Category="3C Network") float GameplayMovementMultiplier = 1.f;
    UPROPERTY(BlueprintReadOnly, Category="3C Network") bool bNetworkActionLocked = false;
    UPROPERTY(BlueprintReadOnly, Category="3C Network") bool bNetworkMovementLocked = false;
    float NetworkLockedYaw = 0.f;
    float NetworkRecoveryUntil = 0.f;
    float NetworkRecoveryTurnRate = 720.f;
};
