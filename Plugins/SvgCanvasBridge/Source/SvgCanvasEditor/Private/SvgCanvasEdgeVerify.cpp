#include "SvgCanvasEdgeVerifyCommandlet.h"
#include "SvgCanvasMotionNode.h"
#include "SvgCanvasScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureCompiler.h"
#include "ImageUtils.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

USvgCanvasEdgeVerifyCommandlet::USvgCanvasEdgeVerifyCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 USvgCanvasEdgeVerifyCommandlet::Main(const FString& Params)
{
 FString Root=TEXT("/Game/UICreatorNotices"),Output;FParse::Value(*Params,TEXT("SvgCanvasRoot="),Root);FParse::Value(*Params,TEXT("Output="),Output);
 if(Output.IsEmpty())Output=FPaths::ProjectSavedDir()/TEXT("Verification/InventoryNotices/native");
 auto Fail=[](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("EDGE_VERIFY_FAIL %s"),Why);return 1;};
 UClass* Class=LoadClass<USvgCanvasScreen>(nullptr,*(Root+TEXT("/WBP_CurrentCanvas.WBP_CurrentCanvas_C")));
 if(!Class)return Fail(TEXT("saved WBP missing"));
 TStrongObjectPtr<USvgCanvasScreen> Screen(NewObject<USvgCanvasScreen>(GetTransientPackage(),Class));Screen->SetDesignerFlags(EWidgetDesignFlags::None);
 if(!Screen->Initialize()||!Screen->SourceDocument)return Fail(TEXT("source document missing"));
 TArray<UTexture*> Textures;for(const auto& Pair:Screen->SourceDocument->Rasters)if(Pair.Value.Texture)Textures.AddUnique(Pair.Value.Texture);
 FTextureCompilingManager::Get().FinishCompilation(Textures);FlushRenderingCommands();
 if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
 const FVector2D Size=Screen->SourceDocument->DesignSize;auto Slate=Screen->TakeWidget();
 FWidgetRenderer Renderer(true,true);TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size,TF_Nearest,true));
 Target->InitCustomFormat(Size.X,Size.Y,PF_B8G8R8A8,true);Target->UpdateResourceImmediate(true);IFileManager::Get().MakeDirectory(*Output,true);
 auto Draw=[&](const TCHAR* Name,TArray<FColor>& Pixels){for(int32 I=0;I<3;I++){Renderer.DrawWidget(Target.Get(),Slate,1,Size,0,false);FlushRenderingCommands();}FReadSurfaceDataFlags Flags(RCM_UNorm);Flags.SetLinearToGamma(false);if(!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))return false;TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);return FFileHelper::SaveArrayToFile(Png,*(Output/(FString(Name)+TEXT(".png"))));};
 TArray<USvgCanvasMotionNode*> Edges;Screen->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<USvgCanvasMotionNode>(W);N&&N->Binding==TEXT("row:new"))Edges.Add(N);});
 if(Edges.Num()!=4)return Fail(TEXT("four saved edge nodes expected"));
 TArray<UButton*> Inputs;TArray<FMargin> Offsets;TArray<FVector4> Rects;
 auto Rect=[&](UWidget* W){const auto& G=W->GetCachedGeometry();const FVector2D P=Screen->GetRootWidget()->GetCachedGeometry().AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector)),S=G.GetLocalSize();return FVector4(P.X,P.Y,S.X,S.Y);};
 TArray<FColor> Before,A,B,Seen;
 for(auto* N:Edges){FString Error;if(!N->MotionAsset||!N->MotionAsset->Decode(Error)||!N->MotionAsset->EdgeOnly||!N->EdgeOverlay)return Fail(TEXT("shared edge asset did not reload"));N->SetMotionEnabled(false);}
 if(!Draw(TEXT("00-baseline"),Before))return 2;
 for(auto* N:Edges){UButton* Input=nullptr;for(UWidget* P=N;P;P=P->GetParent())if(auto* Owner=Cast<USvgCanvasNode>(P);Owner&&Owner->Input){Input=Owner->Input;break;}if(!Input)return Fail(TEXT("input owner missing"));Inputs.Add(Input);Offsets.Add(CastChecked<UCanvasPanelSlot>(N->Slot)->GetOffsets());Rects.Add(Rect(Input));N->SetMotionEnabled(true);N->PlayMotion(TEXT("normal"));N->AdvanceMotionForValidation(.65f);}
 if(!Draw(TEXT("01-phase-90"),A))return 3;
 for(auto* N:Edges)N->AdvanceMotionForValidation(.65f);
 if(!Draw(TEXT("02-phase-180"),B))return 4;
 int32 Changed=0,ChangedBetweenFrames=0,InteriorChanges=0;
 for(int32 I=0;I<A.Num();I++)
 {
  if(A[I]!=Before[I])Changed++;if(A[I]!=B[I])ChangedBetweenFrames++;
  const int32 X=I%int32(Size.X),Y=I/int32(Size.X);
  for(const auto& R:Rects)if(X>R.X+5&&X<R.X+R.Z-5&&Y>R.Y+5&&Y<R.Y+R.W-5&&A[I]!=Before[I])InteriorChanges++;
 }
 if(Changed==0||ChangedBetweenFrames==0||InteriorChanges!=0)return Fail(TEXT("edge must move without changing item interiors"));
 Inputs[0]->OnHovered.Broadcast();Inputs[0]->OnUnhovered.Broadcast();Edges[0]->AdvanceMotionForValidation(.3f);
 if(Edges[0]->MotionEnabled)return Fail(TEXT("hover did not consume the prompt"));
 if(!Draw(TEXT("03-first-seen"),Seen))return 5;
 for(int32 I=0;I<Edges.Num();I++)if(!(CastChecked<UCanvasPanelSlot>(Edges[I]->Slot)->GetOffsets()==Offsets[I])||Rect(Inputs[I])!=Rects[I])return Fail(TEXT("layout or input geometry changed"));
 auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("ok"),true);Result->SetNumberField(TEXT("savedEdgeNodes"),Edges.Num());Result->SetNumberField(TEXT("edgePixels"),Changed);Result->SetNumberField(TEXT("movingPixels"),ChangedBetweenFrames);Result->SetNumberField(TEXT("interiorChanges"),InteriorChanges);Result->SetBoolField(TEXT("hoverConsumes"),true);Result->SetBoolField(TEXT("layoutAndHitAreaUnchanged"),true);
 FString Json;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Output/TEXT("edge-receipt.json")));
 UE_LOG(LogTemp,Display,TEXT("EDGE_VERIFY_PASS moving=%d interior=%d"),ChangedBetweenFrames,InteriorChanges);return 0;
}
