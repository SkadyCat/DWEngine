#pragma once
#include "CoreMinimal.h"
#include "Components/Image.h"
#include "SvgCanvasImage.generated.h"

// PNG caches contain browser-space, premultiplied pixels. Keep that color contract
// in both Designer and the game viewport, independently of Slate's window gamma.
UCLASS(meta=(DisplayName="SVG Raster Image"))
class SVGCANVASRUNTIME_API USvgCanvasImage : public UImage
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
