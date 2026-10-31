#include "SvgCanvasCommandlets.h"
#include "SvgCanvasDocument.h"
#include "SvgCanvasNode.h"
#include "SvgCanvasMotionNode.h"
#include "SvgCanvasSoundAsset.h"
#include "Factories/SoundFactory.h"
#include "Audio.h"
#include "SvgCanvasImage.h"
#include "SvgCanvasScreen.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "ImageUtils.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Widgets/Images/SImage.h"
#include "Layout/ArrangedChildren.h"
#include "Brushes/SlateColorBrush.h"
#include "TextureResource.h"
#include "TextureCompiler.h"
#include "RenderingThread.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogSvgCanvasBridge, Log, All);
namespace SvgCanvasImport { static bool Verify(const FString& BundlePath, const FString& Output, FString& Error); }
class FSvgCanvasEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FString Bundle, Output;
        if (!FParse::Value(FCommandLine::Get(), TEXT("SvgCanvasVerifyBundle="), Bundle)) return;
        FParse::Value(FCommandLine::Get(), TEXT("SvgCanvasVerifyOutput="), Output);
        FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([Bundle, Output]()
        {
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Bundle, Output](float)
            {
                FString Error; const bool Ok = SvgCanvasImport::Verify(Bundle, Output, Error);
                if (!Ok) UE_LOG(LogSvgCanvasBridge, Error, TEXT("%s"), *Error);
                FPlatformMisc::RequestExitWithStatus(false, Ok ? 0 : 1); return false;
            }), 2.0f);
        });
    }
};
IMPLEMENT_MODULE(FSvgCanvasEditorModule, SvgCanvasEditor)

