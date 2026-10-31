#include "SvgCanvasSoundVerifyCommandlet.h"
#include "SvgCanvasMotionNode.h"
#include "SvgCanvasDocument.h"
#include "WidgetBlueprint.h"
#include "Audio.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UObject/UObjectIterator.h"

USvgCanvasSoundVerifyCommandlet::USvgCanvasSoundVerifyCommandlet() { IsEditor = true; IsClient = false; IsServer = false; LogToConsole = true; }
int32 USvgCanvasSoundVerifyCommandlet::Main(const FString& Params)
{
    FString Output; FParse::Value(*Params, TEXT("Output="), Output); if (Output.IsEmpty()) Output = FPaths::ProjectSavedDir() / TEXT("SvgCanvas/audio");
    auto Receipt = MakeShared<FJsonObject>(); bool Passed = true;
    USvgCanvasDocument* Document = LoadObject<USvgCanvasDocument>(nullptr, TEXT("/Game/UICreator/DA_CurrentCanvas.DA_CurrentCanvas"));
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UICreator/WBP_CurrentCanvas.WBP_CurrentCanvas"));
    if (!Document || !Blueprint) return 1;
    TSharedPtr<FJsonObject> Project; if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Document->ProjectJson), Project)) return 2;
    const TSharedPtr<FJsonObject>* Sounds = nullptr; if (!Project->TryGetObjectField(TEXT("sounds"), Sounds)) return 3;
    for (const auto& Pair : (*Sounds)->Values)
    {
        const FString Id(Pair.Key.ToView()), Name = TEXT("A_") + FMD5::HashAnsiString(*Id).Left(24), Path = TEXT("/Game/UICreator/Audio/") + Name + TEXT(".") + Name;
        USvgCanvasSoundAsset* Asset = LoadObject<USvgCanvasSoundAsset>(nullptr, *Path); if (!Asset || !Asset->Sound) { Passed = false; continue; }
        TArray<uint8> Wav, PCM; FWaveModInfo Info; uint32 Rate = 0; uint16 Channels = 0;
        const FString Source = Pair.Value->AsObject()->GetStringField(TEXT("source"));
        bool Equal = FBase64::Decode(Source.RightChop(22), Wav) && Info.ReadWaveInfo(Wav.GetData(), Wav.Num());
        Equal = Equal && Asset->SourceAssetId == Id && Asset->SourceDataHash == FMD5::HashBytes(Wav.GetData(), Wav.Num()) && Asset->Sound->GetImportedSoundWaveData(PCM, Rate, Channels);
        Equal = Equal && Rate == *Info.pSamplesPerSec && Channels == *Info.pChannels && PCM.Num() == Info.SampleDataSize && FMemory::Memcmp(PCM.GetData(), Info.SampleDataStart, PCM.Num()) == 0;
        const auto Authored = Pair.Value->AsObject();
        Equal = Equal && FMath::IsNearlyEqual(Asset->Volume, (float)Authored->GetNumberField(TEXT("volume"))) && FMath::IsNearlyEqual(Asset->Pitch, (float)Authored->GetNumberField(TEXT("pitch"))) && Asset->CooldownMs == Authored->GetNumberField(TEXT("cooldown"));
        Equal = Equal && Asset->Sound->GetSoundAssetCompressionType() == ESoundAssetCompressionType::PCM && Asset->Sound->LoadingBehavior == ESoundWaveLoadingBehavior::ForceInline;
        Passed &= Equal;
    }
    TSet<FString> Nodes; int32 Bindings = 0;
    for (TObjectIterator<USvgCanvasMotionNode> It; It; ++It)
    {
        USvgCanvasMotionNode* Node = *It; if (Node->GetOutermost() != Blueprint->GetOutermost() || Nodes.Contains(Node->SourceId)) continue; Nodes.Add(Node->SourceId);
        TSharedPtr<FJsonObject> Source; if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Node->SourcePropertiesJson), Source)) { Passed = false; continue; }
        const TSharedPtr<FJsonObject>* Audio = nullptr; if (!Source->TryGetObjectField(TEXT("audio"), Audio)) continue;
        bool Enabled = true; (*Audio)->TryGetBoolField(TEXT("enabled"), Enabled);
        Passed &= Node->UiSoundEnabled == Enabled && FMath::IsNearlyEqual(Node->UiSoundVolume, (float)(*Audio)->GetNumberField(TEXT("volume"))) && FMath::IsNearlyEqual(Node->UiSoundPitch, (float)(*Audio)->GetNumberField(TEXT("pitch")));
        const auto Events = (*Audio)->GetObjectField(TEXT("events"));
        for (const auto& Event : Events->Values) { const FString Id = Event.Value->AsString(); if (Id.IsEmpty()) continue; const auto* Asset = Node->SoundEvents.Find(FName(*FString(Event.Key.ToView()))); Passed &= Asset && *Asset && (*Asset)->SourceAssetId == Id; Bindings++; }
    }
    Passed &= (*Sounds)->Values.Num() > 0 && Bindings > 0;
    Receipt->SetBoolField(TEXT("ok"), Passed); Receipt->SetStringField(TEXT("sourceHash"), Document->SourceHash); Receipt->SetNumberField(TEXT("soundAssets"), (*Sounds)->Values.Num()); Receipt->SetNumberField(TEXT("eventBindings"), Bindings);
    Receipt->SetBoolField(TEXT("wavAndImportedPCMEqual"), Passed); Receipt->SetBoolField(TEXT("savedWbpReferencesEqual"), Passed); Receipt->SetStringField(TEXT("host"), TEXT("fresh native asset reload; no audio playback"));
    Receipt->SetBoolField(TEXT("assetSettingsAndInlinePCMEqual"), Passed);
    IFileManager::Get().MakeDirectory(*Output, true); FString Json; FJsonSerializer::Serialize(Receipt, TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json, *(Output / TEXT("sound-receipt.json")));
    return Passed ? 0 : 4;
}
