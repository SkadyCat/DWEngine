#include "SvgCanvasMotionNode.h"
#include "SvgCanvasScreen.h"
#include "Components/Button.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"

void StartSvgCanvasAudioVerification()
{
    FString Output; if (!FParse::Value(FCommandLine::Get(), TEXT("SvgCanvasVerifyAudio="), Output)) return;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Output, Elapsed = 0.f](float Delta) mutable
    {
        Elapsed += Delta; if (Elapsed < 2) return true;
        USvgCanvasMotionNode* Button = nullptr; FName Trigger(TEXT("clicked"));
        for (TObjectIterator<USvgCanvasMotionNode> It; It; ++It)
        {
            if (!It->GetWorld() || !It->GetWorld()->IsGameWorld() || !It->Input || !It->Input->GetIsEnabled() || !It->GetCachedWidget().IsValid() || It->GetCachedGeometry().GetLocalSize().GetMin() <= 0) continue;
            bool Visible = true; for (const UWidget* Parent = *It; Parent; Parent = Parent->GetParent()) Visible &= Parent->IsVisible(); if (!Visible) continue;
            // Prefer an authored press cue, so the import test covers non-default WAV settings.
            if (It->SoundEvents.Contains(TEXT("pressed"))) { Button = *It; Trigger = TEXT("pressed"); break; }
            if (It->SoundEvents.Contains(TEXT("clicked"))) Button = *It;
        }
        if (!Button && Elapsed < 45) return true;
        auto Receipt = MakeShared<FJsonObject>(); bool Passed = false;
        if (Button)
        {
            const int32 Before = Button->UiSoundPlayCount;
            if (Trigger == TEXT("pressed")) Button->Input->OnPressed.Broadcast(); else Button->Input->OnClicked.Broadcast();
            UAudioComponent* Voice = Button->LastUiAudioComponent;
            const bool Started = Voice && Voice->IsPlaying() && Button->UiSoundPlayCount > Before;
            const bool Cooldown = Button->PlayUiSound(Trigger) == nullptr;
            Button->UiSoundEnabled = false; const bool Muted = Button->PlayUiSound(Trigger) == nullptr; Button->UiSoundEnabled = true;
            Passed = Started && Cooldown && Muted;
            Receipt->SetBoolField(TEXT("nativeButtonStartedAudioComponent"), Started); Receipt->SetBoolField(TEXT("cooldownSuppressedRepeat"), Cooldown); Receipt->SetBoolField(TEXT("disabledSoundSuppressed"), Muted);
            Receipt->SetStringField(TEXT("node"), Button->SourceId); Receipt->SetNumberField(TEXT("volume"), Voice ? Voice->VolumeMultiplier : 0); Receipt->SetNumberField(TEXT("pitch"), Voice ? Voice->PitchMultiplier : 0);
            Receipt->SetStringField(TEXT("event"), Trigger.ToString()); Receipt->SetStringField(TEXT("soundAssetId"), Button->SoundEvents[Trigger]->SourceAssetId);
        }
        Receipt->SetBoolField(TEXT("ok"), Passed); Receipt->SetStringField(TEXT("host"), TEXT("game world with audio enabled"));
        IFileManager::Get().MakeDirectory(*Output, true); FString Json; FJsonSerializer::Serialize(Receipt, TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json, *(Output / TEXT("audio-runtime.json")));
        FPlatformMisc::RequestExit(false); return false;
    }));
}
