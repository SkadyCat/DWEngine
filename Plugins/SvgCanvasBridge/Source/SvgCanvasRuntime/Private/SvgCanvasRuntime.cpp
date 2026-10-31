#include "SvgCanvasNode.h"
#include "SvgCanvasScreen.h"
#include "SvgCanvasMotionNode.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Modules/ModuleManager.h"
#include "InputCoreTypes.h"
#include "Engine/Texture2D.h"
#include "UObject/UnrealType.h"
void StartSvgCanvasAudioVerification();
class FSvgCanvasRuntimeModule : public FDefaultModuleImpl
{
    virtual void StartupModule() override { StartSvgCanvasAudioVerification(); }
};
IMPLEMENT_MODULE(FSvgCanvasRuntimeModule, SvgCanvasRuntime)
USvgCanvasScreen::USvgCanvasScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { SetIsFocusable(true); }
FString USvgCanvasScreen::ClosedState() const
{
    const FString State = FString::Printf(TEXT("selected-%d"), SelectedSlot + 1);
    return SourceDocument && SourceDocument->States.Contains(State) ? State : TEXT("canvas");
}
bool USvgCanvasScreen::HandleInputKey(FKey Key)
{
    if(InputFilter.IsBound()&&InputFilter.Execute(Key))return true;
    const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
    for (int32 Index = 0; Index < 4; Index++) if (Key == Digits[Index])
    {
        const bool Changed = SelectedSlot != Index; SelectedSlot = Index; const bool Applied = ApplySceneState(ClosedState());
        if (Changed && Applied) for (const auto& Pair : SourceNodes) if (USvgCanvasMotionNode* Node = Cast<USvgCanvasMotionNode>(Pair.Value))
        {
            if (Node->Binding != TEXT("ability")) continue; TSharedPtr<FJsonObject> Action;
            if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Node->ActionJson), Action) && Action && Action->GetIntegerField(TEXT("value")) == Index) { Node->PlayUiSound(TEXT("selected")); break; }
        }
        return Applied;
    }
    if (Key == EKeys::Escape) return ApplySceneState(ClosedState());
    if (Key == EKeys::B || Key == EKeys::C || Key == EKeys::M) { const FString State = Key == EKeys::B ? TEXT("inventory") : Key == EKeys::C ? TEXT("character") : TEXT("atlas"); return ApplySceneState(CurrentState == State ? ClosedState() : State); }
    return false;
}
FReply USvgCanvasScreen::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.IsControlDown() || Event.IsAltDown() || Event.IsCommandDown()) return Super::NativeOnKeyDown(Geometry, Event);
    if(Event.GetKey()==EKeys::M)return Super::NativeOnKeyDown(Geometry,Event); // Opus AS owns the tower settings shortcut.
    return HandleInputKey(Event.GetKey()) ? FReply::Handled() : Super::NativeOnKeyDown(Geometry, Event);
}
FReply USvgCanvasScreen::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(InputFilter.IsBound()&&InputFilter.Execute(Event.GetKey()))return FReply::Handled();
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
FReply USvgCanvasScreen::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
    // Generic pointer delivery; the host's AS input filter owns context menus
    // and dismissal. A button must not turn a right press into its left action.
    PointerPosition=UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).AbsoluteToLocal(Event.GetScreenSpacePosition());
    TGuardValue<bool> PointerDispatch(DispatchingPointer,true);
    if(InputFilter.IsBound()&&InputFilter.Execute(Event.GetEffectingButton()))return FReply::Handled();
    return Super::NativeOnPreviewMouseButtonDown(Geometry,Event);
}

