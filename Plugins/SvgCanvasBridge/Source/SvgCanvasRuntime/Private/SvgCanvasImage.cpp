#include "SvgCanvasImage.h"
#include "Widgets/Images/SImage.h"
#include "Rendering/DrawElements.h"
#include "Engine/Texture2D.h"

class SSvgCanvasImage : public SImage
{
public:
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool ParentEnabled) const override
    {
        const FSlateBrush* Image = GetImageAttribute().Get();
        if (Image) if (const UTexture2D* Texture = Cast<UTexture2D>(Image->GetResourceObject())) if (Texture->SRGB) return SImage::OnPaint(Args, Geometry, Culling, Elements, Layer, Style, ParentEnabled);
        if (Image && Image->DrawAs != ESlateBrushDrawType::NoDrawType)
        {
            const FLinearColor Tint = Style.GetColorAndOpacityTint() * GetColorAndOpacityAttribute().Get().GetColor(Style) * Image->GetTint(Style);
            FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), Image,
                ESlateDrawEffect::NoGamma | ESlateDrawEffect::PreMultipliedAlpha, Tint);
        }
        return Layer;
    }
};

TSharedRef<SWidget> USvgCanvasImage::RebuildWidget()
{
    SAssignNew(MyImage, SSvgCanvasImage);
    return MyImage.ToSharedRef();
}
