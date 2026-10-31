#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SvgCanvasMotionAsset.generated.h"

struct FSvgMotionFrame
{
    float T = 0, X = 0, Y = 0, ScaleX = 1, ScaleY = 1, Rotation = 0, Opacity = 1;
};
struct FSvgMotionClip
{
    float Duration = 0;
    bool Loop = false, FromCurrent = false;
    FString Easing;
    TArray<FSvgMotionFrame> Frames;
    FSvgMotionFrame Sample(float TimeSeconds, const FSvgMotionFrame& Start) const;
};
struct FSvgMotionEdge
{
    float Width = 1.5f, Radius = 9;
    FLinearColor Color = FLinearColor::White;
};

// Versioned, reusable authoring data. No effect-specific native classes or Blueprint graphs.
UCLASS(BlueprintType)
class SVGCANVASRUNTIME_API USvgCanvasMotionAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SVG Motion") FString SourceAssetId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SVG Motion") FString SourceAssetName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG Motion", meta=(MultiLine=true)) FString DefinitionJson;
    bool Decode(FString& Error);
    const FSvgMotionClip* FindClip(const FString& Trigger, const FString& Previous = FString()) const;
    bool VisualOnly = false;
    bool EdgeOnly = false;
    FSvgMotionEdge Edge;
private:
    FString DecodedJson;
    TMap<FString, FSvgMotionClip> Clips;
    TMap<FString, FString> Bindings;
};
