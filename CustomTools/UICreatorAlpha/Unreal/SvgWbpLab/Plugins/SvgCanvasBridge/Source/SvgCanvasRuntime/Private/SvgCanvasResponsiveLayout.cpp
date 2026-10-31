#include "SvgCanvasNode.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Layout/ArrangedChildren.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FSourceLayout
{
    FMargin Padding;
    FString Horizontal = TEXT("fill"), Vertical = TEXT("fill"), Stretch = TEXT("fit");
    double Scale = 1;
    static FSourceLayout Read(const USvgCanvasNode* Node, const TCHAR* Field)
    {
        FSourceLayout Result; TSharedPtr<FJsonObject> Json;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Node->SourcePropertiesJson), Json) || !Json) return Result;
        const TSharedPtr<FJsonObject>* Value = nullptr;
        if (!Json->TryGetObjectField(Field, Value)) return Result;
        (*Value)->TryGetStringField(TEXT("horizontalAlignment"), Result.Horizontal);
        (*Value)->TryGetStringField(TEXT("verticalAlignment"), Result.Vertical);
        (*Value)->TryGetStringField(TEXT("stretch"), Result.Stretch);
        (*Value)->TryGetNumberField(TEXT("scale"), Result.Scale);
        const TSharedPtr<FJsonObject>* Padding = nullptr;
        if ((*Value)->TryGetObjectField(TEXT("padding"), Padding))
        {
            double L = 0, T = 0, R = 0, B = 0;
            (*Padding)->TryGetNumberField(TEXT("left"), L); (*Padding)->TryGetNumberField(TEXT("top"), T);
            (*Padding)->TryGetNumberField(TEXT("right"), R); (*Padding)->TryGetNumberField(TEXT("bottom"), B);
            Result.Padding = FMargin(L, T, R, B);
        }
        return Result;
    }
};

// Keep native Canvas slots and individual editable UMG controls. Only ScaleBox
// arrangement differs: the complete child subtree (images, clips and hit tests)
// receives a Slate layout scale, recalculated from the actual allocated area.
class SSvgResponsiveCanvas : public SConstraintCanvas
{
public:
    SLATE_BEGIN_ARGS(SSvgResponsiveCanvas) {} SLATE_ARGUMENT(USvgCanvasNode*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Owner = Args._Owner; Layout = FSourceLayout::Read(Args._Owner, TEXT("layout"));
        SConstraintCanvas::Construct(SConstraintCanvas::FArguments());
    }
    virtual void OnArrangeChildren(const FGeometry& Geometry, FArrangedChildren& Arranged) const override
    {
        SConstraintCanvas::OnArrangeChildren(Geometry, Arranged);
        const USvgCanvasNode* Parent = Owner.Get(); if (!Parent || Parent->SourceType != TEXT("ScaleBox")) return;
        const FMargin P = Layout.Padding * Parent->DesignScale;
        for (int32 Index = 0; Index < Parent->GetChildrenCount(); ++Index)
        {
            const USvgCanvasNode* Child = Cast<USvgCanvasNode>(Parent->GetChildAt(Index));
            if (!Child || Child->AuthoredLayout) continue;
            if (Parent->IsDesignTime()) if (const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Child->Slot))
                if (Slot->GetOffsets() != Child->ImportedSlotOffsets || Slot->GetAnchors() != Child->ImportedSlotAnchors || Slot->GetAlignment() != Child->ImportedSlotAlignment) continue;
            FSourceLayout* ChildLayout = ChildLayouts.Find(Child);
            if (!ChildLayout) ChildLayout = &ChildLayouts.Add(Child, FSourceLayout::Read(Child, TEXT("slot")));
            const FMargin M = ChildLayout->Padding * Parent->DesignScale;
            const FVector2D Available(FMath::Max(0., Geometry.GetLocalSize().X - P.Left - P.Right - M.Left - M.Right), FMath::Max(0., Geometry.GetLocalSize().Y - P.Top - P.Bottom - M.Top - M.Bottom));
            const FVector2D BakedSize(Child->DesignRect.Z, Child->DesignRect.W);
            const double Baseline = Parent->DesignScale / FMath::Max(.0001, Child->DesignScale);
            const double X = Available.X / (BakedSize.X > 0 ? BakedSize.X : 1), Y = Available.Y / (BakedSize.Y > 0 ? BakedSize.Y : 1);
            double Scale = Layout.Stretch == TEXT("fill") ? FMath::Max(X, Y) : FMath::Min(X, Y);
            if (Layout.Stretch == TEXT("user")) Scale = Layout.Scale * Baseline;
            if (Layout.Stretch == TEXT("down")) Scale = FMath::Min(Baseline, Scale);
            Scale = FMath::Max(.0001, Scale);
            auto Offset = [](const FString& Alignment, double Space, double Size)
            {
                return Alignment == TEXT("left") || Alignment == TEXT("top") ? 0. : Alignment == TEXT("right") || Alignment == TEXT("bottom") ? Space - Size : (Space - Size) / 2;
            };
            const FVector2D Position(P.Left + M.Left + Offset(ChildLayout->Horizontal, Available.X, BakedSize.X * Scale), P.Top + M.Top + Offset(ChildLayout->Vertical, Available.Y, BakedSize.Y * Scale));
            for (int32 I = 0; I < Arranged.Num(); ++I)
                if (Arranged[I].Widget == Child->GetCachedWrappedWidget() || Arranged[I].Widget == Child->GetCachedWidget())
                    Arranged[I] = Geometry.MakeChild(Arranged[I].Widget, BakedSize, FSlateLayoutTransform(Scale, Position));
        }
    }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool Enabled) const override
    {
        const USvgCanvasNode* Parent = Owner.Get();
        if (!Parent || Parent->SourceType != TEXT("ScaleBox")) return SConstraintCanvas::OnPaint(Args, Geometry, Culling, Elements, Layer, Style, Enabled);
        FArrangedChildren Arranged(EVisibility::Visible); OnArrangeChildren(Geometry, Arranged);
        const FPaintArgs ChildArgs = Args.WithNewParent(this);
        for (const FArrangedWidget& Child : Arranged.GetInternalArray())
            if (!IsChildWidgetCulled(Culling, Child)) Layer = Child.Widget->Paint(ChildArgs, Child.Geometry, Culling, Elements, Layer + 1, Style, ShouldBeEnabled(Enabled));
        return Layer;
    }
private:
    TWeakObjectPtr<USvgCanvasNode> Owner;
    FSourceLayout Layout;
    mutable TMap<const USvgCanvasNode*, FSourceLayout> ChildLayouts;
};
}

TSharedRef<SWidget> USvgCanvasNode::RebuildWidget()
{
    if (SourceType != TEXT("ScaleBox")) return Super::RebuildWidget();
    MyCanvas = SNew(SSvgResponsiveCanvas).Owner(this);
    for (UPanelSlot* PanelSlot : Slots) if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
    {
        CanvasSlot->Parent = this; CanvasSlot->BuildSlot(MyCanvas.ToSharedRef());
    }
    return MyCanvas.ToSharedRef();
}
