#pragma once
#include "CoreMinimal.h"
#include "Components/CanvasPanel.h"
#include "Slate/WidgetTransform.h"
#include "Components/CanvasPanelSlot.h"
#include "SvgCanvasDocument.h"
#include "SvgCanvasNode.generated.h"

class UImage;
class UButton;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSvgCanvasInvoked, class USvgCanvasNode*, Node);

// One native UMG node per source node. SVG imagery is a child, never the whole canvas.
UCLASS(BlueprintType, meta=(DisplayName="SVG Canvas Node"))
class SVGCANVASRUNTIME_API USvgCanvasNode : public UCanvasPanel
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString SourceId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString SourceName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString SourceType;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString SourcePropertiesJson;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString SourcePrefab;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString Binding;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString ActionJson;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString SourceText;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Layout") FAnchors SourceAnchors;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Layout") FMargin SourceOffsets;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Layout") FVector2D SourceAlignment;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Layout") FVector4 DesignRect;
    // Scale already baked into the per-control PNGs at export time.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Layout") double DesignScale = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Render Cache") FSvgCanvasRaster Normal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Render Cache") FSvgCanvasRaster Hovered;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Render Cache") FSvgCanvasRaster Pressed;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Render Cache") FSvgCanvasRaster Disabled;
    UPROPERTY(BlueprintAssignable, Category="SVG Events") FSvgCanvasInvoked OnInvoked;
    UPROPERTY() TObjectPtr<UImage> Surface = nullptr;
    UPROPERTY() TObjectPtr<UButton> Input = nullptr;
    UPROPERTY() FMargin ImportedSlotOffsets;
    UPROPERTY() FAnchors ImportedSlotAnchors;
    UPROPERTY() FVector2D ImportedSlotAlignment;
    UPROPERTY() int32 ImportedZOrder = 0;
    UPROPERTY() ESlateVisibility ImportedVisibility = ESlateVisibility::SelfHitTestInvisible;
    UPROPERTY() FMargin ImportedSurfaceOffsets;
    UPROPERTY() TObjectPtr<UTexture2D> ImportedNormalTexture = nullptr;
    UPROPERTY() TObjectPtr<UTexture2D> ImportedHoveredTexture = nullptr;
    UPROPERTY() TObjectPtr<UTexture2D> ImportedPressedTexture = nullptr;
    UPROPERTY() TObjectPtr<UTexture2D> ImportedDisabledTexture = nullptr;
    UPROPERTY(Transient) bool AuthoredLayout = false;
    UPROPERTY(Transient) bool AuthoredAppearance = false;
    UPROPERTY(Transient) bool AuthoredVisibility = false;
    UFUNCTION(BlueprintCallable, Category="SVG") void ApplyRaster(const FSvgCanvasRaster& Raster);
    UFUNCTION(BlueprintCallable, Category="SVG") virtual void SetInteractionEnabled(bool Enabled);
    UFUNCTION() void Hover();
    UFUNCTION() void Unhover();
    UFUNCTION() void Press();
    UFUNCTION() void Release();
    UFUNCTION() void Invoke();
    void BindInput();
    void ApplyLayout(const FVector4& ParentRect, int32 PaintOrder);
    void RememberImportedDefaults();
    void CaptureAuthoredOverrides();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
public:
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};
