#include "CleanNetworkCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

class FCleanSavedMove : public FSavedMove_Character
{
public:
    bool bWalk = false, bSprint = false;
    float MotionYaw = 0.f;
    virtual void Clear() override { Super::Clear(); bWalk = bSprint = false; MotionYaw = 0.f; }
    using Super = FSavedMove_Character;
    virtual uint8 GetCompressedFlags() const override
    {
        return Super::GetCompressedFlags() | (bWalk ? FLAG_Custom_0 : 0) | (bSprint ? FLAG_Custom_1 : 0);
    }
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
    {
        const auto* Other = static_cast<const FCleanSavedMove*>(NewMove.Get());
        return bWalk == Other->bWalk && bSprint == Other->bSprint && MotionYaw == Other->MotionYaw && Super::CanCombineWith(NewMove, Character, MaxDelta);
    }
    virtual void SetMoveFor(ACharacter* C, float Dt, FVector const& Accel, FNetworkPredictionData_Client_Character& Data) override
    {
        Super::SetMoveFor(C, Dt, Accel, Data);
        const auto* M = CastChecked<UCleanNetworkMovement>(C->GetCharacterMovement());
        bWalk = M->bWantsWalk; bSprint = M->bWantsSprint;
        MotionYaw = M->RootMotionTranslationYaw;
    }
    virtual void PrepMoveFor(ACharacter* C) override
    {
        Super::PrepMoveFor(C);
        auto* M = CastChecked<UCleanNetworkMovement>(C->GetCharacterMovement());
        M->bWantsWalk = bWalk; M->bWantsSprint = bSprint;
        M->RootMotionTranslationYaw = MotionYaw;
    }
};
class FCleanPredictionData : public FNetworkPredictionData_Client_Character
{
public:
    explicit FCleanPredictionData(const UCharacterMovementComponent& M) : FNetworkPredictionData_Client_Character(M) {}
    virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FCleanSavedMove()); }
};
FNetworkPredictionData_Client* UCleanNetworkMovement::GetPredictionData_Client() const
{
    if (!ClientPredictionData)
        const_cast<UCleanNetworkMovement*>(this)->ClientPredictionData = new FCleanPredictionData(*this);
    return ClientPredictionData;
}
void UCleanNetworkMovement::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsWalk = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bWantsSprint = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
    if (const auto* C = Cast<ACleanNetworkCharacter>(CharacterOwner); C && C->bNetworkActionLocked)
        CharacterOwner->bPressedJump = false;
}
float UCleanNetworkMovement::GetMaxSpeed() const
{
    const auto* C = Cast<ACleanNetworkCharacter>(CharacterOwner);
    if (!C) return Super::GetMaxSpeed();
    if (C->bNetworkMovementLocked) return 0.f;
    if (IsCrouching()) return MaxWalkSpeedCrouched * FMath::Max(0.f, C->GameplayMovementMultiplier);
    if (C->GetLocalRole() == ROLE_SimulatedProxy) return C->NetworkGaitSpeed;
    // Validate the sprint cone on the server too; transmitted flags cannot enable backward sprint.
    const FVector Direction = Acceleration.GetSafeNormal2D();
    const bool Forward = FVector::DotProduct(Direction, C->GetActorForwardVector()) >= .499f;
    return (bWantsSprint && Forward ? SprintSpeed : (bWantsWalk ? WalkSpeed : RunSpeed)) * FMath::Max(0.f, C->GameplayMovementMultiplier);
}
float UCleanNetworkMovement::GetMaxAcceleration() const
{
    const auto* C = Cast<ACleanNetworkCharacter>(CharacterOwner);
    return C && C->bNetworkMovementLocked ? 0.f : Super::GetMaxAcceleration();
}
void UCleanNetworkMovement::PhysicsRotation(float Dt)
{
    auto* C = Cast<ACleanNetworkCharacter>(CharacterOwner);
    if (!C || !C->GetController() || C->GetLocalRole() == ROLE_SimulatedProxy) return;
    float Yaw = C->bNetworkActionLocked ? C->NetworkLockedYaw : C->GetControlRotation().Yaw;
    if (!C->bNetworkActionLocked && C->HasAuthority() && GetWorld()->GetTimeSeconds() < C->NetworkRecoveryUntil)
    {
        const float Delta = FMath::FindDeltaAngleDegrees(C->GetActorRotation().Yaw, Yaw);
        Yaw = C->GetActorRotation().Yaw + FMath::Clamp(Delta, -C->NetworkRecoveryTurnRate * Dt, C->NetworkRecoveryTurnRate * Dt);
    }
    MoveUpdatedComponent(FVector::ZeroVector, FRotator(0.f, Yaw, 0.f).Quaternion(), true);
}
void UCleanNetworkMovement::OnMovementUpdated(float Dt, const FVector& OldLocation, const FVector& OldVelocity)
{
    Super::OnMovementUpdated(Dt, OldLocation, OldVelocity);
    if (auto* C = Cast<ACleanNetworkCharacter>(CharacterOwner); C && C->HasAuthority() && !C->bNetworkMovementLocked)
        C->NetworkGaitSpeed = GetMaxSpeed();
}
FTransform UCleanNetworkMovement::TransformRootMotionTranslation(const FTransform& Motion, UCharacterMovementComponent*, float)
{
    FTransform Result = Motion;
    Result.SetTranslation(FRotator(0.f, RootMotionTranslationYaw, 0.f).RotateVector(Motion.GetTranslation()));
    return Result;
}
void ACleanNetworkCharacter::SetRootMotionTranslationYaw(float WorldDeltaYaw)
{
    CastChecked<UCleanNetworkMovement>(GetCharacterMovement())->RootMotionTranslationYaw =
        FMath::IsFinite(WorldDeltaYaw) ? FMath::UnwindDegrees(WorldDeltaYaw) : 0.f;
}
ACleanNetworkCharacter::ACleanNetworkCharacter(const FObjectInitializer& Init)
    : Super(Init.SetDefaultSubobjectClass<UCleanNetworkMovement>(ACharacter::CharacterMovementComponentName))
{
    bReplicates = true;
    SetReplicateMovement(true);
    NetUpdateFrequency = 60.f;
    MinNetUpdateFrequency = 20.f;
    // AS handles movement/action input in the owner tick. It must run before
    // CMC can advance root-motion montages or the mesh can evaluate its pose.
    GetCharacterMovement()->bTickBeforeOwner = false;
}
void ACleanNetworkCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACleanNetworkCharacter, NetworkGaitSpeed);
}
void ACleanNetworkCharacter::SetNetworkLocomotion(bool Walk, bool Sprint, float WalkValue, float RunValue, float SprintValue)
{
    auto* M = CastChecked<UCleanNetworkMovement>(GetCharacterMovement());
    M->bWantsWalk = Walk; M->bWantsSprint = Sprint;
    M->WalkSpeed = WalkValue; M->RunSpeed = RunValue; M->SprintSpeed = SprintValue;
}
void ACleanNetworkCharacter::SetNetworkActionLock(bool Locked, float Yaw, bool LockMovement)
{
    bNetworkActionLocked = Locked;
    bNetworkMovementLocked = Locked && LockMovement;
    if (Locked) NetworkLockedYaw = Yaw;
}
void ACleanNetworkCharacter::BeginNetworkRollRecovery(float TurnRate)
{
    NetworkRecoveryUntil = GetWorld()->GetTimeSeconds() + 1.f;
    NetworkRecoveryTurnRate = FMath::Max(1.f, TurnRate);
}
float ACleanNetworkCharacter::GetNetworkTime() const
{
    const auto* State = GetWorld()->GetGameState();
    return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}
float ACleanNetworkCharacter::GetNetworkMaxSpeed() const { return GetCharacterMovement()->GetMaxSpeed(); }

void ACleanNetworkCharacter::BeginPlay()
{
    Super::BeginPlay();
    auto* Movement = GetCharacterMovement();
    // UE's conversion delegate is native-only. Preserve montage timing, distance,
    // height and rotation; script supplies the planar translation correction.
    if (ensure(!Movement->ProcessRootMotionPostConvertToWorld.IsBound()))
        Movement->ProcessRootMotionPostConvertToWorld.BindUObject(
            CastChecked<UCleanNetworkMovement>(Movement), &UCleanNetworkMovement::TransformRootMotionTranslation);
    // Remove the legacy prerequisite as well: existing Blueprint component
    // defaults may still have registered CMC-before-owner during construction.
    Movement->bTickBeforeOwner = false;
    RemoveTickPrerequisiteComponent(Movement);
    Movement->AddTickPrerequisiteActor(this);
    GetMesh()->AddTickPrerequisiteActor(this);
    InitializeCleanCharacter();
}
void ACleanNetworkCharacter::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); TickCleanCharacter(DeltaSeconds); }