namespace SvgCanvasImport
{
    // Keep each generated screen family in its own package namespace.
    static FString CanvasRoot()
    {
        FString Root = TEXT("/Game/UICreator");
        FParse::Value(FCommandLine::Get(), TEXT("SvgCanvasRoot="), Root);
        Root.RemoveFromEnd(TEXT("/"));
        return Root;
    }
    static bool ValidRoot(FString& Error)
    {
        const FString Root = CanvasRoot();
        if (Root.StartsWith(TEXT("/Game/")) && FPackageName::IsValidLongPackageName(Root)) return true;
        Error = TEXT("Invalid SvgCanvasRoot: ") + Root;
        return false;
    }
    static FString Json(const TSharedPtr<FJsonObject>& Object)
    {
        FString Result; FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Result)); return Result;
    }
    static bool WriteJson(const FString& File, const TSharedRef<FJsonObject>& Value)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        return FFileHelper::SaveStringToFile(Json(Value), *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
    static FString String(const TSharedPtr<FJsonObject>& Value, const TCHAR* Key, const FString& Default = TEXT(""))
    {
        FString Result; return Value && Value->TryGetStringField(Key, Result) ? Result : Default;
    }
    static double Number(const TSharedPtr<FJsonObject>& Value, const TCHAR* Key, double Default = 0)
    {
        double Result; return Value && Value->TryGetNumberField(Key, Result) ? Result : Default;
    }
    static FVector4 Rect(const TSharedPtr<FJsonObject>& Value)
    {
        return FVector4(Number(Value, TEXT("x")), Number(Value, TEXT("y")), Number(Value, TEXT("width")), Number(Value, TEXT("height")));
    }
    static bool Save(UObject* Asset)
    {
        Asset->MarkPackageDirty(); UPackage* Package = Asset->GetOutermost();
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *Filename, Args);
    }
    static UTexture2D* Texture(const FString& Key, const TSharedPtr<FJsonObject>& Record)
    {
        const FString AssetName = TEXT("T_") + Key.Left(40), PackageName = CanvasRoot() + TEXT("/Rasters/") + AssetName;
        UTexture2D* Result = LoadObject<UTexture2D>(nullptr, *(PackageName + TEXT(".") + AssetName), nullptr, LOAD_NoWarn);
        TArray<uint8> Compressed; if (!FBase64::Decode(String(Record, TEXT("png")), Compressed)) return nullptr;
        IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        TSharedPtr<IImageWrapper> Image = Images.CreateImageWrapper(EImageFormat::PNG);
        TArray64<uint8> Pixels;
        if (!Image || !Image->SetCompressed(Compressed.GetData(), Compressed.Num()) || !Image->GetRaw(ERGBFormat::BGRA, 8, Pixels)) return nullptr;
        if (Image->GetWidth() < 1 || Image->GetHeight() < 1 || Image->GetWidth() > 8192 || Image->GetHeight() > 8192) return nullptr;
        UPackage* Package = CreatePackage(*PackageName);
        const bool IsNew = Result == nullptr;
        if (!Result) Result = NewObject<UTexture2D>(Package, *AssetName, RF_Public | RF_Standalone);
        for (int64 Index = 0; Index < Pixels.Num(); Index += 4)
        {
            const uint32 Alpha = Pixels[Index + 3];
            for (int32 Channel = 0; Channel < 3; Channel++) Pixels[Index + Channel] = (uint32(Pixels[Index + Channel]) * Alpha + 127) / 255;
        }
        // Content-addressed UI caches do not need a reimport when their source
        // pixels and import settings are unchanged. Keep their compiled data.
        if (!IsNew && Result->Source.GetSizeX() == Image->GetWidth() && Result->Source.GetSizeY() == Image->GetHeight()
            && Result->Source.GetFormat() == TSF_BGRA8 && !Result->SRGB && Result->NeverStream && Result->LODGroup == TEXTUREGROUP_UI
            && Result->MipGenSettings == TMGS_NoMipmaps && Result->CompressionSettings == TC_EditorIcon && Result->CompressionNone && Result->Filter == TF_Nearest)
        {
            TArray64<uint8> PreviousPixels;
            if (Result->Source.GetMipData(PreviousPixels, 0) && PreviousPixels.Num() == Pixels.Num()
                && FMemory::Memcmp(PreviousPixels.GetData(), Pixels.GetData(), Pixels.Num()) == 0) return Result;
        }
        Result->Source.Init(Image->GetWidth(), Image->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
        Result->SRGB = false; Result->NeverStream = true; Result->LODGroup = TEXTUREGROUP_UI; Result->MipGenSettings = TMGS_NoMipmaps;
        Result->CompressionSettings = TC_EditorIcon; Result->CompressionNone = true; Result->Filter = TF_Nearest;
        Result->PostEditChange(); Result->UpdateResource();
        if (IsNew) FAssetRegistryModule::AssetCreated(Result); if (!Save(Result)) return nullptr; return Result;
    }
    static FSvgCanvasRaster Raster(const USvgCanvasDocument* Document, const FString& Key)
    {
        const FSvgCanvasRaster* Value = Document->Rasters.Find(Key); return Value ? *Value : FSvgCanvasRaster();
    }
    static ESlateVisibility Visibility(const FString& Value)
    {
        return Value == TEXT("visible") ? ESlateVisibility::SelfHitTestInvisible : Value == TEXT("hidden") ? ESlateVisibility::Hidden : ESlateVisibility::Collapsed;
    }

    static bool Import(const FString& BundlePath, const FString& Receipt, FString& Error)
    {
        if (!ValidRoot(Error)) return false;
        if (GEditor && GEditor->PlayWorld) { Error = TEXT("Stop PIE before importing a canvas."); return false; }
        FString Body; TSharedPtr<FJsonObject> Bundle;
        if (!FFileHelper::LoadFileToString(Body, *BundlePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Bundle) || !Bundle) { Error = TEXT("Invalid bundle JSON"); return false; }
        if (String(Bundle, TEXT("schema")) != TEXT("uicreator-alpha/native-wbp") || Number(Bundle, TEXT("version")) != 1) { Error = TEXT("Unsupported bundle contract"); return false; }
        const auto Viewport = Bundle->GetObjectField(TEXT("viewport"));
        const int32 Width = Number(Viewport, TEXT("width")), Height = Number(Viewport, TEXT("height"));
        if (Width < 16 || Height < 16 || Width > 8192 || Height > 8192) { Error = TEXT("Invalid viewport"); return false; }
        const TArray<TSharedPtr<FJsonValue>>& Nodes = Bundle->GetArrayField(TEXT("nodes"));
        if (Nodes.Num() < 1 || Nodes.Num() > 20000) { Error = TEXT("Invalid node count"); return false; }
        const auto Project = Bundle->GetObjectField(TEXT("project"));
        TMap<FString, USvgCanvasMotionAsset*> Motions;
        TMap<FString, USvgCanvasSoundAsset*> Sounds;
        const TSharedPtr<FJsonObject>* SoundValues = nullptr;
        if (Project->TryGetObjectField(TEXT("sounds"), SoundValues)) for (const auto& Pair : (*SoundValues)->Values)
        {
            if (Pair.Value->Type != EJson::Object) { Error = TEXT("Invalid UI sound asset"); return false; }
            const FString Id(Pair.Key.ToView()), Suffix = FMD5::HashAnsiString(*Id).Left(24); const auto Value = Pair.Value->AsObject();
            const FString Source = String(Value, TEXT("source")); TArray<uint8> Bytes;
            if (!Source.StartsWith(TEXT("data:audio/wav;base64,")) || !FBase64::Decode(Source.RightChop(22), Bytes) || Bytes.Num() < 44 || Bytes.Num() > 1048576) { Error = TEXT("UI sound must be a WAV under 1 MB"); return false; }
            FWaveModInfo WaveInfo;
            if (Number(Value, TEXT("version")) != 1 || !WaveInfo.ReadWaveInfo(Bytes.GetData(), Bytes.Num()) || !WaveInfo.pFormatTag || *WaveInfo.pFormatTag != 1 || !WaveInfo.pBitsPerSample || *WaveInfo.pBitsPerSample != 16 || *WaveInfo.pChannels < 1 || *WaveInfo.pChannels > 2 || *WaveInfo.pSamplesPerSec < 8000 || *WaveInfo.pSamplesPerSec > 96000) { Error = TEXT("UI sounds require 16-bit PCM WAV, mono/stereo, 8-96 kHz"); return false; }
            const FString WaveName = TEXT("S_") + Suffix, WavePath = CanvasRoot() + TEXT("/Audio/") + WaveName;
            // Load the existing asset before reimport; a newly created, partially loaded package cannot be saved over it.
            USoundWave* PreviousWave = LoadObject<USoundWave>(nullptr, *(WavePath + TEXT(".") + WaveName), nullptr, LOAD_NoWarn);
            UPackage* WavePackage = PreviousWave ? PreviousWave->GetOutermost() : CreatePackage(*WavePath); WavePackage->FullyLoad(); const bool ExistingWave = PreviousWave != nullptr; USoundFactory* Factory = NewObject<USoundFactory>(); Factory->bAutoCreateCue = false; Factory->SuppressImportDialogs();
            const uint8* Buffer = Bytes.GetData(); USoundWave* Wave = Cast<USoundWave>(Factory->FactoryCreateBinary(USoundWave::StaticClass(), WavePackage, *WaveName, RF_Public | RF_Standalone, nullptr, TEXT("wav"), Buffer, Bytes.GetData() + Bytes.Num(), GWarn));
            if (!Wave) { Error = TEXT("Could not import UI WAV: ") + Id; return false; }
            Wave->Volume = 1; Wave->Pitch = 1; Wave->bLooping = false;
            Wave->SetSoundAssetCompressionType(ESoundAssetCompressionType::PCM); Wave->LoadingBehavior = ESoundWaveLoadingBehavior::ForceInline;
            if (!Save(Wave)) { Error = TEXT("Could not save UI WAV"); return false; }
            if (!ExistingWave) FAssetRegistryModule::AssetCreated(Wave);
            const FString Name = TEXT("A_") + Suffix, PackageName = CanvasRoot() + TEXT("/Audio/") + Name; UPackage* Package = CreatePackage(*PackageName);
            USvgCanvasSoundAsset* Asset = LoadObject<USvgCanvasSoundAsset>(nullptr, *(PackageName + TEXT(".") + Name));
            if (!Asset) { Asset = NewObject<USvgCanvasSoundAsset>(Package, *Name, RF_Public | RF_Standalone); FAssetRegistryModule::AssetCreated(Asset); }
            Asset->SourceAssetId = Id; Asset->SourceAssetName = String(Value, TEXT("name")); Asset->SourceDataHash = FMD5::HashBytes(Bytes.GetData(), Bytes.Num()); Asset->Sound = Wave;
            Asset->Volume = Number(Value, TEXT("volume"), .65); Asset->Pitch = Number(Value, TEXT("pitch"), 1); Asset->CooldownMs = Number(Value, TEXT("cooldown"), 60);
            if (!FMath::IsFinite(Asset->Volume) || Asset->Volume < 0 || Asset->Volume > 1 || !FMath::IsFinite(Asset->Pitch) || Asset->Pitch < .25 || Asset->Pitch > 4 || Asset->CooldownMs < 0 || Asset->CooldownMs > 5000 || Wave->Duration > 10) { Error = TEXT("Invalid UI sound settings"); return false; }
            if (!Save(Asset)) { Error = TEXT("Could not save UI sound asset"); return false; } Sounds.Add(Id, Asset);
        }
        const TSharedPtr<FJsonObject>* MotionValues = nullptr;
        if (Project->TryGetObjectField(TEXT("motions"), MotionValues)) for (const auto& Pair : (*MotionValues)->Values)
        {
            if (Pair.Value->Type != EJson::Object) { Error = TEXT("Motion asset must be an object"); return false; }
            const FString Id(Pair.Key.ToView()), Name = TEXT("MA_") + FMD5::HashAnsiString(*Id).Left(24), PackageName = CanvasRoot() + TEXT("/Motions/") + Name;
            UPackage* Package = CreatePackage(*PackageName);
            USvgCanvasMotionAsset* Asset = LoadObject<USvgCanvasMotionAsset>(nullptr, *(PackageName + TEXT(".") + Name));
            if (!Asset) { Asset = NewObject<USvgCanvasMotionAsset>(Package, *Name, RF_Public | RF_Standalone); FAssetRegistryModule::AssetCreated(Asset); }
            Asset->SourceAssetId = Id; Asset->SourceAssetName = String(Pair.Value->AsObject(), TEXT("name")); Asset->DefinitionJson = Json(Pair.Value->AsObject());
            if (!Asset->Decode(Error) || !Save(Asset)) { if (Error.IsEmpty()) Error = TEXT("Could not save motion asset"); return false; }
            Motions.Add(Id, Asset);
        }
        const FString DocumentName = TEXT("DA_CurrentCanvas"), DocumentPackageName = CanvasRoot() + TEXT("/") + DocumentName;
        UPackage* DocumentPackage = CreatePackage(*DocumentPackageName);
        USvgCanvasDocument* Document = LoadObject<USvgCanvasDocument>(nullptr, *(DocumentPackageName + TEXT(".") + DocumentName));
        if (!Document) { Document = NewObject<USvgCanvasDocument>(DocumentPackage, *DocumentName, RF_Public | RF_Standalone); FAssetRegistryModule::AssetCreated(Document); }
        Document->ProjectJson = Json(Project); Document->SourceSvg = String(Bundle, TEXT("sourceSvg")); Document->SourceHash = String(Bundle, TEXT("sourceHash")); Document->DesignSize = FVector2D(Width, Height);
        Document->SvgAssets.Reset(); Document->Rasters.Reset(); Document->States.Reset();
        for (const auto& Pair : Project->GetObjectField(TEXT("assets"))->Values) Document->SvgAssets.Add(FString(Pair.Key.ToView()), String(Pair.Value->AsObject(), TEXT("source")));
        const auto Rasters = Bundle->GetObjectField(TEXT("rasters"));
        for (const auto& Pair : Rasters->Values)
        {
            const auto Value = Pair.Value->AsObject(); FSvgCanvasRaster Layer; Layer.Texture = Texture(FString(Pair.Key.ToView()), Value);
            if (!Layer.Texture) { Error = FString::Printf(TEXT("Failed importing raster %s"), *FString(Pair.Key.ToView())); return false; }
            Layer.PixelOrigin = FVector2D(Number(Value, TEXT("x")), Number(Value, TEXT("y"))); Layer.PixelSize = FVector2D(Number(Value, TEXT("width")), Number(Value, TEXT("height"))); Layer.SourceSvg = String(Value, TEXT("svg"));
            Document->Rasters.Add(FString(Pair.Key.ToView()), MoveTemp(Layer));
        }
        for (const auto& Pair : Bundle->GetObjectField(TEXT("states"))->Values)
        {
            FSvgCanvasSceneState State;
            for (const auto& Item : Pair.Value->AsObject()->GetArrayField(TEXT("nodes")))
            {
                const auto Value = Item->AsObject(); FSvgCanvasNodeState Node;
                Node.Id = String(Value, TEXT("id")); Node.ParentId = String(Value, TEXT("parentId")); Node.Rect = Rect(Value); Node.LayoutScale = Number(Value, TEXT("layoutScale"), 1);
                Node.Visibility = String(Value, TEXT("visibility"), TEXT("collapsed")); Node.Raster = String(Value, TEXT("raster")); Node.Text = String(Value, TEXT("text")); Node.PaintOrder = Number(Value, TEXT("paintOrder"));
                Node.LocalVisibility = String(Value, TEXT("localVisibility"), Node.Visibility);
                const TSharedPtr<FJsonObject>* Brushes = nullptr;
                if (Value->TryGetObjectField(TEXT("brushRasters"), Brushes) && Brushes->IsValid())
                {
                    Node.HoveredRaster = String(*Brushes, TEXT("hovered")); Node.PressedRaster = String(*Brushes, TEXT("pressed")); Node.DisabledRaster = String(*Brushes, TEXT("disabled"));
                    const FString Normal = String(*Brushes, TEXT("normal")); if (!Normal.IsEmpty()) Node.Raster = Normal;
                }
                Value->TryGetBoolField(TEXT("interactable"), Node.Interactable);
                State.Nodes.Add(MoveTemp(Node));
            }
            Document->States.Add(FString(Pair.Key.ToView()), MoveTemp(State));
        }
        const FSvgCanvasSceneState* Default = Document->States.Find(TEXT("canvas"));
        if (!Default || Default->Nodes.Num() != Nodes.Num()) { Error = TEXT("Canvas state must cover the complete source node set"); return false; }
        if (!Save(Document)) { Error = TEXT("Could not save SVG document"); return false; }
        // Runtime resolves only the raster references actually used by a template.
        // Keep this identity asset free of hard texture references and editor source payloads.
        const FString RuntimeName=TEXT("DA_RuntimeCanvas"),RuntimePackageName=CanvasRoot()+TEXT("/")+RuntimeName;
        UPackage* RuntimePackage=CreatePackage(*RuntimePackageName);
        USvgCanvasDocument* RuntimeDocument=LoadObject<USvgCanvasDocument>(nullptr,*(RuntimePackageName+TEXT(".")+RuntimeName));
        if(!RuntimeDocument){RuntimeDocument=NewObject<USvgCanvasDocument>(RuntimePackage,*RuntimeName,RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(RuntimeDocument);}
        RuntimeDocument->SourceHash=Document->SourceHash;RuntimeDocument->DesignSize=Document->DesignSize;RuntimeDocument->ProjectJson.Empty();RuntimeDocument->SourceSvg.Empty();RuntimeDocument->SvgAssets.Empty();RuntimeDocument->Rasters.Empty();RuntimeDocument->States.Empty();
        if(!Save(RuntimeDocument)){Error=TEXT("Could not save runtime document identity");return false;}


        const FString BlueprintName = TEXT("WBP_CurrentCanvas"), BlueprintPackageName = CanvasRoot() + TEXT("/") + BlueprintName;
        UPackage* BlueprintPackage = CreatePackage(*BlueprintPackageName);
        UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *(BlueprintPackageName + TEXT(".") + BlueprintName));
        if (Blueprint && Blueprint->ParentClass != USvgCanvasScreen::StaticClass()) { Error = TEXT("Destination WBP belongs to another author; refusing to replace it"); return false; }
        if (!Blueprint) { Blueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(USvgCanvasScreen::StaticClass(), BlueprintPackage, *BlueprintName, BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass())); FAssetRegistryModule::AssetCreated(Blueprint); }
        if (!Blueprint) { Error = TEXT("Could not create Widget Blueprint"); return false; }
        // Generated scope is the full WidgetTree. Replacing it removes stale generated nodes.
        TSet<FName> PreviousWidgetNames;
        if (Blueprint->WidgetTree) Blueprint->WidgetTree->ForEachWidget([&](UWidget* Widget) { PreviousWidgetNames.Add(Widget->GetFName()); });
        if (Blueprint->WidgetTree) { Blueprint->WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional); }
        Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
        TMap<FString, USvgCanvasNode*> Native;
        TMap<FString, const FSvgCanvasNodeState*> DefaultStates;
        for (const auto& Item : Default->Nodes) DefaultStates.Add(Item.Id, &Item);
        TMap<FString, const FSvgCanvasNodeState*> AuthoringStates;
        for (const auto& Scene : Document->States) for (const auto& Item : Scene.Value.Nodes)
            if (Item.Visibility == TEXT("visible") && !AuthoringStates.Contains(Item.Id)) AuthoringStates.Add(Item.Id, &Item);
        TArray<FSvgCanvasNodeState> Authoring;
        const TArray<TSharedPtr<FJsonValue>>* AuthoringValues = nullptr;
        if (Bundle->TryGetArrayField(TEXT("authoringNodes"), AuthoringValues))
        {
            for (const auto& Item : *AuthoringValues)
            {
                const auto Value = Item->AsObject(); FSvgCanvasNodeState State;
                State.Id = String(Value, TEXT("id")); State.Rect = Rect(Value); State.LayoutScale = Number(Value, TEXT("layoutScale"), 1); State.Raster = String(Value, TEXT("raster")); State.Text = String(Value, TEXT("text")); State.PaintOrder = Number(Value, TEXT("paintOrder"));
                const auto Brushes = Value->GetObjectField(TEXT("brushRasters")); State.HoveredRaster = String(Brushes, TEXT("hovered")); State.PressedRaster = String(Brushes, TEXT("pressed")); State.DisabledRaster = String(Brushes, TEXT("disabled"));
                const FString Normal = String(Brushes, TEXT("normal")); if (!Normal.IsEmpty()) State.Raster = Normal;
                Authoring.Add(MoveTemp(State));
            }
            for (const auto& Item : Authoring) if (!AuthoringStates.Contains(Item.Id)) AuthoringStates.Add(Item.Id, &Item);
        }
        for (const auto& Item : Nodes)
        {
            const auto Value = Item->AsObject(); const FString Id = String(Value, TEXT("id"));
            if (Id.IsEmpty() || Native.Contains(Id) || !DefaultStates.Contains(Id)) { Error = TEXT("Duplicate, missing or invalid node identity"); return false; }
            const FString Name = TEXT("N_") + FMD5::HashAnsiString(*Id).Left(24);
            USvgCanvasNode* Node = Blueprint->WidgetTree->ConstructWidget<USvgCanvasNode>(USvgCanvasMotionNode::StaticClass(), *Name);
            Node->SourceId = Id; Node->SourceName = String(Value, TEXT("name")); Node->SourceType = String(Value, TEXT("type")); Node->SourcePrefab = String(Value, TEXT("prefabId"));
            Node->SetDisplayLabel(Node->SourceName.IsEmpty() ? Node->SourceId : Node->SourceName);
            Node->SourcePropertiesJson = Json(Value); Node->Binding = String(Value, TEXT("binding"));
            const TSharedPtr<FJsonObject>* Audio = nullptr;
            if (Value->TryGetObjectField(TEXT("audio"), Audio))
            {
                USvgCanvasMotionNode* Feedback = CastChecked<USvgCanvasMotionNode>(Node);
                (*Audio)->TryGetBoolField(TEXT("enabled"), Feedback->UiSoundEnabled); Feedback->UiSoundVolume = Number(*Audio, TEXT("volume"), 1); Feedback->UiSoundPitch = Number(*Audio, TEXT("pitch"), 1);
                if (!FMath::IsFinite(Feedback->UiSoundVolume) || Feedback->UiSoundVolume < 0 || Feedback->UiSoundVolume > 1 || !FMath::IsFinite(Feedback->UiSoundPitch) || Feedback->UiSoundPitch < .25 || Feedback->UiSoundPitch > 4) { Error = TEXT("Invalid UI sound binding settings"); return false; }
                const TSharedPtr<FJsonObject>* Events = nullptr;
                if ((*Audio)->TryGetObjectField(TEXT("events"), Events)) for (const auto& Pair : (*Events)->Values)
                {
                    const FString SoundId = Pair.Value->AsString(); if (SoundId.IsEmpty()) continue;
                    if (!Sounds.Contains(SoundId)) { Error = TEXT("Missing UI sound reference: ") + SoundId; return false; }
                    Feedback->SoundEvents.Add(FName(*FString(Pair.Key.ToView())), Sounds[SoundId]);
                }
            }
            const TSharedPtr<FJsonObject>* MotionReference = nullptr;
            if (Value->TryGetObjectField(TEXT("motion"), MotionReference))
            {
                const FString AssetId = String(*MotionReference, TEXT("assetId"));
                if (String(*MotionReference, TEXT("preset")) != TEXT("none") && !AssetId.IsEmpty())
                {
                    if (!Motions.Contains(AssetId)) { Error = TEXT("Missing motion asset: ") + AssetId; return false; }
                    CastChecked<USvgCanvasMotionNode>(Node)->MotionAsset = Motions[AssetId];
                    bool Enabled=true;(*MotionReference)->TryGetBoolField(TEXT("enabled"),Enabled);CastChecked<USvgCanvasMotionNode>(Node)->MotionEnabled=Enabled;
                    bool Dismiss=false;(*MotionReference)->TryGetBoolField(TEXT("dismissOnHover"),Dismiss);CastChecked<USvgCanvasMotionNode>(Node)->MotionDismissOnHover=Dismiss;
                    const FString Tint=String(*MotionReference,TEXT("tint"));if(!Tint.IsEmpty())CastChecked<USvgCanvasMotionNode>(Node)->MotionTint=FLinearColor::FromSRGBColor(FColor::FromHex(Tint));
                }
            }
            const TSharedPtr<FJsonObject>* Action = nullptr; if (Value->TryGetObjectField(TEXT("action"), Action) && Action->IsValid()) Node->ActionJson = Json(*Action);
            const auto SourceSlot = Value->GetObjectField(TEXT("slot")), Anchors = SourceSlot->GetObjectField(TEXT("anchors")), Offsets = SourceSlot->GetObjectField(TEXT("offsets")), Alignment = SourceSlot->GetObjectField(TEXT("alignment"));
            Node->SourceAnchors = FAnchors(Number(Anchors, TEXT("minX")), Number(Anchors, TEXT("minY")), Number(Anchors, TEXT("maxX")), Number(Anchors, TEXT("maxY")));
            Node->SourceOffsets = FMargin(Number(Offsets, TEXT("left")), Number(Offsets, TEXT("top")), Number(Offsets, TEXT("right")), Number(Offsets, TEXT("bottom")));
            Node->SourceAlignment = FVector2D(Number(Alignment, TEXT("x")), Number(Alignment, TEXT("y")));
            const FSvgCanvasNodeState& DefaultState = *DefaultStates[Id];
            const FSvgCanvasNodeState& State = DefaultState.Visibility != TEXT("visible") && AuthoringStates.Contains(Id) ? *AuthoringStates[Id] : DefaultState;
            Node->DesignRect = State.Rect; Node->DesignScale = State.LayoutScale; Node->SourceText = State.Text;
            Node->SetVisibility(Visibility(DefaultState.LocalVisibility));
            bool SourceClip = false; Value->GetObjectField(TEXT("style"))->TryGetBoolField(TEXT("clip"), SourceClip);
            Node->SetClipping(SourceClip || Node->SourceType == TEXT("ScrollBox") ? EWidgetClipping::ClipToBounds : EWidgetClipping::Inherit);
            Node->bHiddenInDesigner = DefaultState.LocalVisibility != TEXT("visible"); Node->bIsVariable = true;
            Node->Surface = Blueprint->WidgetTree->ConstructWidget<USvgCanvasImage>(USvgCanvasImage::StaticClass(), *(Name + TEXT("_SVG")));
            UCanvasPanelSlot* SurfaceSlot = Node->AddChildToCanvas(Node->Surface); SurfaceSlot->SetZOrder(-10000);
            Node->Normal = Raster(Document, State.Raster); Node->ApplyRaster(Node->Normal);
            Node->Hovered = Raster(Document, State.HoveredRaster); Node->Pressed = Raster(Document, State.PressedRaster); Node->Disabled = Raster(Document, State.DisabledRaster);
            const TSharedPtr<FJsonObject>* Variants = nullptr;
            if (Value->TryGetObjectField(TEXT("brushRasters"), Variants) && Variants->IsValid())
            {
                const FString Normal = String(*Variants, TEXT("normal")), Hovered = String(*Variants, TEXT("hovered")), Pressed = String(*Variants, TEXT("pressed")), Disabled = String(*Variants, TEXT("disabled"));
                if (!Normal.IsEmpty()) Node->Normal = Raster(Document, Normal);
                if (!Hovered.IsEmpty()) Node->Hovered = Raster(Document, Hovered);
                if (!Pressed.IsEmpty()) Node->Pressed = Raster(Document, Pressed);
                if (!Disabled.IsEmpty()) Node->Disabled = Raster(Document, Disabled);
            }
            if (Node->SourceType == TEXT("Button"))
            {
                Node->Input = Blueprint->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *(Name + TEXT("_Input")));
                FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
                FButtonStyle Style; Style.SetNormal(Empty).SetHovered(Empty).SetPressed(Empty).SetDisabled(Empty).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
                Node->Input->SetStyle(Style);
                bool Enabled = true; Value->TryGetBoolField(TEXT("interactable"), Enabled); Node->Input->SetIsEnabled(Enabled);
                UCanvasPanelSlot* InputSlot = Node->AddChildToCanvas(Node->Input); InputSlot->SetAnchors(FAnchors(0, 0, 1, 1)); InputSlot->SetOffsets(FMargin(0)); InputSlot->SetZOrder(-9999);
            }
            Native.Add(Id, Node);
        }
        for (const auto& Item : Nodes)
        {
            const auto Value = Item->AsObject(); const FString Id = String(Value, TEXT("id")), ParentId = String(Value, TEXT("parentId")); USvgCanvasNode* Node = Native[Id]; const auto& State = *DefaultStates[Id];
            if (ParentId.IsEmpty()) { if (Blueprint->WidgetTree->RootWidget) { Error = TEXT("Multiple root nodes"); return false; } Blueprint->WidgetTree->RootWidget = Node; }
            else
            {
                if (!Native.Contains(ParentId)) { Error = TEXT("Missing source parent"); return false; }
                USvgCanvasNode* Parent = Native[ParentId]; const FVector4 ParentRect = Parent->DesignRect;
                const FSvgCanvasNodeState* AuthoringState = DefaultStates[Id]->Visibility != TEXT("visible") ? AuthoringStates.FindRef(Id) : nullptr;
                Parent->AddChildToCanvas(Node); Node->ApplyLayout(ParentRect, AuthoringState ? AuthoringState->PaintOrder : State.PaintOrder);
            }
        }
        // Root ownership is known only after the complete hierarchy has been attached.
        for (const auto& Pair : Native) { Pair.Value->ApplyRaster(Pair.Value->Normal); Pair.Value->RememberImportedDefaults(); }
        // UE tracks every source widget in the variable GUID map, including images
        // and native input children. Preserve surviving GUIDs and reconcile the rest
        // before structural compilation; the compiler's fallback emits an ensure per miss.
        TSet<FName> CurrentWidgetNames;
        Blueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            const FName Name = Widget->GetFName(); CurrentWidgetNames.Add(Name);
            if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Name)) Blueprint->OnVariableAdded(Name);
        });
        for (const FName Name : PreviousWidgetNames) if (!CurrentWidgetNames.Contains(Name)) Blueprint->OnVariableRemoved(Name);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (!Blueprint->GeneratedClass || Blueprint->Status == BS_Error) { Error = TEXT("Widget Blueprint compilation failed"); return false; }
        USvgCanvasScreen* CDO = Cast<USvgCanvasScreen>(Blueprint->GeneratedClass->GetDefaultObject()); CDO->SourceDocument = Document; CDO->InitialState = TEXT("canvas");
        CDO->DesignTimeSize = FVector2D(Width, Height); CDO->DesignSizeMode = EDesignPreviewSizeMode::FillScreen;
        if (!Save(Blueprint)) { Error = TEXT("Could not save Widget Blueprint"); return false; }
        auto Result = MakeShared<FJsonObject>(); Result->SetBoolField(TEXT("ok"), true); Result->SetStringField(TEXT("asset"), BlueprintPackageName); Result->SetStringField(TEXT("sourceHash"), Document->SourceHash);
        Result->SetNumberField(TEXT("logicalNodes"), Native.Num()); Result->SetNumberField(TEXT("svgAssets"), Document->SvgAssets.Num()); Result->SetNumberField(TEXT("rasterCaches"), Document->Rasters.Num()); Result->SetNumberField(TEXT("states"), Document->States.Num());
        Result->SetBoolField(TEXT("compiled"), true); Result->SetBoolField(TEXT("saved"), true); Result->SetBoolField(TEXT("reloadedInSeparateProcess"), false);
        Result->SetNumberField(TEXT("motionAssets"), Motions.Num());
        Result->SetNumberField(TEXT("soundAssets"), Sounds.Num());
        return WriteJson(Receipt, Result);
    }

    static bool Verify(const FString& BundlePath, const FString& Output, FString& Error)
    {
        FString Body; TSharedPtr<FJsonObject> Bundle;
        if (!FFileHelper::LoadFileToString(Body, *BundlePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Bundle)) { Error = TEXT("Invalid verification bundle"); return false; }
        if (!ValidRoot(Error)) return false;
        UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *(CanvasRoot() + TEXT("/WBP_CurrentCanvas.WBP_CurrentCanvas")));
        if (!Blueprint || !Blueprint->GeneratedClass) { Error = TEXT("Saved WBP is missing"); return false; }
        TStrongObjectPtr<USvgCanvasScreen> Screen(NewObject<USvgCanvasScreen>(GetTransientPackage(), Blueprint->GeneratedClass));
        Screen->SetDesignerFlags(EWidgetDesignFlags::None);
        if (!Screen->Initialize() || !Screen->SourceDocument || Screen->SourceDocument->SourceHash != String(Bundle, TEXT("sourceHash"))) { Error = TEXT("Reloaded source/version mismatch"); return false; }
        TArray<UTexture*> Textures; for (const auto& Pair : Screen->SourceDocument->Rasters) if (Pair.Value.Texture) Textures.AddUnique(Pair.Value.Texture);
        FTextureCompilingManager::Get().FinishCompilation(Textures); FlushRenderingCommands();
        for (const auto& Pair : Screen->SourceDocument->Rasters)
        {
            const auto& Raster = Pair.Value;
            if (!Raster.Texture || Raster.Texture->GetSizeX() != int32(Raster.PixelSize.X) || Raster.Texture->GetSizeY() != int32(Raster.PixelSize.Y)) { Error = TEXT("Compiled PNG texture size differs from source"); return false; }
        }
        TMap<FString, FString> ActualParents; int32 InputCount = 0;
        Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            if (USvgCanvasNode* Node = Cast<USvgCanvasNode>(Widget)) { const USvgCanvasNode* Parent = Cast<USvgCanvasNode>(Node->GetParent()); ActualParents.Add(Node->SourceId, Parent ? Parent->SourceId : TEXT("")); if (Node->Input) InputCount++; }
        });
        const auto& Expected = Bundle->GetArrayField(TEXT("nodes"));
        if (ActualParents.Num() != Expected.Num()) { Error = TEXT("Saved/reloaded logical node set differs"); return false; }
        for (const auto& Item : Expected) { auto Value = Item->AsObject(); const FString* Parent = ActualParents.Find(String(Value, TEXT("id"))); if (!Parent || *Parent != String(Value, TEXT("parentId"))) { Error = TEXT("Saved/reloaded source hierarchy differs"); return false; } }
        const int32 Width = Screen->SourceDocument->DesignSize.X, Height = Screen->SourceDocument->DesignSize.Y;
        if (!FSlateApplication::IsInitialized())
        {
            if (!FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletRendering"))) { Error = TEXT("Native rendering requires -AllowCommandletRendering"); return false; }
            FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
        }
        TSharedRef<SWidget> Slate = Screen->TakeWidget();
        FWidgetRenderer* Renderer = new FWidgetRenderer(true, true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Width, Height), TF_Nearest, true));
        if (!Target.IsValid()) { delete Renderer; Error = TEXT("Could not allocate widget render target"); return false; }
        Target->InitCustomFormat(Width, Height, PF_B8G8R8A8, true); Target->UpdateResourceImmediate(true); FlushRenderingCommands();
        IFileManager::Get().MakeDirectory(*Output, true);
        TArray<TSharedPtr<FJsonValue>> Captures;
        double MaxLayoutError = 0; int32 ButtonStateChecks = 0, ArrangedLayoutNodes = 0, InactiveVisibilityChecks = 0;
        for (const auto& Pair : Screen->SourceDocument->States)
        {
            if (Pair.Key != TEXT("canvas") && !Screen->ApplySceneState(Pair.Key)) { Error = TEXT("Scene state failed to apply"); delete Renderer; return false; }
            for (int32 Pass = 0; Pass < 3; Pass++) { Renderer->DrawWidget(Target.Get(), Slate, 1, FVector2D(Width, Height), 0, false); FlushRenderingCommands(); }
            const FGeometry RootGeometry = Screen->GetRootWidget()->GetCachedGeometry();
            // Slate does not paint/tick culled scroll descendants, so their cached
            // geometry may be empty. Ask the real Slate widgets to arrange children
            // without paint culling; no source rectangles are used in this traversal.
            TMap<const SWidget*, FGeometry> ArrangedGeometry;
            TFunction<void(const TSharedRef<SWidget>&, const FGeometry&)> Arrange;
            Arrange = [&](const TSharedRef<SWidget>& Widget, const FGeometry& Geometry)
            {
                ArrangedGeometry.Add(&Widget.Get(), Geometry);
                FArrangedChildren Children(EVisibility::Visible); Widget->ArrangeChildren(Geometry, Children);
                for (int32 Index = 0; Index < Children.Num(); Index++) Arrange(Children[Index].Widget, Children[Index].Geometry);
            };
            Arrange(Slate, FGeometry::MakeRoot(FVector2D(Width, Height), FSlateLayoutTransform()));
            int32 GeometryFailures = 0;
            for (const auto& ExpectedNode : Pair.Value.Nodes)
            {
                USvgCanvasNode* Node = Screen->FindSourceNode(ExpectedNode.Id);
                if (ExpectedNode.Visibility != TEXT("visible"))
                {
                    bool Visible = true;
                    for (const UWidget* Ancestor = Node; Ancestor; Ancestor = Ancestor->GetParent()) if (Ancestor->GetVisibility() == ESlateVisibility::Collapsed || Ancestor->GetVisibility() == ESlateVisibility::Hidden) { Visible = false; break; }
                    if (Visible) { Error = TEXT("Inactive source node remains visible in ") + Pair.Key + TEXT(": ") + ExpectedNode.Id; delete Renderer; return false; }
                    InactiveVisibilityChecks++; continue;
                }
                const TSharedPtr<SWidget> Widget = Node->GetCachedWidget();
                const FGeometry* Arranged = Widget ? ArrangedGeometry.Find(Widget.Get()) : nullptr;
                if (!Arranged) { Error = TEXT("Visible source node is missing from native Slate arrangement: ") + ExpectedNode.Id; delete Renderer; return false; }
                const FGeometry Geometry = *Arranged; ArrangedLayoutNodes++;
                const FVector2D Position = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector)), Size = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize())) - Position;
                const FVector4 Actual(Position.X, Position.Y, Size.X, Size.Y), Delta = Actual - ExpectedNode.Rect;
                const double NodeError = FMath::Max(FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y)), FMath::Max(FMath::Abs(Delta.Z), FMath::Abs(Delta.W)));
                if (NodeError > .01 && GeometryFailures++ < 12) UE_LOG(LogSvgCanvasBridge, Error, TEXT("Layout %s / %s parent=%s expected=(%.3f,%.3f,%.3f,%.3f) actual=(%.3f,%.3f,%.3f,%.3f) authored=%d"), *Pair.Key, *ExpectedNode.Id, *ExpectedNode.ParentId, ExpectedNode.Rect.X, ExpectedNode.Rect.Y, ExpectedNode.Rect.Z, ExpectedNode.Rect.W, Actual.X, Actual.Y, Actual.Z, Actual.W, Node->AuthoredLayout);
                MaxLayoutError = FMath::Max(MaxLayoutError, FMath::Max(FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y)), FMath::Max(FMath::Abs(Delta.Z), FMath::Abs(Delta.W))));
                if (!Node->Input) continue;
                auto Matches = [&](const FSvgCanvasRaster& Raster) { return Raster.Texture ? Node->Surface->GetBrush().GetResourceObject() == Raster.Texture : Node->Surface->GetVisibility() == ESlateVisibility::Collapsed; };
                Node->Input->OnHovered.Broadcast(); if (!Matches(Node->Hovered.Texture ? Node->Hovered : Node->Normal)) { Error = TEXT("Native hover binding differs"); delete Renderer; return false; }
                Node->Input->OnPressed.Broadcast(); if (!Matches(Node->Pressed.Texture ? Node->Pressed : Node->Normal)) { Error = TEXT("Native pressed binding differs"); delete Renderer; return false; }
                Node->SetInteractionEnabled(false); if (Node->Input->GetIsEnabled() || !Matches(Node->Disabled.Texture ? Node->Disabled : Node->Normal)) { Error = TEXT("Native disabled binding differs"); delete Renderer; return false; }
                Node->SetInteractionEnabled(ExpectedNode.Interactable); Node->Unhover(); ButtonStateChecks++;
            }
            TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
            if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) { Error = TEXT("Native Slate readback failed"); delete Renderer; return false; }
            TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
            if (!FFileHelper::SaveArrayToFile(Png, *(Output / (Pair.Key + TEXT(".png"))))) { Error = TEXT("Could not write native capture"); delete Renderer; return false; }
            if (MaxLayoutError > 0.01) { Error = FString::Printf(TEXT("Native layout %s differs by %.6f pixels; raw capture preserved"), *Pair.Key, MaxLayoutError); delete Renderer; return false; }
            auto Value = MakeShared<FJsonObject>(); Value->SetStringField(TEXT("state"), Pair.Key); Value->SetNumberField(TEXT("width"), Width); Value->SetNumberField(TEXT("height"), Height); Captures.Add(MakeShared<FJsonValueObject>(Value));
        }
        if (Screen->SourceDocument->States.Contains(TEXT("selected-2")) && Screen->SourceDocument->States.Contains(TEXT("picker-2")))
        {
            if (!Screen->HandleInputKey(EKeys::Two) || Screen->CurrentState != TEXT("selected-2")) { Error = TEXT("Digit selection must leave the picker closed"); delete Renderer; return false; }
            USvgCanvasNode* Ability = nullptr;
            Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
            {
                USvgCanvasNode* Node = Cast<USvgCanvasNode>(Widget); TSharedPtr<FJsonObject> Action;
                if (Node && Node->Input && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Node->ActionJson), Action) && Action && String(Action, TEXT("type")) == TEXT("ability") && Number(Action, TEXT("value")) == 1) Ability = Node;
            });
            if (!Ability) { Error = TEXT("Second ability has no native button"); delete Renderer; return false; }
            Ability->Input->OnClicked.Broadcast(); if (Screen->CurrentState != TEXT("picker-2")) { Error = TEXT("Native click did not open picker 2"); delete Renderer; return false; }
            Ability->Input->OnClicked.Broadcast(); if (Screen->CurrentState != TEXT("selected-2")) { Error = TEXT("Native click did not retain selection while closing picker"); delete Renderer; return false; }
        }
        int32 NavigationChecks = 0;
        if (Screen->SourceDocument->States.Contains(TEXT("atlas")))
        {
            Screen->ApplySceneState(TEXT("canvas"));
            if (!Screen->HandleInputKey(EKeys::M) || Screen->CurrentState != TEXT("atlas")) { Error = TEXT("M did not open the imported atlas"); delete Renderer; return false; }
            if (!Screen->HandleInputKey(EKeys::M) || Screen->CurrentState == TEXT("atlas")) { Error = TEXT("M did not close the imported atlas"); delete Renderer; return false; }
            NavigationChecks += 2;
        }
        Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            if (!Error.IsEmpty()) return;
            USvgCanvasNode* Node = Cast<USvgCanvasNode>(Widget); TSharedPtr<FJsonObject> Action;
            if (!Node || !Node->Input || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Node->ActionJson), Action) || !Action) return;
            const FString Type = String(Action, TEXT("type"));
            if (Type != TEXT("function") && !(Type == TEXT("battle-ui") && String(Action, TEXT("op")) == TEXT("navigate"))) return;
            FString Destination = String(Action, TEXT("value"));
            Screen->HandleInputKey(EKeys::One);
            if (Destination == TEXT("normal")) Destination = TEXT("selected-1");
            if (Destination == TEXT("skills")) Destination = TEXT("picker-1");
            if (Destination == TEXT("map")) Destination = TEXT("map-equipment");
            if (Destination == TEXT("lineage")) Destination = TEXT("map-lineage");
            if (!Screen->SourceDocument->States.Contains(Destination)) { Error = TEXT("Navigation references a missing exported interface: ") + Destination; return; }
            Node->Input->OnClicked.Broadcast();
            if (Screen->CurrentState != Destination) { Error = TEXT("Native navigation did not open interface: ") + Destination; return; }
            NavigationChecks++;
        });
        if (!Error.IsEmpty()) { delete Renderer; return false; }
        TStrongObjectPtr<USvgCanvasScreen> Authored(NewObject<USvgCanvasScreen>(GetTransientPackage(), Blueprint->GeneratedClass)); Authored->SetDesignerFlags(EWidgetDesignFlags::None); Authored->Initialize();
        USvgCanvasNode* AuthoredButton = nullptr;
        Authored->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            USvgCanvasNode* Node = Cast<USvgCanvasNode>(Widget); TSharedPtr<FJsonObject> Action;
            if (Node && Node->Input && Node->Hovered.Texture && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Node->ActionJson), Action) && Action && String(Action, TEXT("type")) == TEXT("ability") && Number(Action, TEXT("value")) == 1) AuthoredButton = Node;
        });
        bool AuthoredOverridesVerified = false;
        if (AuthoredButton && Authored->SourceDocument->States.Contains(TEXT("picker-2")))
        {
            UCanvasPanelSlot* EditedSlot = CastChecked<UCanvasPanelSlot>(AuthoredButton->Slot); FMargin EditedOffsets = EditedSlot->GetOffsets(); EditedOffsets.Left += 17; EditedSlot->SetOffsets(EditedOffsets);
            AuthoredButton->Normal = AuthoredButton->Hovered; AuthoredButton->ApplyRaster(AuthoredButton->Normal); UTexture2D* EditedTexture = AuthoredButton->Normal.Texture;
            AuthoredButton->SetVisibility(ESlateVisibility::Hidden);
            TSharedRef<SWidget> AuthoredSlate = Authored->TakeWidget(); Authored->ApplySceneState(TEXT("picker-2"));
            AuthoredOverridesVerified = EditedSlot->GetOffsets() == EditedOffsets && AuthoredButton->Surface->GetBrush().GetResourceObject() == EditedTexture && AuthoredButton->GetVisibility() == ESlateVisibility::Hidden;
            Authored->ReleaseSlateResources(true); AuthoredSlate = SNullWidget::NullWidget;
            if (!AuthoredOverridesVerified) { Error = TEXT("Independent WBP edits were overwritten by source state playback"); delete Renderer; return false; }
        }
        // Change the allocated screen itself. Scaling a fixed screenshot cannot pass these checks.
        TArray<TSharedPtr<FJsonValue>> ResponsiveChecks;
        const TArray<FIntPoint> ScreenSizes = { FIntPoint(1280, 720), FIntPoint(1024, 768), FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(2258, 1080) };
        for (const FIntPoint Size : ScreenSizes) for (const FString State : { FString(TEXT("canvas")), FString(TEXT("picker-2")) })
        {
            if (!Screen->SourceDocument->States.Contains(State)) continue;
            Screen->ApplySceneState(State);
            Target->InitCustomFormat(Size.X, Size.Y, PF_B8G8R8A8, true); Target->UpdateResourceImmediate(true); FlushRenderingCommands();
            for (int32 Pass = 0; Pass < 3; Pass++) { Renderer->DrawWidget(Target.Get(), Slate, 1, FVector2D(Size), 0, false); FlushRenderingCommands(); }
            const FGeometry Root = Screen->GetRootWidget()->GetCachedGeometry();
            auto GeometryRect = [&](const UWidget* Widget)
            {
                const FGeometry Geometry = Widget->GetCachedGeometry(); const FVector2D Position = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
                return FVector4(Position.X, Position.Y, Geometry.GetLocalSize().X, Geometry.GetLocalSize().Y);
            };
            double LayoutError = 0; int32 PrefabRoots = 0, ScreenAnchors = 0;
            auto Check = [&](const FVector4& Actual, const FVector4& Wanted) { const FVector4 Delta = Actual - Wanted; for (int32 Axis = 0; Axis < 4; Axis++) LayoutError = FMath::Max(LayoutError, FMath::Abs(Delta[Axis])); };
            Check(GeometryRect(Screen->GetRootWidget()), FVector4(0, 0, Size.X, Size.Y));
            const auto& Scene = Screen->SourceDocument->States[State];
            TArray<TSharedPtr<FJsonValue>> AnchorRects;
            for (const auto& Value : Scene.Nodes)
            {
                if (Value.Visibility != TEXT("visible")) continue;
                const USvgCanvasNode* Node = Screen->FindSourceNode(Value.Id); if (!Node) continue;
                const USvgCanvasNode* Parent = Cast<USvgCanvasNode>(Node->GetParent());
                if (!Parent)
                {
                    if (Node->Normal.Texture && Node->Normal.PixelOrigin.Equals(FVector2D(Node->DesignRect.X, Node->DesignRect.Y), .01) && Node->Normal.PixelSize.Equals(FVector2D(Node->DesignRect.Z, Node->DesignRect.W), .01)) Check(GeometryRect(Node->Surface), FVector4(0, 0, Size.X, Size.Y));
                    continue;
                }
                const FVector4 ParentRect = GeometryRect(Parent);
                if (Parent->SourceType == TEXT("PrefabInstance"))
                {
                    const FVector4 Insets = Node->DesignRect - Parent->DesignRect;
                    Check(GeometryRect(Node), FVector4(ParentRect.X + Insets.X, ParentRect.Y + Insets.Y, ParentRect.Z + Insets.Z, ParentRect.W + Insets.W)); PrefabRoots++;
                }
                if (Parent->SourceType == TEXT("CanvasPanel") && Parent->DesignRect.Equals(FVector4(0, 0, Width, Height), .01))
                {
                    const FAnchors A = Node->SourceAnchors; const double Dx = Size.X - Width, Dy = Size.Y - Height;
                    const FVector4 Wanted(Node->DesignRect.X + Dx * A.Minimum.X, Node->DesignRect.Y + Dy * A.Minimum.Y, Node->DesignRect.Z + Dx * (A.Maximum.X - A.Minimum.X), Node->DesignRect.W + Dy * (A.Maximum.Y - A.Minimum.Y));
                    const FVector4 Actual = GeometryRect(Node); Check(Actual, Wanted); ScreenAnchors++;
                    auto Detail = MakeShared<FJsonObject>(); Detail->SetStringField(TEXT("id"), Node->SourceId); Detail->SetNumberField(TEXT("x"), Actual.X); Detail->SetNumberField(TEXT("y"), Actual.Y); Detail->SetNumberField(TEXT("width"), Actual.Z); Detail->SetNumberField(TEXT("height"), Actual.W); AnchorRects.Add(MakeShared<FJsonValueObject>(Detail));
                }
            }
            if (LayoutError > .01) { Error = FString::Printf(TEXT("Responsive %dx%d %s differs by %.6f screen pixels"), Size.X, Size.Y, *State, LayoutError); delete Renderer; return false; }
            TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
            if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) { Error = TEXT("Responsive Slate readback failed"); delete Renderer; return false; }
            const FString CaptureName = FString::Printf(TEXT("viewport-%dx%d-%s.png"), Size.X, Size.Y, *State); TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
            if (!FFileHelper::SaveArrayToFile(Png, *(Output / CaptureName))) { Error = TEXT("Could not save responsive native capture"); delete Renderer; return false; }
            auto Result = MakeShared<FJsonObject>(); Result->SetNumberField(TEXT("width"), Size.X); Result->SetNumberField(TEXT("height"), Size.Y); Result->SetStringField(TEXT("state"), State); Result->SetStringField(TEXT("capture"), CaptureName); Result->SetNumberField(TEXT("maxLayoutErrorPixels"), LayoutError); Result->SetNumberField(TEXT("prefabRootsChecked"), PrefabRoots); Result->SetNumberField(TEXT("screenAnchorsChecked"), ScreenAnchors); Result->SetArrayField(TEXT("screenWidgets"), AnchorRects); ResponsiveChecks.Add(MakeShared<FJsonValueObject>(Result));
        }
        TArray<TSharedPtr<FJsonValue>> ResponsiveSceneChecks;
        const TSharedPtr<FJsonObject>* ResponsiveLayouts = nullptr;
        if (Bundle->TryGetObjectField(TEXT("responsiveLayouts"), ResponsiveLayouts)) for (const auto& Page : (*ResponsiveLayouts)->Values)
        {
            const FString State(Page.Key.ToView());
            if (!Screen->ApplySceneState(State)) { Error = TEXT("Missing responsive interface: ") + State; delete Renderer; return false; }
            for (const auto& Fixture : Page.Value->AsArray())
            {
                const auto ExpectedLayout = Fixture->AsObject(); const int32 W = Number(ExpectedLayout, TEXT("width")), H = Number(ExpectedLayout, TEXT("height"));
                TMap<const SWidget*, FGeometry> GeometryMap;
                TFunction<void(const TSharedRef<SWidget>&, const FGeometry&)> Arrange;
                Arrange = [&](const TSharedRef<SWidget>& Widget, const FGeometry& Geometry)
                {
                    GeometryMap.Add(&Widget.Get(), Geometry); FArrangedChildren Children(EVisibility::Visible); Widget->ArrangeChildren(Geometry, Children);
                    for (int32 Index = 0; Index < Children.Num(); ++Index) Arrange(Children[Index].Widget, Children[Index].Geometry);
                };
                Arrange(Slate, FGeometry::MakeRoot(FVector2D(W, H), FSlateLayoutTransform()));
                double LayoutError = 0; int32 Count = 0, Inputs = 0, Backdrops = 0;
                auto NativeRect = [&](const UWidget* Node, FVector4& Result)
                {
                    const auto Widget = Node->GetCachedWidget(); const FGeometry* Geometry = Widget ? GeometryMap.Find(Widget.Get()) : nullptr;
                    if (!Geometry) return false;
                    const FVector2D Position = Geometry->LocalToAbsolute(FVector2D::ZeroVector), Size = Geometry->LocalToAbsolute(Geometry->GetLocalSize()) - Position;
                    Result = FVector4(Position.X, Position.Y, Size.X, Size.Y); return true;
                };
                auto Check = [&](const FVector4& Actual, const FVector4& Wanted, const FString& Id)
                {
                    const FVector4 Delta = Actual - Wanted; double Difference = 0;
                    for (int32 Axis = 0; Axis < 4; ++Axis) Difference = FMath::Max(Difference, FMath::Abs(Delta[Axis]));
                    if (Difference > .01 && LayoutError <= .01) UE_LOG(LogSvgCanvasBridge, Error, TEXT("Responsive %s %dx%d %s expected=(%.3f,%.3f,%.3f,%.3f) actual=(%.3f,%.3f,%.3f,%.3f)"), *State, W, H, *Id, Wanted.X, Wanted.Y, Wanted.Z, Wanted.W, Actual.X, Actual.Y, Actual.Z, Actual.W);
                    LayoutError = FMath::Max(LayoutError, Difference);
                };
                for (const auto& Value : ExpectedLayout->GetArrayField(TEXT("nodes")))
                {
                    const auto ExpectedNode = Value->AsObject(); const FString Id = String(ExpectedNode, TEXT("id")); const USvgCanvasNode* Node = Screen->FindSourceNode(Id); FVector4 Actual;
                    if (!Node || !NativeRect(Node, Actual)) { Error = TEXT("Responsive source node missing from Slate: ") + State + TEXT(" / ") + Id; delete Renderer; return false; }
                    Check(Actual, Rect(ExpectedNode), Id); Count++;
                    if (Node->Input)
                    {
                        FVector4 InputRect; if (!NativeRect(Node->Input, InputRect)) { Error = TEXT("Missing responsive input geometry"); delete Renderer; return false; }
                        Check(InputRect, Actual, Id + TEXT(" input")); Inputs++;
                    }
                    // A source screen-sized visual must fill the allocated screen too.
                    if (Node->Surface && Node->Normal.Texture && Node->DesignRect.Equals(FVector4(0, 0, Width, Height), .01)
                        && Node->Normal.PixelOrigin.Equals(FVector2D::ZeroVector, .01) && Node->Normal.PixelSize.Equals(FVector2D(Width, Height), .01))
                    {
                        FVector4 Visual; if (!NativeRect(Node->Surface, Visual)) { Error = TEXT("Missing responsive backdrop"); delete Renderer; return false; }
                        Check(Visual, FVector4(0, 0, W, H), Id + TEXT(" backdrop")); Backdrops++;
                    }
                }
                const bool Capture = State == TEXT("character") || State == TEXT("inventory") || State == TEXT("shop") || State == TEXT("atlas") || State == TEXT("skill-detail");
                FString CaptureName;
                if (Capture)
                {
                    Target->InitCustomFormat(W, H, PF_B8G8R8A8, true); Target->UpdateResourceImmediate(true); FlushRenderingCommands();
                    for (int32 Pass = 0; Pass < 3; ++Pass) { Renderer->DrawWidget(Target.Get(), Slate, 1, FVector2D(W, H), 0, false); FlushRenderingCommands(); }
                    TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
                    if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) { Error = TEXT("Responsive dialog readback failed"); delete Renderer; return false; }
                    CaptureName = FString::Printf(TEXT("dialog-%dx%d-%s.png"), W, H, *State); TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(W, H, Pixels, Png);
                    if (!FFileHelper::SaveArrayToFile(Png, *(Output / CaptureName))) { Error = TEXT("Could not save responsive dialog capture"); delete Renderer; return false; }
                }
                if (LayoutError > .01) { Error = FString::Printf(TEXT("Responsive dialog %s %dx%d differs by %.6f pixels; raw capture preserved"), *State, W, H, LayoutError); delete Renderer; return false; }
                auto Result = MakeShared<FJsonObject>(); Result->SetStringField(TEXT("state"), State); Result->SetNumberField(TEXT("width"), W); Result->SetNumberField(TEXT("height"), H);
                Result->SetNumberField(TEXT("nodesChecked"), Count); Result->SetNumberField(TEXT("inputsChecked"), Inputs); Result->SetNumberField(TEXT("backdropsChecked"), Backdrops); Result->SetNumberField(TEXT("maxLayoutErrorPixels"), LayoutError);
                if (!CaptureName.IsEmpty()) Result->SetStringField(TEXT("capture"), CaptureName); ResponsiveSceneChecks.Add(MakeShared<FJsonValueObject>(Result));
            }
        }
        Target->InitCustomFormat(Width, Height, PF_B8G8R8A8, true); Target->UpdateResourceImmediate(true); FlushRenderingCommands();
        TStrongObjectPtr<USvgCanvasScreen> Designer(NewObject<USvgCanvasScreen>(GetTransientPackage(), Blueprint->GeneratedClass));
        Designer->SetDesignerFlags(EWidgetDesignFlags::Designing);
        if (!Designer->Initialize()) { Error = TEXT("Saved Designer defaults could not initialize"); delete Renderer; return false; }
        TSharedRef<SWidget> DesignerSlate = Designer->TakeWidget();
        for (int32 Pass = 0; Pass < 3; Pass++) { Renderer->DrawWidget(Target.Get(), DesignerSlate, 1, FVector2D(Width, Height), 0, false); FlushRenderingCommands(); }
        TArray<FColor> DesignerPixels; FReadSurfaceDataFlags DesignerFlags(RCM_UNorm); DesignerFlags.SetLinearToGamma(false);
        if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(DesignerPixels, DesignerFlags)) { Error = TEXT("Saved Designer readback failed"); delete Renderer; return false; }
        TArray64<uint8> DesignerPng; FImageUtils::PNGCompressImageArray(Width, Height, DesignerPixels, DesignerPng);
        if (!FFileHelper::SaveArrayToFile(DesignerPng, *(Output / TEXT("designer.png")))) { Error = TEXT("Could not save Designer capture"); delete Renderer; return false; }
        Designer->ReleaseSlateResources(true); DesignerSlate = SNullWidget::NullWidget;
        Screen->ReleaseSlateResources(true); Slate = SNullWidget::NullWidget; delete Renderer;
        auto Receipt = MakeShared<FJsonObject>(); Receipt->SetBoolField(TEXT("ok"), true); Receipt->SetBoolField(TEXT("reloadedInSeparateProcess"), true); Receipt->SetBoolField(TEXT("hierarchyMatches"), true);
        Receipt->SetStringField(TEXT("sourceHash"), Screen->SourceDocument->SourceHash); Receipt->SetNumberField(TEXT("logicalNodes"), ActualParents.Num()); Receipt->SetNumberField(TEXT("nativeButtons"), InputCount); Receipt->SetArrayField(TEXT("captures"), Captures);
        Receipt->SetBoolField(TEXT("pixelAcceptancePassed"), false); Receipt->SetStringField(TEXT("pixelAcceptanceNote"), TEXT("External reference comparison required. Rendering alone does not certify equality."));
        Receipt->SetStringField(TEXT("verificationHost"), IsRunningCommandlet() ? TEXT("commandlet") : TEXT("editor"));
        Receipt->SetNumberField(TEXT("maxLayoutErrorPixels"), MaxLayoutError); Receipt->SetNumberField(TEXT("nativeButtonStateChecks"), ButtonStateChecks);
        Receipt->SetNumberField(TEXT("nativeNavigationChecks"), NavigationChecks);
        Receipt->SetNumberField(TEXT("nativeArrangedLayoutNodesChecked"), ArrangedLayoutNodes);
        Receipt->SetNumberField(TEXT("inactiveVisibilityChecks"), InactiveVisibilityChecks);
        Receipt->SetBoolField(TEXT("digitSelectionAndNativeClickVerified"), Screen->SourceDocument->States.Contains(TEXT("selected-2")));
        Receipt->SetBoolField(TEXT("savedDesignerDefaultsRendered"), true);
        Receipt->SetBoolField(TEXT("independentWbpEditsPreserved"), AuthoredOverridesVerified);
        Receipt->SetBoolField(TEXT("responsiveViewportVerified"), !ResponsiveChecks.IsEmpty()); Receipt->SetArrayField(TEXT("responsiveViewportChecks"), ResponsiveChecks);
        Receipt->SetArrayField(TEXT("responsiveSceneChecks"), ResponsiveSceneChecks);
        return WriteJson(Output / TEXT("reload-receipt.json"), Receipt);
    }
}

