#include "SvgCanvasEdgeImage.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

class SSvgCanvasEdgeImage : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSvgCanvasEdgeImage) {} SLATE_END_ARGS()
    void Construct(const FArguments&) { SetCanTick(false); }
    FSvgMotionEdge Edge;
    FLinearColor Tint = FLinearColor::White;
    float Phase = 0, Opacity = 0;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        if (Opacity <= 0) return Layer;
        const FVector2D Size = G.GetLocalSize();
        const float W=Size.X, H=Size.Y, Thickness=FMath::Min(Edge.Width, FMath::Min(W,H)*.5f);
        if (W<=0 || H<=0 || Thickness<=0) return Layer;
        const float R=FMath::Clamp(Edge.Radius-Thickness*.5f,0.f,FMath::Min(W,H)*.5f-Thickness*.5f);
        const float X=W-Thickness-2*R, Y=H-Thickness-2*R, Arc=R*PI*.5f, Perimeter=2*(X+Y)+4*Arc;
        auto Point=[&](float Distance,FVector2f& P,FVector2f& Normal)
        {
            const float O=Thickness*.5f; float D=Distance;
            const FVector2f Centers[4]={{W-O-R,O+R},{W-O-R,H-O-R},{O+R,H-O-R},{O+R,O+R}};
            for(int32 Side=0;Side<4;++Side)
            {
                const float Length=(Side%2)?Y:X;
                if(D<=Length)
                {
                    if(Side==0){P={O+R+D,O};Normal={0,-1};}
                    else if(Side==1){P={W-O,O+R+D};Normal={1,0};}
                    else if(Side==2){P={W-O-R-D,H-O};Normal={0,1};}
                    else {P={O,H-O-R-D};Normal={-1,0};}return;
                }
                D-=Length;
                if(Arc>0&&D<=Arc){const float A=(-.5f+Side*.5f)*PI+D/R;Normal={FMath::Cos(A),FMath::Sin(A)};P=Centers[Side]+Normal*R;return;}
                D-=Arc;
            }
            P={O+R,O};Normal={0,-1};
        };
        const FLinearColor Base=Edge.Color*Tint;
        auto ColorAt=[&](const FVector2f& P)
        {
            float Angle=FMath::RadiansToDegrees(FMath::Atan2(P.X-W*.5f,-(P.Y-H*.5f)));
            Angle=FMath::Fmod(Angle-Phase+1080.f,360.f);
            FLinearColor Color=Base;float Alpha=0;
            if(Angle>=205&&Angle<245)Alpha=.3f*(Angle-205)/40;
            else if(Angle>=245&&Angle<285)Alpha=.3f+.7f*(Angle-245)/40;
            else if(Angle>=285&&Angle<306){Alpha=1;Color=FMath::Lerp(Base,FLinearColor::FromSRGBColor(FColor(245,251,255)),(Angle-285)/21);}
            else if(Angle>=306&&Angle<324){Color=FLinearColor::FromSRGBColor(FColor(245,251,255));Alpha=1-(Angle-306)/18;}
            Color.A=Alpha*Opacity*Style.GetColorAndOpacityTint().A;return Color.ToFColorSRGB();
        };
        TArray<FSlateVertex> V;TArray<SlateIndex> Indices;
        constexpr int32 Segments=128;V.Reserve((Segments+1)*2);Indices.Reserve(Segments*6);
        for(int32 I=0;I<=Segments;++I)
        {
            FVector2f P,N;Point(Perimeter*I/Segments,P,N);const FColor Color=ColorAt(P);
            V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),P+N*Thickness*.5f,FVector2f(.5f,.5f),Color));
            V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),P-N*Thickness*.5f,FVector2f(.5f,.5f),Color));
            if(I<Segments){const SlateIndex A=I*2;Indices.Append({A,SlateIndex(A+1),SlateIndex(A+2),SlateIndex(A+1),SlateIndex(A+3),SlateIndex(A+2)});}
        }
        const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
        FSlateDrawElement::MakeCustomVerts(Out,Layer,Resource,V,Indices,nullptr,0,0);return Layer;
    }
};
TSharedRef<SWidget> USvgCanvasEdgeImage::RebuildWidget(){SAssignNew(EdgeWidget,SSvgCanvasEdgeImage);return EdgeWidget.ToSharedRef();}
void USvgCanvasEdgeImage::ReleaseSlateResources(bool Children){Super::ReleaseSlateResources(Children);EdgeWidget.Reset();}
void USvgCanvasEdgeImage::SetFrame(const FSvgMotionEdge& Settings,const FLinearColor& Tint,float Phase,float Opacity)
{
    if(EdgeWidget){EdgeWidget->Edge=Settings;EdgeWidget->Tint=Tint;EdgeWidget->Phase=Phase;EdgeWidget->Opacity=Opacity;EdgeWidget->Invalidate(EInvalidateWidgetReason::Paint);}
}
