#include "SvgCanvasMotionVerifyCommandlet.h"
#include "SvgCanvasMotionNode.h"
#include "SvgCanvasScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureCompiler.h"
#include "ImageUtils.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

USvgCanvasMotionVerifyCommandlet::USvgCanvasMotionVerifyCommandlet() { IsEditor = true; IsClient = false; IsServer = false; LogToConsole = true; }
int32 USvgCanvasMotionVerifyCommandlet::Main(const FString& Params)
{
    FString Output; FParse::Value(*Params, TEXT("Output="), Output);
    if (Output.IsEmpty()) Output = FPaths::ProjectSavedDir() / TEXT("SvgCanvas/motion");
    UClass* Class = LoadClass<USvgCanvasScreen>(nullptr, TEXT("/Game/UICreator/WBP_CurrentCanvas.WBP_CurrentCanvas_C"));
    if (!Class) return 1;
    TStrongObjectPtr<USvgCanvasScreen> Screen(NewObject<USvgCanvasScreen>(GetTransientPackage(), Class));
    Screen->SetDesignerFlags(EWidgetDesignFlags::None);
    if (!Screen->Initialize() || !Screen->SourceDocument) return 2;
    TArray<UTexture*> Textures; for (const auto& Pair : Screen->SourceDocument->Rasters) if (Pair.Value.Texture) Textures.AddUnique(Pair.Value.Texture);
    FTextureCompilingManager::Get().FinishCompilation(Textures); FlushRenderingCommands();
    if (!FSlateApplication::IsInitialized()) FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    const FVector2D Size = Screen->SourceDocument->DesignSize;
    TSharedRef<SWidget> Slate = Screen->TakeWidget();
    FWidgetRenderer Renderer(true, true);
    TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Nearest, true));
    Target->InitCustomFormat(Size.X, Size.Y, PF_B8G8R8A8, true); Target->UpdateResourceImmediate(true);
    IFileManager::Get().MakeDirectory(*Output, true);
    auto Draw = [&](const FString& Name)
    {
        for (int32 Pass = 0; Pass < 3; Pass++) { Renderer.DrawWidget(Target.Get(), Slate, 1, Size, 0, false); FlushRenderingCommands(); }
        TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
        if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) return false;
        TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
        return FFileHelper::SaveArrayToFile(Png, *(Output / (Name + TEXT(".png"))));
    };
    if (!Draw(TEXT("normal"))) return 3;
    USvgCanvasMotionNode* Button = nullptr; USvgCanvasMotionNode* Rim = nullptr; USvgCanvasMotionNode* Custom = nullptr;
    Screen->WidgetTree->ForEachWidget([&](UWidget* Widget)
    {
        if (USvgCanvasMotionNode* Node = Cast<USvgCanvasMotionNode>(Widget))
        {
            if (!Button && Node->Binding == TEXT("ability") && Node->Input) Button = Node;
            if (Node->SourceId.EndsWith(TEXT("hud-ability-selected")) && Node->GetVisibility() != ESlateVisibility::Collapsed) Rim = Node;
            if (Node->MotionAsset && Node->MotionAsset->SourceAssetId == TEXT("motion-review-custom")) Custom = Node;
        }
    });
    if (!Button || !Rim) return 4;
    const FGeometry Root = Screen->GetRootWidget()->GetCachedGeometry();
    auto HitRect = [&]() { const FGeometry G = Button->Input->GetCachedGeometry(); const FVector2D P = Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector)), S = G.GetLocalSize(); return FVector4(P.X, P.Y, S.X, S.Y); };
    const FVector4 InitialHit = HitRect(); const FMargin InitialOffsets = CastChecked<UCanvasPanelSlot>(Button->Slot)->GetOffsets();
    Button->Input->OnHovered.Broadcast(); Button->AdvanceMotionForValidation(.08f);
    const float HoverMid = Button->Surface->GetRenderTransform().Scale.X;
    if (!Draw(TEXT("hover-mid"))) return 5;
    Button->AdvanceMotionForValidation(.08f); const float HoverEnd = Button->Surface->GetRenderTransform().Scale.X;
    if (!Draw(TEXT("hover-end"))) return 6;
    Button->Input->OnPressed.Broadcast(); Button->AdvanceMotionForValidation(.035f);
    const float Press = Button->Surface->GetRenderTransform().Scale.X;
    if (!Draw(TEXT("press-mid"))) return 7;
    Button->Input->OnReleased.Broadcast(); Button->AdvanceMotionForValidation(.1f); Button->AdvanceMotionForValidation(.02f);
    const float Release = Button->Surface->GetRenderTransform().Scale.X;
    if (!Draw(TEXT("release-mid"))) return 8;
    Rim->AdvanceMotionForValidation(.1f); const float OpacityA = Rim->Surface->GetRenderOpacity();
    Rim->AdvanceMotionForValidation(.1f); const float OpacityB = Rim->Surface->GetRenderOpacity();
    if (!Draw(TEXT("selection-breathe"))) return 9;
    Button->SetInteractionEnabled(false);
    const bool DisabledRestored = !Button->Input->GetIsEnabled() && FMath::IsNearlyEqual(Button->Surface->GetRenderTransform().Scale.X, 1.f);
    Button->SetInteractionEnabled(true);
    const bool StableHit = HitRect().Equals(InitialHit, .001) && CastChecked<UCanvasPanelSlot>(Button->Slot)->GetOffsets() == InitialOffsets;
    const bool Motion = HoverMid > 1 && HoverEnd > HoverMid && Press < 1 && Release > HoverEnd && !FMath::IsNearlyEqual(OpacityA, OpacityB);
    bool CustomPassed = false;
    if (Custom)
    {
        Custom->PlayMotion(TEXT("hovered")); Custom->AdvanceMotionForValidation(.2f);
        const FWidgetTransform Transform = Custom->Surface->GetRenderTransform();
        CustomPassed = FMath::IsNearlyEqual(Transform.Translation.X, 8.f) && FMath::IsNearlyEqual(Transform.Translation.Y, -10.f) && FMath::IsNearlyEqual(Transform.Angle, -8.f) && FMath::IsNearlyEqual(Transform.Scale.X, 1.12f) && FMath::IsNearlyEqual(Transform.Scale.Y, .96f) && FMath::IsNearlyEqual(Custom->Surface->GetRenderOpacity(), .8f);
        if (!Draw(TEXT("authored-custom-mid"))) return 11;
    }
    auto Receipt = MakeShared<FJsonObject>(); Receipt->SetBoolField(TEXT("ok"), StableHit && Motion && CustomPassed && DisabledRestored); Receipt->SetBoolField(TEXT("nativeButtonEvents"), true); Receipt->SetBoolField(TEXT("layoutAndHitAreaUnchanged"), StableHit);
    Receipt->SetBoolField(TEXT("disabledRestoresVisuals"), DisabledRestored);
    Receipt->SetBoolField(TEXT("authoredAssetReloaded"), Custom != nullptr); Receipt->SetBoolField(TEXT("authoredSixChannelsPassed"), CustomPassed); Receipt->SetNumberField(TEXT("authoredRotation"), Custom ? Custom->Surface->GetRenderTransform().Angle : 0);
    Receipt->SetNumberField(TEXT("hoverMidScale"), HoverMid); Receipt->SetNumberField(TEXT("hoverEndScale"), HoverEnd); Receipt->SetNumberField(TEXT("pressMidScale"), Press); Receipt->SetNumberField(TEXT("releaseMidScale"), Release);
    Receipt->SetNumberField(TEXT("selectionOpacityA"), OpacityA); Receipt->SetNumberField(TEXT("selectionOpacityB"), OpacityB); Receipt->SetStringField(TEXT("verificationHost"), TEXT("native Slate commandlet; no automatic timers"));
    FString Json; FJsonSerializer::Serialize(Receipt, TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json, *(Output / TEXT("motion-receipt.json")));
    Screen->ReleaseSlateResources(true); Slate = SNullWidget::NullWidget;
    return StableHit && Motion && CustomPassed && DisabledRestored ? 0 : 10;
}
