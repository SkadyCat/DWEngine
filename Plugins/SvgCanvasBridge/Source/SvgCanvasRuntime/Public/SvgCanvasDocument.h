#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SvgCanvasDocument.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct SVGCANVASRUNTIME_API FSvgCanvasRaster
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Texture = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D PixelOrigin = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D PixelSize = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString SourceSvg;
};

USTRUCT(BlueprintType)
struct SVGCANVASRUNTIME_API FSvgCanvasNodeState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString ParentId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector4 Rect = FVector4(0, 0, 0, 0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) double LayoutScale = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Visibility = TEXT("collapsed");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString LocalVisibility = TEXT("visible");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Raster;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString HoveredRaster;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString PressedRaster;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DisabledRaster;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool Interactable = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Text;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 PaintOrder = 0;
};

USTRUCT(BlueprintType)
struct SVGCANVASRUNTIME_API FSvgCanvasSceneState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FSvgCanvasNodeState> Nodes;
};

UCLASS(BlueprintType)
class SVGCANVASRUNTIME_API USvgCanvasDocument : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString ProjectJson;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString SourceSvg;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FString SourceHash;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") FVector2D DesignSize = FVector2D(1600, 900);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Source") TMap<FString, FString> SvgAssets;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Render Cache") TMap<FString, FSvgCanvasRaster> Rasters;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG States") TMap<FString, FSvgCanvasSceneState> States;
};
