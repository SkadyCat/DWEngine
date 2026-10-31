// Local presentation only. Integrate a linearly moving target exactly so a long
// frame does not change the camera's steady following distance.
class UCleanCameraFollowComponent : UActorComponent
{
    UPROPERTY() USpringArmComponent Boom;
    FVector PreviousTarget;
    FVector SmoothedTarget;
    bool bInitialized = false;

    UFUNCTION(BlueprintCallable)
    void Configure(USpringArmComponent InBoom, UCharacterMovementComponent Movement)
    {
        if (Boom == InBoom || InBoom == nullptr) return;
        Boom = InBoom;
        SetTickGroup(ETickingGroup::TG_PostPhysics);
        if (Movement != nullptr) AddTickPrerequisiteComponent(Movement);
        Boom.AddTickPrerequisiteComponent(this);
        PreviousTarget = Boom.GetWorldLocation();
        // Preserve the current view when attaching to an already running pawn.
        FRotator Rotation = Boom.GetWorldRotation();
        SmoothedTarget = Boom.GetSocketLocation(n"SpringEndpoint")
            + Rotation.ForwardVector * Boom.TargetArmLength
            - Rotation.RotateVector(Boom.SocketOffset);
        bInitialized = true;
        Boom.bEnableCameraLag = false;
    }

    void ResetAfterTeleport()
    {
        if (Boom == nullptr) return;
        PreviousTarget = Boom.GetWorldLocation();
        SmoothedTarget = PreviousTarget;
        Boom.TargetOffset = FVector::ZeroVector;
        bInitialized = true;
    }

    UFUNCTION(BlueprintOverride)
    void Tick(float DeltaSeconds)
    {
        AdvanceFollow(DeltaSeconds);
    }

    // Share the exact follow step with deterministic integration probes;
    // BlueprintOverride events cannot be called from another AS component.
    void AdvanceFollow(float DeltaSeconds)
    {
        if (Boom == nullptr || DeltaSeconds <= 0) return;
        FVector Target = Boom.GetWorldLocation();
        float Speed = Boom.CameraLagSpeed;
        if (!bInitialized || DeltaSeconds > 0.25 ||
            (Target - PreviousTarget).SizeSquared() > 1000.0 * 1000.0 || Speed <= 0)
        {
            SmoothedTarget = Target;
            bInitialized = true;
        }
        else
        {
            FVector TargetVelocity = (Target - PreviousTarget) / DeltaSeconds;
            float Decay = Math::Exp(-Speed * DeltaSeconds);
            SmoothedTarget = Target - TargetVelocity / Speed
                + (SmoothedTarget - PreviousTarget + TargetVelocity / Speed) * Decay;
        }
        PreviousTarget = Target;
        Boom.TargetOffset = SmoothedTarget - Target;
    }
}
