// Presentation references only. Lua selects directions; CMC consumes root motion.
struct FCleanRollClip
{
    UPROPERTY(EditAnywhere) UAnimSequence Animation;
    // Authored travel angle in actor space after the mesh's -90 degree yaw.
    UPROPERTY(EditAnywhere) float MotionYaw = 0;
}

class UCleanRollAnimationSet : UDataAsset
{
    UPROPERTY(EditAnywhere) FString DisplayName;
    // Playback speed; 2 means half the animation duration. Runtime range 0.1..4.
    UPROPERTY(EditAnywhere, Category = "Tuning") float PlayRate = 1;
    // Root-motion distance multiplier, independent of playback speed (0.1..4).
    UPROPERTY(EditAnywhere, Category = "Tuning") float DistanceScale = 1;
    UPROPERTY(EditAnywhere, Category = "Directions") FCleanRollClip Forward;
    UPROPERTY(EditAnywhere, Category = "Directions") FCleanRollClip ForwardRight;
    UPROPERTY(EditAnywhere, Category = "Directions") FCleanRollClip Right;
    UPROPERTY(EditAnywhere, Category = "Directions") FCleanRollClip BackRight;
    UPROPERTY(EditAnywhere, Category = "Directions") FCleanRollClip Back;
    UPROPERTY(EditAnywhere, Category = "Directions") FCleanRollClip BackLeft;
    UPROPERTY(EditAnywhere, Category = "Directions") FCleanRollClip Left;
    UPROPERTY(EditAnywhere, Category = "Directions") FCleanRollClip ForwardLeft;
}
