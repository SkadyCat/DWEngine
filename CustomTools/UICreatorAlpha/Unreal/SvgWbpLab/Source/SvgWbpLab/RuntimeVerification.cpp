#include "RuntimeVerification.h"
#include "SvgCanvasScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#include "Engine/Texture2D.h"
#include "RenderingThread.h"
#endif
#include "Layout/ArrangedChildren.h"
#include "Widgets/SWidget.h"

void StartSvgRuntimeVerification(UUserWidget* Widget)
{
    FString Output;
    if (!FParse::Value(FCommandLine::Get(), TEXT("SvgCanvasRuntimeOutput="), Output)) return;
    FString State = TEXT("canvas"), ExpectedFile;
    FParse::Value(FCommandLine::Get(), TEXT("SvgCanvasRuntimeState="), State);
    FParse::Value(FCommandLine::Get(), TEXT("SvgCanvasExpectedLayout="), ExpectedFile);
    IFileManager::Get().MakeDirectory(*Output, true);
    const TWeakObjectPtr<USvgCanvasScreen> Weak(Cast<USvgCanvasScreen>(Widget));
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Output, State, ExpectedFile, Elapsed = 0.f, Ready = false](float Delta) mutable
    {
        Elapsed += Delta; if (Elapsed < 2.f) return true;
        USvgCanvasScreen* Screen = Weak.Get(); if (!Screen || !Screen->SourceDocument || !GEngine || !GEngine->GameViewport || !GEngine->GameViewport->Viewport) return true;
        if (!Ready)
        {
            if (!Screen->ApplySceneState(State)) { FPlatformMisc::RequestExitWithStatus(false, 1); return false; }
#if WITH_EDITOR
            TArray<UTexture*> Textures; for (const auto& Pair : Screen->SourceDocument->Rasters) if (Pair.Value.Texture) Textures.AddUnique(Pair.Value.Texture);
            FTextureCompilingManager::Get().FinishCompilation(Textures); FlushRenderingCommands();
#endif
            Ready = true; Elapsed = 0; return true;
        }
        const FIntPoint Pixels = GEngine->GameViewport->Viewport->GetSizeXY();
        const float Dpi = UWidgetLayoutLibrary::GetViewportScale(Screen);
        const FGeometry Root = Screen->GetRootWidget()->GetCachedGeometry();
        // Source layout excludes visual animation. Measure layout transforms for
        // layout, and separately check the real painted input geometry below.
        auto LayoutRect = [&](const FGeometry& G)
        {
            const FSlateRect Bounds = G.GetLayoutBoundingRect(); const auto Transform = Root.GetAccumulatedLayoutTransform();
            const FVector2D P = (FVector2D(Bounds.Left, Bounds.Top) - FVector2D(Transform.GetTranslation())) / Transform.GetScale();
            const FVector2D S = FVector2D(Bounds.GetSize()) / Transform.GetScale(); return FVector4(P.X, P.Y, S.X, S.Y);
        };
        auto Rect = [&](const UWidget* Item) { return LayoutRect(Item->GetCachedGeometry()); };
        auto PaintedRect = [&](const UWidget* Item)
        {
            const FGeometry G = Item->GetCachedGeometry(); const FVector2D P = Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
            const FVector2D S = Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize())) - P; return FVector4(P.X, P.Y, S.X, S.Y);
        };
        double Error = 0; int32 PrefabCount = 0, AnchorCount = 0;
        auto Check = [&](const FVector4& Actual, const FVector4& Expected) { const FVector4 D = Actual - Expected; for (int32 Axis = 0; Axis < 4; Axis++) Error = FMath::Max(Error, FMath::Abs(D[Axis])); };
        const FVector2D Size = Root.GetLocalSize(), Design = Screen->SourceDocument->DesignSize;
        Check(Rect(Screen->GetRootWidget()), FVector4(0, 0, Pixels.X / Dpi, Pixels.Y / Dpi));
        const FSvgCanvasSceneState& Scene = Screen->SourceDocument->States[State];
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (const auto& Value : Scene.Nodes)
        {
            if (Value.Visibility != TEXT("visible")) continue;
            const USvgCanvasNode* Node = Screen->FindSourceNode(Value.Id); if (!Node) continue;
            const USvgCanvasNode* Parent = Cast<USvgCanvasNode>(Node->GetParent()); const FVector4 Actual = Rect(Node);
            if (!Parent && Node->Surface) Check(Rect(Node->Surface), FVector4(0, 0, Size.X, Size.Y));
            if (ExpectedFile.IsEmpty() && Parent && Parent->SourceType == TEXT("PrefabInstance"))
            {
                const FVector4 P = Rect(Parent), Inset = Node->DesignRect - Parent->DesignRect;
                Check(Actual, FVector4(P.X + Inset.X, P.Y + Inset.Y, P.Z + Inset.Z, P.W + Inset.W)); PrefabCount++;
            }
            if (ExpectedFile.IsEmpty() && Parent && Parent->SourceType == TEXT("CanvasPanel") && Parent->DesignRect.Equals(FVector4(0, 0, Design.X, Design.Y), .01))
            {
                const FAnchors A = Node->SourceAnchors; const double Dx = Size.X - Design.X, Dy = Size.Y - Design.Y;
                Check(Actual, FVector4(Node->DesignRect.X + Dx * A.Minimum.X, Node->DesignRect.Y + Dy * A.Minimum.Y, Node->DesignRect.Z + Dx * (A.Maximum.X - A.Minimum.X), Node->DesignRect.W + Dy * (A.Maximum.Y - A.Minimum.Y))); AnchorCount++;
            }
            auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("id"), Node->SourceId); Item->SetNumberField(TEXT("x"), Actual.X); Item->SetNumberField(TEXT("y"), Actual.Y); Item->SetNumberField(TEXT("width"), Actual.Z); Item->SetNumberField(TEXT("height"), Actual.W); Nodes.Add(MakeShared<FJsonValueObject>(Item));
        }
        int32 ExpectedNodes = 0, InputChecks = 0, PaintedInputChecks = 0;
        if (!ExpectedFile.IsEmpty())
        {
            FString Json; TSharedPtr<FJsonObject> Expected;
            if (!FFileHelper::LoadFileToString(Json, *ExpectedFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Expected)) { FPlatformMisc::RequestExitWithStatus(false, 1); return false; }
            TMap<const SWidget*, FGeometry> GeometryMap;
            TFunction<void(const TSharedRef<SWidget>&, const FGeometry&)> Arrange;
            Arrange = [&](const TSharedRef<SWidget>& Slate, const FGeometry& Geometry)
            {
                GeometryMap.Add(&Slate.Get(), Geometry); FArrangedChildren Children(EVisibility::Visible); Slate->ArrangeChildren(Geometry, Children);
                for (int32 Index = 0; Index < Children.Num(); ++Index) Arrange(Children[Index].Widget, Children[Index].Geometry);
            };
            Arrange(Screen->GetRootWidget()->GetCachedWidget().ToSharedRef(), Root);
            auto ArrangedRect = [&](const UWidget* Node)
            {
                const auto Slate = Node->GetCachedWidget(); const FGeometry* Geometry = Slate ? GeometryMap.Find(Slate.Get()) : nullptr;
                if (!Geometry) { Error = BIG_NUMBER; return FVector4(0,0,0,0); }
                return LayoutRect(*Geometry);
            };
            for (const auto& Item : Expected->GetArrayField(TEXT("nodes")))
            {
                const auto Value = Item->AsObject(); const USvgCanvasNode* Node = Screen->FindSourceNode(Value->GetStringField(TEXT("id")));
                if (!Node) { Error = BIG_NUMBER; continue; }
                const FVector4 Actual = ArrangedRect(Node), Wanted(Value->GetNumberField(TEXT("x")), Value->GetNumberField(TEXT("y")), Value->GetNumberField(TEXT("width")), Value->GetNumberField(TEXT("height")));
                Check(Actual, Wanted); ExpectedNodes++;
                // Also inspect the geometry used by paint/hit testing for on-screen controls.
                const FGeometry* G = GeometryMap.Find(Node->GetCachedWidget().Get());
                bool Painted = false;
                if (G)
                {
                    FSlateRect Clip = Root.GetLayoutBoundingRect();
                    for (const UWidget* Parent = Node->GetParent(); Parent; Parent = Parent->GetParent()) if (Parent->GetClipping() != EWidgetClipping::Inherit)
                        if (const FGeometry* ParentGeometry = GeometryMap.Find(Parent->GetCachedWidget().Get())) Clip = Clip.IntersectionWith(ParentGeometry->GetLayoutBoundingRect());
                    bool Overlap = false; G->GetLayoutBoundingRect().IntersectionWith(Clip, Overlap);
                    Painted = Overlap; if (Painted) Check(Rect(Node), Actual);
                }
                if (Node->Input)
                {
                    Check(ArrangedRect(Node->Input), Actual); InputChecks++;
                    if (Painted) { Check(PaintedRect(Node->Input), Actual); PaintedInputChecks++; }
                }
                if (Node->Surface && Node->Normal.Texture && Node->DesignRect.Equals(FVector4(0,0,Design.X,Design.Y), .01) && Node->Normal.PixelOrigin.IsNearlyZero() && Node->Normal.PixelSize.Equals(Design, .01)) Check(Rect(Node->Surface), FVector4(0,0,Size.X,Size.Y));
            }
        }
        auto Result = MakeShared<FJsonObject>(); Result->SetBoolField(TEXT("ok"), Error < .01); Result->SetStringField(TEXT("state"), State); Result->SetNumberField(TEXT("responsiveSourceNodesChecked"), ExpectedNodes); Result->SetNumberField(TEXT("responsiveInputsChecked"), InputChecks); Result->SetNumberField(TEXT("paintedInputBoundsChecked"), PaintedInputChecks); Result->SetStringField(TEXT("sourceHash"), Screen->SourceDocument->SourceHash); Result->SetStringField(TEXT("host"), TEXT("game-viewport")); Result->SetStringField(TEXT("geometryMeasurement"), TEXT("layout transforms, plus painted native input bounds; visual animation stays enabled")); Result->SetNumberField(TEXT("viewportWidth"), Pixels.X); Result->SetNumberField(TEXT("viewportHeight"), Pixels.Y); Result->SetNumberField(TEXT("dpiScale"), Dpi); Result->SetNumberField(TEXT("screenWidth"), Size.X); Result->SetNumberField(TEXT("screenHeight"), Size.Y); Result->SetNumberField(TEXT("maxLayoutErrorPixels"), Error); Result->SetNumberField(TEXT("prefabRootsChecked"), PrefabCount); Result->SetNumberField(TEXT("screenAnchorsChecked"), AnchorCount); Result->SetArrayField(TEXT("nodes"), Nodes);
        const FString Capture = Output / TEXT("game-viewport.png");
        FScreenshotRequest::OnScreenshotRequestProcessed().AddLambda([Output, Capture, Result]()
        {
            const bool Saved = IFileManager::Get().FileSize(*Capture) > 0; Result->SetBoolField(TEXT("screenshotSaved"), Saved); Result->SetBoolField(TEXT("ok"), Result->GetBoolField(TEXT("ok")) && Saved);
            FString Json; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json, *(Output / TEXT("runtime-receipt.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            FPlatformMisc::RequestExitWithStatus(false, Result->GetBoolField(TEXT("ok")) ? 0 : 1);
        });
        FScreenshotRequest::RequestScreenshot(Capture, true, false, false, FIntRect(), true);
        return false;
    }));
}
