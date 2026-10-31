#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SvgCanvasMotionAsset.h"
#include "SvgCanvasEdgeImage.generated.h"

// One transparent, non-interactive layer. No background texture or control redraw.
UCLASS()
class SVGCANVASRUNTIME_API USvgCanvasEdgeImage : public UWidget
{
    GENERATED_BODY()
public:
    void SetFrame(const FSvgMotionEdge& Settings, const FLinearColor& Tint, float Phase, float Opacity);
    virtual void ReleaseSlateResources(bool ReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    TSharedPtr<class SSvgCanvasEdgeImage> EdgeWidget;
};
