#pragma once
#include "CoreMinimal.h"
#include "OpusWorldUI.h"
#include "OpusMapUI.generated.h"

// Primitive presentation data. No progression, status or map-rule decisions.
USTRUCT(BlueprintType)
struct FOpusPlotPoint
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FVector2D Position=FVector2D::ZeroVector;
 UPROPERTY(BlueprintReadWrite) float Radius=0;
 UPROPERTY(BlueprintReadWrite) FString Fill;
 UPROPERTY(BlueprintReadWrite) FString Stroke;
 UPROPERTY(BlueprintReadWrite) float Width=1;
 UPROPERTY(BlueprintReadWrite) float Ring=0;
 UPROPERTY(BlueprintReadWrite) FString RingColor;
 UPROPERTY(BlueprintReadWrite) FString Glow;
 UPROPERTY(BlueprintReadWrite) int32 Glyph=-1;
 UPROPERTY(BlueprintReadWrite) float GlyphRadius=0;
 UPROPERTY(BlueprintReadWrite) float Angle=0;
 UPROPERTY(BlueprintReadWrite) FString Label;
 UPROPERTY(BlueprintReadWrite) FString LabelColor;
 UPROPERTY(BlueprintReadWrite) float FogRadius=0;
 UPROPERTY(BlueprintReadWrite) float FogStrength=0;
};
USTRUCT(BlueprintType)
struct FOpusPlotRect
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FVector4 Rect=FVector4(0,0,0,0);
 UPROPERTY(BlueprintReadWrite) FString Fill;
};
USTRUCT(BlueprintType)
struct FOpusPlotPath
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) TArray<FVector2D> Points;
 UPROPERTY(BlueprintReadWrite) FString Color;
 UPROPERTY(BlueprintReadWrite) float Width=1;
 UPROPERTY(BlueprintReadWrite) float Dash=0;
};

// Generic input/layout adapters for saved UICreator controls. AS owns maps.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusMapUI : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) static void Fit(UOpusWorldScreen* Screen,APlayerController* Player);
 UFUNCTION(BlueprintPure) static bool MouseDown();
 UFUNCTION(BlueprintPure) static USvgCanvasNode* Hit(USvgCanvasScreen* Screen,APlayerController* Player);
 UFUNCTION(BlueprintPure) static FVector4 Bounds(USvgCanvasScreen* Screen,const FString& Binding);
 UFUNCTION(BlueprintCallable) static void Arrange(USvgCanvasScreen* Screen,const FString& Binding,const TArray<FVector4>& Rects,const TArray<float>& Angles);
 UFUNCTION(BlueprintCallable) static void Plot(UOpusWorldScreen* Screen,const FString& Id,const TArray<FOpusPlotPoint>& Points,const TArray<FOpusPlotPath>& Paths,FVector2D Center,float Zoom);
 UFUNCTION(BlueprintCallable) static void Rectangles(UOpusWorldScreen* Screen,const FString& Id,const TArray<FOpusPlotRect>& Rectangles);
};
