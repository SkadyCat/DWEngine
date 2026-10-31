#include "SvgCanvasMotionNode.h"
#include "SvgCanvasEdgeImage.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

bool USvgCanvasMotionNode::HasFeedback() const { return MotionEnabled && MotionAsset != nullptr; }
TSharedRef<SWidget> USvgCanvasMotionNode::RebuildWidget()
{
    FString EdgeError;
    if(MotionAsset && MotionAsset->Decode(EdgeError) && MotionAsset->EdgeOnly && !EdgeOverlay)
    {
        EdgeOverlay=NewObject<USvgCanvasEdgeImage>(this);EdgeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* EdgeSlot=AddChildToCanvas(EdgeOverlay);EdgeSlot->SetAnchors(FAnchors(0,0,1,1));EdgeSlot->SetOffsets(FMargin(0));EdgeSlot->SetZOrder(1000);
    }
    TSharedRef<SWidget> Slate = Super::RebuildWidget(); TimerWidget = Slate;
    if(MotionDismissOnHover)for(UWidget* Owner=this;Owner;Owner=Owner->GetParent())if(auto* Parent=Cast<USvgCanvasNode>(Owner);Parent&&Parent->Input)
    {MotionOwnerInput=Parent->Input;MotionOwnerInput->OnHovered.AddUniqueDynamic(this,&USvgCanvasMotionNode::DismissMotionOnHover);break;}
    if (Input)
    {
        Input->OnHovered.AddUniqueDynamic(this, &USvgCanvasMotionNode::FeedbackEnter); Input->OnUnhovered.AddUniqueDynamic(this, &USvgCanvasMotionNode::FeedbackLeave);
        Input->OnPressed.AddUniqueDynamic(this, &USvgCanvasMotionNode::FeedbackDown); Input->OnReleased.AddUniqueDynamic(this, &USvgCanvasMotionNode::FeedbackUp);
        Input->OnClicked.AddUniqueDynamic(this, &USvgCanvasMotionNode::FeedbackClick);
    }
    if (MotionAsset)
    {
        FString Error; if (!MotionAsset->Decode(Error)) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return Slate; }
        // Reference/Designer rendering stays at the unanimated source appearance.
        if (!IsDesignTime() && !IsRunningCommandlet()) PlayMotion(Input && !Input->GetIsEnabled() ? TEXT("disabled") : TEXT("normal"));
    }
    return Slate;
}
void USvgCanvasMotionNode::ReleaseSlateResources(bool ReleaseChildren)
{
    if(MotionOwnerInput)MotionOwnerInput->OnHovered.RemoveDynamic(this,&USvgCanvasMotionNode::DismissMotionOnHover);MotionOwnerInput=nullptr;
    StopMotion(); TimerWidget.Reset(); FeedbackHovered = FeedbackPressed = false; FeedbackState.Empty(); Super::ReleaseSlateResources(ReleaseChildren);
}
void USvgCanvasMotionNode::QueueFeedback()
{
    if (!ActiveClip || IsDesignTime() || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("SvgCanvasNoMotion")) || FeedbackTimer.IsValid()) return;
    if (const auto Slate = TimerWidget.Pin()) FeedbackTimer = Slate->RegisterActiveTimer(0, FWidgetActiveTimerDelegate::CreateUObject(this, &USvgCanvasMotionNode::TickFeedback));
}
void USvgCanvasMotionNode::PlayMotion(FName Trigger)
{
    if (!HasFeedback()) return; FString Error; if (!MotionAsset->Decode(Error)) return;
    const FString State = Trigger.ToString();
    const FSvgMotionClip* Clip = MotionAsset->FindClip(State, FeedbackState); FeedbackState = State;
    if (!Clip) { StopMotion(); return; }
    StartFrame = CurrentFrame; FeedbackTime = 0; ActiveClip = Clip; QueueFeedback();
}
void USvgCanvasMotionNode::StopMotion()
{
    if (const auto Handle = FeedbackTimer.Pin()) if (const auto Slate = TimerWidget.Pin()) Slate->UnRegisterActiveTimer(Handle.ToSharedRef());
    FeedbackTimer.Reset(); ActiveClip = nullptr; RestoreMotionVisuals();
}
void USvgCanvasMotionNode::ChangeFeedback(bool HoveredState, bool PressedState)
{
    if (FeedbackHovered == HoveredState && FeedbackPressed == PressedState) return;
    FeedbackHovered = HoveredState; FeedbackPressed = PressedState;
    PlayMotion(PressedState ? TEXT("pressed") : HoveredState ? TEXT("hovered") : TEXT("normal"));
}
void USvgCanvasMotionNode::FeedbackEnter() { ChangeFeedback(true, false); PlayUiSound(TEXT("hovered")); }
void USvgCanvasMotionNode::FeedbackLeave() { ChangeFeedback(false, false); PlayUiSound(TEXT("unhovered")); }
void USvgCanvasMotionNode::FeedbackDown() { ChangeFeedback(FeedbackHovered, true); PlayUiSound(TEXT("pressed")); }
void USvgCanvasMotionNode::FeedbackUp() { ChangeFeedback(FeedbackHovered, false); PlayUiSound(TEXT("released")); }
void USvgCanvasMotionNode::FeedbackClick() { PlayUiSound(TEXT("clicked")); }
UAudioComponent* USvgCanvasMotionNode::PlayUiSound(FName Trigger)
{
    if (!UiSoundEnabled || UiSoundVolume <= 0 || IsDesignTime() || (Input && !Input->GetIsEnabled()) || !GetWorld() || !GetWorld()->IsGameWorld()) return nullptr;
    const TObjectPtr<USvgCanvasSoundAsset>* Found = SoundEvents.Find(Trigger); if (!Found || !*Found || !(*Found)->Sound || (*Found)->Volume <= 0) return nullptr;
    USvgCanvasSoundAsset* Asset = *Found;
    static TMap<TWeakObjectPtr<USvgCanvasSoundAsset>, double> LastPlayed;
    static TArray<TWeakObjectPtr<UAudioComponent>> Voices;
    const double Now = FPlatformTime::Seconds(); if (const double* Last = LastPlayed.Find(Asset)) if ((Now - *Last) * 1000 < Asset->CooldownMs) return nullptr;
    Voices.RemoveAll([](const auto& Voice) { return !Voice.IsValid() || !Voice->IsPlaying(); });
    while (Voices.Num() >= 8) { if (Voices[0].IsValid()) Voices[0]->Stop(); Voices.RemoveAt(0); }
    UAudioComponent* Component = UGameplayStatics::SpawnSound2D(this, Asset->Sound, FMath::Clamp(UiSoundVolume * Asset->Volume, 0.f, 1.f), FMath::Clamp(UiSoundPitch * Asset->Pitch, .25f, 4.f));
    if (Component) { LastPlayed.Add(Asset, Now); Voices.Add(Component); LastUiAudioComponent = Component; UiSoundPlayCount++; }
    return Component;
}
void USvgCanvasMotionNode::SetVisibility(ESlateVisibility InVisibility)
{
    const bool Before = IsVisible(); Super::SetVisibility(InVisibility);
    if (Before != IsVisible() && GetCachedWidget().IsValid()) PlayUiSound(IsVisible() ? TEXT("opened") : TEXT("closed"));
}
void USvgCanvasMotionNode::SetInteractionEnabled(bool Enabled)
{
    const bool Changed = Input && Input->GetIsEnabled() != Enabled;
    Super::SetInteractionEnabled(Enabled);
    if (Changed && HasFeedback())
    {
        FeedbackHovered = FeedbackPressed = false;
        PlayMotion(Enabled ? TEXT("normal") : TEXT("disabled"));
    }
}
void USvgCanvasMotionNode::RestoreMotionVisuals()
{
    if(EdgeOverlay && MotionAsset)EdgeOverlay->SetFrame(MotionAsset->Edge,MotionTint,0,0);
    for (const auto& Visual : Visuals) if (UWidget* Widget = Visual.Widget.Get())
    {
        Widget->SetRenderTransform(Visual.Transform); Widget->SetRenderTransformPivot(Visual.Pivot); Widget->SetRenderOpacity(Visual.Opacity);
    }
    Visuals.Reset(); CurrentFrame = StartFrame = FSvgMotionFrame(); FeedbackTime = 0;
}
void USvgCanvasMotionNode::AdvanceMotionForValidation(float DeltaTime)
{
    if (!HasFeedback()) return;
    if (!ActiveClip && FeedbackState.IsEmpty()) PlayMotion(TEXT("normal"));
    if (!ActiveClip) return;
    for (const UWidget* Current = this; Current; Current = Current->GetParent())
        if (Current->GetVisibility() == ESlateVisibility::Collapsed || Current->GetVisibility() == ESlateVisibility::Hidden) return;
    if (Input && !Input->GetIsEnabled() && FeedbackState != TEXT("disabled")) { PlayMotion(TEXT("disabled")); if (!ActiveClip) return; }
    FeedbackTime += FMath::Max(0.f, DeltaTime); CurrentFrame = ActiveClip->Sample(FeedbackTime, StartFrame);
    if(MotionAsset->EdgeOnly){if(EdgeOverlay)EdgeOverlay->SetFrame(MotionAsset->Edge,MotionTint,CurrentFrame.Rotation,CurrentFrame.Opacity);return;}
    if (Visuals.IsEmpty()) for (UWidget* Child : GetAllChildren()) if (Child != Input && (!MotionAsset->VisualOnly || Child == Surface))
    {
        FSvgMotionVisual Visual; Visual.Widget = Child; Visual.Transform = Child->GetRenderTransform(); Visual.Pivot = Child->GetRenderTransformPivot(); Visual.Opacity = Child->GetRenderOpacity(); Visuals.Add(Visual);
    }
    const FVector2D ParentSize = GetCachedGeometry().GetLocalSize(), Center = ParentSize * .5;
    for (const auto& Visual : Visuals) if (UWidget* Widget = Visual.Widget.Get())
    {
        const FVector2D LocalSize = Widget->GetCachedGeometry().GetLocalSize(); if (LocalSize.X <= 0 || LocalSize.Y <= 0) continue;
        const UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Widget->Slot);
        const FVector2D Position = Canvas ? ParentSize * FVector2D(Canvas->GetAnchors().Minimum) + Canvas->GetPosition() - LocalSize * Canvas->GetAlignment() : FVector2D::ZeroVector;
        Widget->SetRenderTransformPivot(FVector2D((Center.X - Position.X) / LocalSize.X, (Center.Y - Position.Y) / LocalSize.Y));
        FWidgetTransform Transform = Visual.Transform; Transform.Scale *= FVector2D(CurrentFrame.ScaleX, CurrentFrame.ScaleY); Transform.Translation += FVector2D(CurrentFrame.X, CurrentFrame.Y); Transform.Angle += CurrentFrame.Rotation;
        Widget->SetRenderTransform(Transform); Widget->SetRenderOpacity(Visual.Opacity * FMath::Clamp(CurrentFrame.Opacity, 0.f, 1.f));
    }
}
void USvgCanvasMotionNode::SetMotionEnabled(bool Enabled)
{
    if(MotionEnabled==Enabled)return;MotionEnabled=Enabled;
    if(Enabled)PlayMotion(TEXT("normal"));else StopMotion();
}
void USvgCanvasMotionNode::DismissMotionOnHover(){SetMotionEnabled(false);}
void USvgCanvasMotionNode::SetMotionTint(FLinearColor Tint)
{
    MotionTint=Tint;if(EdgeOverlay&&MotionAsset)EdgeOverlay->SetFrame(MotionAsset->Edge,MotionTint,CurrentFrame.Rotation,ActiveClip?CurrentFrame.Opacity:0);
}
EActiveTimerReturnType USvgCanvasMotionNode::TickFeedback(double CurrentTime, float DeltaTime)
{
    if (!ActiveClip) { FeedbackTimer.Reset(); return EActiveTimerReturnType::Stop; }
    AdvanceMotionForValidation(DeltaTime);
    if (!ActiveClip || (!ActiveClip->Loop && FeedbackTime >= ActiveClip->Duration)) { FeedbackTimer.Reset(); return EActiveTimerReturnType::Stop; }
    return EActiveTimerReturnType::Continue;
}