USvgCanvasImportCommandlet::USvgCanvasImportCommandlet() { IsEditor = true; IsClient = false; IsServer = false; LogToConsole = true; }
USvgCanvasVerifyCommandlet::USvgCanvasVerifyCommandlet() { IsEditor = true; IsClient = false; IsServer = false; LogToConsole = true; }
int32 USvgCanvasImportCommandlet::Main(const FString& Params)
{
    FString Bundle, Output, Error; FParse::Value(*Params, TEXT("Bundle="), Bundle); FParse::Value(*Params, TEXT("Output="), Output);
    if (Output.IsEmpty()) Output = FPaths::ProjectSavedDir() / TEXT("SvgCanvas/import-receipt.json");
    if (!SvgCanvasImport::Import(Bundle, Output, Error)) { UE_LOG(LogSvgCanvasBridge, Error, TEXT("%s"), *Error); auto Result = MakeShared<FJsonObject>(); Result->SetBoolField(TEXT("ok"), false); Result->SetStringField(TEXT("error"), Error); SvgCanvasImport::WriteJson(Output, Result); return 1; }
    UE_LOG(LogSvgCanvasBridge, Display, TEXT("Imported and saved SVG canvas. Receipt: %s"), *Output); return 0;
}
int32 USvgCanvasVerifyCommandlet::Main(const FString& Params)
{
    FString Bundle, Output, Error; FParse::Value(*Params, TEXT("Bundle="), Bundle); FParse::Value(*Params, TEXT("Output="), Output);
    if (Output.IsEmpty()) Output = FPaths::ProjectSavedDir() / TEXT("SvgCanvas/native");
    if (!SvgCanvasImport::Verify(Bundle, Output, Error)) { UE_LOG(LogSvgCanvasBridge, Error, TEXT("%s"), *Error); return 1; }
    UE_LOG(LogSvgCanvasBridge, Display, TEXT("Reloaded and rendered WBP: %s"), *Output); return 0;
}