void USvgCanvasNode::ApplyRaster(const FSvgCanvasRaster& Raster)
{
    if (!Surface) return;
#if WITH_EDITORONLY_DATA
    // Designer promotes runtime Collapsed widgets unless this independent flag is set.
    Surface->bHiddenInDesigner = Raster.Texture == nullptr;
#endif
    if (!Raster.Texture) { Surface->SetVisibility(ESlateVisibility::Collapsed); return; }
    Surface->SetVisibility(ESlateVisibility::HitTestInvisible);
    FSlateBrush Brush = Surface->GetBrush(); Brush.SetResourceObject(Raster.Texture); Brush.ImageSize = FVector2f(Raster.PixelSize);
    Surface->SetBrush(Brush);
    if (UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Surface->Slot))
    {
        const UWidgetTree* Tree = GetTypedOuter<UWidgetTree>();
        const bool FullScreenBackground = Tree && Tree->RootWidget == this
            && Raster.PixelOrigin.Equals(FVector2D(DesignRect.X, DesignRect.Y), .01)
            && Raster.PixelSize.Equals(FVector2D(DesignRect.Z, DesignRect.W), .01);
        // The root's own background must follow the allocated screen, not its baked PNG size.
        const bool CoversNode = Raster.PixelOrigin.X <= DesignRect.X && Raster.PixelOrigin.Y <= DesignRect.Y
            && Raster.PixelOrigin.X + Raster.PixelSize.X >= DesignRect.X + DesignRect.Z
            && Raster.PixelOrigin.Y + Raster.PixelSize.Y >= DesignRect.Y + DesignRect.W;
        Canvas->SetAnchors(FullScreenBackground || CoversNode ? FAnchors(0, 0, 1, 1) : FAnchors(0, 0));
        Canvas->SetAlignment(FVector2D::ZeroVector); Canvas->SetAutoSize(false);
        if (FullScreenBackground) Canvas->SetOffsets(FMargin(0));
        else if (CoversNode) Canvas->SetOffsets(FMargin(Raster.PixelOrigin.X - DesignRect.X, Raster.PixelOrigin.Y - DesignRect.Y,
            DesignRect.X + DesignRect.Z - Raster.PixelOrigin.X - Raster.PixelSize.X, DesignRect.Y + DesignRect.W - Raster.PixelOrigin.Y - Raster.PixelSize.Y));
        else { Canvas->SetPosition(Raster.PixelOrigin - FVector2D(DesignRect.X, DesignRect.Y)); Canvas->SetSize(Raster.PixelSize); }
    }
}
void USvgCanvasNode::Hover() { ApplyRaster(Hovered.Texture ? Hovered : Normal); }
void USvgCanvasNode::RememberImportedDefaults()
{
    if (const UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Slot))
    {
        ImportedSlotOffsets = Canvas->GetOffsets(); ImportedSlotAnchors = Canvas->GetAnchors(); ImportedSlotAlignment = Canvas->GetAlignment(); ImportedZOrder = Canvas->GetZOrder();
    }
    ImportedVisibility = GetVisibility();
    if (Surface) if (const UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Surface->Slot)) ImportedSurfaceOffsets = Canvas->GetOffsets();
    ImportedNormalTexture = Normal.Texture; ImportedHoveredTexture = Hovered.Texture; ImportedPressedTexture = Pressed.Texture; ImportedDisabledTexture = Disabled.Texture;
}
void USvgCanvasNode::CaptureAuthoredOverrides()
{
    if (const UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Slot)) AuthoredLayout = Canvas->GetOffsets() != ImportedSlotOffsets || Canvas->GetAnchors() != ImportedSlotAnchors || Canvas->GetAlignment() != ImportedSlotAlignment || Canvas->GetZOrder() != ImportedZOrder;
    AuthoredVisibility = GetVisibility() != ImportedVisibility;
    AuthoredAppearance = Normal.Texture != ImportedNormalTexture || Hovered.Texture != ImportedHoveredTexture || Pressed.Texture != ImportedPressedTexture || Disabled.Texture != ImportedDisabledTexture;
    if (!Surface) return;
    if (UTexture2D* Texture = Cast<UTexture2D>(Surface->GetBrush().GetResourceObject()))
    {
        if (Texture != Normal.Texture) { Normal.Texture = Texture; AuthoredAppearance = true; }
    }
    if (const UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Surface->Slot))
    {
        if (Canvas->GetOffsets() != ImportedSurfaceOffsets)
        {
            // Preserve authored stretch margins as native margins, not as a raster size.
            if (Canvas->GetAnchors().Minimum != Canvas->GetAnchors().Maximum) { AuthoredAppearance = true; return; }
            const FVector2D OldOrigin = Normal.PixelOrigin, OldSize = Normal.PixelSize;
            Normal.PixelOrigin = FVector2D(DesignRect.X, DesignRect.Y) + Canvas->GetPosition(); Normal.PixelSize = Canvas->GetSize();
            const FVector2D Scale(OldSize.X > 0 ? Normal.PixelSize.X / OldSize.X : 1, OldSize.Y > 0 ? Normal.PixelSize.Y / OldSize.Y : 1);
            for (FSvgCanvasRaster* Raster : { &Hovered, &Pressed, &Disabled }) { Raster->PixelOrigin = Normal.PixelOrigin + (Raster->PixelOrigin - OldOrigin) * Scale; Raster->PixelSize *= Scale; }
            AuthoredAppearance = true;
        }
    }
}
#if WITH_EDITOR
void USvgCanvasNode::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    if (Event.MemberProperty && Event.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(USvgCanvasNode, Normal)) ApplyRaster(Normal);
}
#endif
void USvgCanvasNode::SetInteractionEnabled(bool Enabled)
{
    if (Input) Input->SetIsEnabled(Enabled);
    ApplyRaster(!Enabled && Disabled.Texture ? Disabled : Normal);
}
void USvgCanvasNode::Unhover() { ApplyRaster(Normal); }
void USvgCanvasNode::Press() { ApplyRaster(Pressed.Texture ? Pressed : Normal); }
void USvgCanvasNode::Release() { Hover(); }
void USvgCanvasNode::Invoke() { OnInvoked.Broadcast(this); }
void USvgCanvasNode::BindInput()
{
    if (!Input) return;
    Input->OnHovered.AddUniqueDynamic(this, &USvgCanvasNode::Hover); Input->OnUnhovered.AddUniqueDynamic(this, &USvgCanvasNode::Unhover);
    Input->OnPressed.AddUniqueDynamic(this, &USvgCanvasNode::Press); Input->OnReleased.AddUniqueDynamic(this, &USvgCanvasNode::Release); Input->OnClicked.AddUniqueDynamic(this, &USvgCanvasNode::Invoke);
}
void USvgCanvasNode::ApplyLayout(const FVector4& ParentRect, int32 PaintOrder)
{
    UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Slot); if (!Canvas) return;
    const double X = DesignRect.X - ParentRect.X, Y = DesignRect.Y - ParentRect.Y;
    const USvgCanvasNode* ParentNode = Cast<USvgCanvasNode>(GetParent());
    // Source overlay-like panels use slot alignment, not Canvas anchors. This
    // includes prefab roots and a button's label/content (such as modal shades).
    const bool OverlaySlot = ParentNode && ParentNode->SourceType != TEXT("CanvasPanel") && ParentNode->SourceType != TEXT("ScaleBox")
        && ParentNode->SourceType != TEXT("HorizontalBox") && ParentNode->SourceType != TEXT("VerticalBox") && ParentNode->SourceType != TEXT("ScrollBox");
    FAnchors NativeAnchors = SourceAnchors; FVector2D NativeAlignment = SourceAlignment;
    if (OverlaySlot)
    {
        FString Horizontal = TEXT("fill"), Vertical = TEXT("fill"); TSharedPtr<FJsonObject> Properties;
        if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SourcePropertiesJson), Properties) && Properties)
        {
            const TSharedPtr<FJsonObject>* SourceSlot = nullptr;
            if (Properties->TryGetObjectField(TEXT("slot"), SourceSlot)) { (*SourceSlot)->TryGetStringField(TEXT("horizontalAlignment"), Horizontal); (*SourceSlot)->TryGetStringField(TEXT("verticalAlignment"), Vertical); }
        }
        auto Axis = [](const FString& Alignment, float& Min, float& Max, double& Pivot)
        {
            Min = Alignment == TEXT("right") || Alignment == TEXT("bottom") ? 1.f : Alignment == TEXT("center") ? .5f : 0.f;
            Max = Alignment == TEXT("fill") ? 1.f : Min; Pivot = Min;
        };
        float MinX, MaxX, MinY, MaxY; double PivotX, PivotY;
        Axis(Horizontal, MinX, MaxX, PivotX); Axis(Vertical, MinY, MaxY, PivotY);
        NativeAnchors = FAnchors(MinX, MinY, MaxX, MaxY); NativeAlignment = FVector2D(PivotX, PivotY);
    }
    const bool StretchX = NativeAnchors.Minimum.X != NativeAnchors.Maximum.X;
    const bool StretchY = NativeAnchors.Minimum.Y != NativeAnchors.Maximum.Y;
    Canvas->SetAnchors(NativeAnchors); Canvas->SetAlignment(NativeAlignment); Canvas->SetAutoSize(false);
    Canvas->SetOffsets(FMargin(
        X - ParentRect.Z * NativeAnchors.Minimum.X + (StretchX ? 0 : DesignRect.Z * NativeAlignment.X),
        Y - ParentRect.W * NativeAnchors.Minimum.Y + (StretchY ? 0 : DesignRect.W * NativeAlignment.Y),
        StretchX ? ParentRect.Z * NativeAnchors.Maximum.X - X - DesignRect.Z : DesignRect.Z,
        StretchY ? ParentRect.W * NativeAnchors.Maximum.Y - Y - DesignRect.W : DesignRect.W));
    Canvas->SetZOrder(PaintOrder);
}
void USvgCanvasScreen::IndexNodes()
{
    SourceNodes.Reset(); if (!WidgetTree) return;
    WidgetTree->ForEachWidget([&](UWidget* Widget) { if (USvgCanvasNode* Node = Cast<USvgCanvasNode>(Widget)) { SourceNodes.Add(Node->SourceId, Node); Node->CaptureAuthoredOverrides(); Node->BindInput(); Node->OnInvoked.AddUniqueDynamic(this, &USvgCanvasScreen::HandleInvoke); } });
}
void USvgCanvasScreen::NativeConstruct() { Super::NativeConstruct(); IndexNodes(); CurrentState = TEXT("canvas"); if (InitialState != TEXT("canvas")) ApplySceneState(InitialState); }
USvgCanvasNode* USvgCanvasScreen::FindSourceNode(const FString& Id) const { const auto* Result = SourceNodes.Find(Id); return Result ? Result->Get() : nullptr; }
bool USvgCanvasScreen::ApplySceneState(const FString& State)
{
    if (!SourceDocument) return false;
    const FSvgCanvasSceneState* Scene = SourceDocument->States.Find(State); if (!Scene) return false;
    if (State.StartsWith(TEXT("selected-")) || State.StartsWith(TEXT("picker-"))) SelectedSlot = FMath::Clamp(FCString::Atoi(*State.RightChop(State.Find(TEXT("-")) + 1)) - 1, 0, 3);
    if (SourceNodes.IsEmpty()) IndexNodes();
    TMap<FString, FVector4> Rects; for (const auto& Value : Scene->Nodes) { const USvgCanvasNode* Existing = FindSourceNode(Value.Id); Rects.Add(Value.Id, Value.Visibility == TEXT("visible") || !Existing ? Value.Rect : Existing->DesignRect); }
    for (const auto& Value : Scene->Nodes)
    {
        USvgCanvasNode* Node = FindSourceNode(Value.Id); if (!Node) return false;
        const FVector2D Delta(Rects[Value.Id].X - Node->DesignRect.X, Rects[Value.Id].Y - Node->DesignRect.Y);
        if (Node->AuthoredAppearance) for (FSvgCanvasRaster* Raster : { &Node->Normal, &Node->Hovered, &Node->Pressed, &Node->Disabled }) Raster->PixelOrigin += Delta;
        Node->DesignRect = Rects[Value.Id]; Node->DesignScale = Value.LayoutScale; Node->SourceText = Value.Text;
        if (!Node->AuthoredVisibility) Node->SetVisibility(Value.LocalVisibility == TEXT("visible") ? ESlateVisibility::SelfHitTestInvisible : Value.LocalVisibility == TEXT("hidden") ? ESlateVisibility::Hidden : ESlateVisibility::Collapsed);
        if (Value.Visibility != TEXT("visible")) continue;
        if (!Node->AuthoredLayout) if (const FVector4* Parent = Rects.Find(Value.ParentId)) Node->ApplyLayout(*Parent, Value.PaintOrder);
        if (Node->AuthoredAppearance) { if (Node->Input) Node->SetInteractionEnabled(Value.Interactable); continue; }
        const FSvgCanvasRaster* Raster = SourceDocument->Rasters.Find(Value.Raster); Node->Normal = Raster ? *Raster : FSvgCanvasRaster(); Node->ApplyRaster(Node->Normal);
        auto Brush = [&](const FString& Key) { const FSvgCanvasRaster* Found = SourceDocument->Rasters.Find(Key); return Found ? *Found : FSvgCanvasRaster(); };
        Node->Hovered = Brush(Value.HoveredRaster); Node->Pressed = Brush(Value.PressedRaster); Node->Disabled = Brush(Value.DisabledRaster);
        if (Node->Input) Node->SetInteractionEnabled(Value.Interactable);
    }
    for (const auto& Pair : SourceNodes) if (Pair.Value->SourceType == TEXT("ScaleBox")) Pair.Value->InvalidateLayoutAndVolatility();
    CurrentState = State; return true;
}
void USvgCanvasScreen::HandleInvoke(USvgCanvasNode* Node)
{
    SourceActionHandled=false;OnSourceAction.Broadcast(Node);if(SourceActionHandled)return;
    TSharedPtr<FJsonObject> Action; if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Node->ActionJson), Action) || !Action) return;
    FString Type; Action->TryGetStringField(TEXT("type"), Type);
    if (Type == TEXT("close")) { ApplySceneState(ClosedState()); return; }
    if (Type == TEXT("ability"))
    {
        const int32 NextSlot = FMath::Clamp(Action->GetIntegerField(TEXT("value")), 0, 3); const bool Changed = SelectedSlot != NextSlot; SelectedSlot = NextSlot;
        const FString State = FString::Printf(TEXT("picker-%d"), SelectedSlot + 1); const bool Applied = ApplySceneState(CurrentState == State ? ClosedState() : State);
        if (Changed && Applied) if (USvgCanvasMotionNode* MotionNode = Cast<USvgCanvasMotionNode>(Node)) MotionNode->PlayUiSound(TEXT("selected"));
        return;
    }
    auto Navigate = [&](FString Value)
    {
        if (Value == TEXT("normal")) Value = ClosedState();
        if (Value == TEXT("skills")) Value = FString::Printf(TEXT("picker-%d"), SelectedSlot + 1);
        if (Value == TEXT("map")) Value = TEXT("map-equipment");
        if (Value == TEXT("lineage")) Value = TEXT("map-lineage");
        ApplySceneState(CurrentState == Value ? ClosedState() : Value);
    };
    if (Type == TEXT("function")) { FString Value; Action->TryGetStringField(TEXT("value"), Value); Navigate(Value); return; }
    if (Type == TEXT("battle-ui"))
    {
        FString Op, Value; Action->TryGetStringField(TEXT("op"), Op); Action->TryGetStringField(TEXT("value"), Value);
        if (Op == TEXT("navigate")) Navigate(Value);
        else if (Op == TEXT("back")) ApplySceneState(ClosedState());
        return;
    }
    if (Type == TEXT("build")) { Navigate(SourceDocument && SourceDocument->States.Contains(TEXT("builds")) ? TEXT("builds") : TEXT("bd")); return; }
    if (Type == TEXT("bd")) ApplySceneState(CurrentState == TEXT("bd") ? ClosedState() : TEXT("bd"));
    if (Type == TEXT("map-tab")) { FString Value; Action->TryGetStringField(TEXT("value"), Value); ApplySceneState(TEXT("map-") + Value); }
}
