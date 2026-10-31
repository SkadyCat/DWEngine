#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundWave.h"
#include "SvgCanvasSoundAsset.generated.h"

UCLASS(BlueprintType)
class SVGCANVASRUNTIME_API USvgCanvasSoundAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SVG UI Audio") FString SourceAssetId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SVG UI Audio") FString SourceAssetName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SVG UI Audio") FString SourceDataHash;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG UI Audio") TObjectPtr<USoundWave> Sound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG UI Audio", meta=(ClampMin="0", ClampMax="1")) float Volume = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG UI Audio", meta=(ClampMin=".25", ClampMax="4")) float Pitch = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SVG UI Audio", meta=(ClampMin="0", ClampMax="5000")) float CooldownMs = 60;
};
