#pragma once
#include "CoreMinimal.h"
#include "SvgCanvasNode.h"
#include "Widgets/SWidget.h"
#include "SvgCanvasMotionAsset.h"
#include "SvgCanvasSoundAsset.h"
#include "SvgCanvasMotionNode.generated.h"

struct FSvgMotionVisual
{
    TWeakObjectPtr<UWidget> Widget;
    FWidgetTransform Transform;
    FVector2D Pivot;
    float Opacity = 1;
};

// Motion is a separate presentation layer. The source layout and native UButton do not move.
UCLASS(meta=(DisplayName="SVG Canvas Node · Motion"))
class SVGCANVASRUNTIME_API USvgCanvasMotionNode : public USvgCanvasNode
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG Motion") TObjectPtr<USvgCanvasMotionAsset> MotionAsset;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG Motion") bool MotionEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG Motion") bool MotionDismissOnHover = false;
    UPROPERTY(Transient) TObjectPtr<class UButton> MotionOwnerInput;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG Motion") FLinearColor MotionTint = FLinearColor::White;
    UFUNCTION(BlueprintCallable, Category="SVG Motion") void SetMotionEnabled(bool Enabled);
    UFUNCTION(BlueprintCallable, Category="SVG Motion") void SetMotionTint(FLinearColor Tint);
    UPROPERTY(Transient) TObjectPtr<class USvgCanvasEdgeImage> EdgeOverlay;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG UI Audio") TMap<FName, TObjectPtr<USvgCanvasSoundAsset>> SoundEvents;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG UI Audio") bool UiSoundEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG UI Audio", meta=(ClampMin="0", ClampMax="1")) float UiSoundVolume = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG UI Audio", meta=(ClampMin=".25", ClampMax="4")) float UiSoundPitch = 1;
    UPROPERTY(Transient, BlueprintReadOnly, Category="SVG UI Audio") TObjectPtr<class UAudioComponent> LastUiAudioComponent;
    UPROPERTY(Transient, BlueprintReadOnly, Category="SVG UI Audio") int32 UiSoundPlayCount = 0;
    UFUNCTION(BlueprintCallable, Category="SVG UI Audio") UAudioComponent* PlayUiSound(FName Trigger);
    UFUNCTION(BlueprintCallable, Category="SVG Motion") void PlayMotion(FName Trigger);
    UFUNCTION(BlueprintCallable, Category="SVG Motion") void StopMotion();
    virtual void SetInteractionEnabled(bool Enabled) override;
    virtual void SetVisibility(ESlateVisibility InVisibility) override;
    virtual void ReleaseSlateResources(bool ReleaseChildren) override;
    void AdvanceMotionForValidation(float DeltaTime);
    void RestoreMotionVisuals();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    bool FeedbackHovered = false, FeedbackPressed = false;
    float FeedbackTime = 0;
    FString FeedbackState;
    FSvgMotionFrame StartFrame, CurrentFrame;
    const FSvgMotionClip* ActiveClip = nullptr;
    TArray<FSvgMotionVisual> Visuals;
    TWeakPtr<SWidget> TimerWidget;
    TWeakPtr<FActiveTimerHandle> FeedbackTimer;
    bool HasFeedback() const;
    void ChangeFeedback(bool HoveredState, bool PressedState);
    void QueueFeedback();
    EActiveTimerReturnType TickFeedback(double CurrentTime, float DeltaTime);
    UFUNCTION() void FeedbackEnter();
    UFUNCTION() void FeedbackLeave();
    UFUNCTION() void FeedbackDown();
    UFUNCTION() void FeedbackUp();
    UFUNCTION() void FeedbackClick();
    UFUNCTION() void DismissMotionOnHover();
};
