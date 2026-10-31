#include "OpusWorldUI.h"
#include "OpusAuthoredUI.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/SkeletalMeshComponent.h"
#include "SvgCanvasImage.h"
#include "SvgCanvasMotionAsset.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Texture2D.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "GameFramework/PlayerController.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace
{
TMap<FString,TSharedPtr<FJsonObject>> Contracts;
TArray<TWeakObjectPtr<UOpusWorldScreen>> Widgets;
TMap<FString,TWeakObjectPtr<UTexture2D>> Textures;
TMap<TWeakObjectPtr<UOpusWorldScreen>,TMap<FString,TArray<TWeakObjectPtr<UOpusActionNode>>>> HoverBindings;
FString Str(const TSharedPtr<FJsonObject>& O,const TCHAR* K){FString V;O->TryGetStringField(K,V);return V;}
FString Json(const TSharedPtr<FJsonObject>& O){FString V;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&V));return V;}
FSvgCanvasRaster Raster(const TSharedPtr<FJsonObject>& Contract,const FString& Directory,const FString& Key,FVector2D Origin)
{
 FSvgCanvasRaster R;if(Key.IsEmpty())return R;
 const TSharedPtr<FJsonObject>* V=nullptr;if(!Contract->GetObjectField(TEXT("rasters"))->TryGetObjectField(Key,V))return R;const auto O=*V;
 auto& Texture=Textures.FindOrAdd(Directory+Key);if(!Texture.IsValid())
 {
  Texture=LoadObject<UTexture2D>(nullptr,*(TEXT("/Game/")+Directory+TEXT("/Rasters/T_")+Key.Left(40)+TEXT(".T_")+Key.Left(40)),nullptr,LOAD_NoWarn);
  // Older world overlays predate packaged exports. New loot must use saved UE assets.
  if(!Texture.IsValid()&&Directory!=TEXT("UICreatorEvents")&&Directory!=TEXT("UICreatorLoot")&&Directory!=TEXT("UICreatorTalents")&&Directory!=TEXT("UICreatorEffects")&&Directory!=TEXT("UICreatorRecent")&&Directory!=TEXT("UICreatorContext"))Texture=FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir()/Directory/TEXT("rasters")/(Key+TEXT(".png")));
  if(Texture.IsValid()){Texture->Filter=TF_Nearest;Texture->NeverStream=true;}
 }
 R.Texture=Texture.Get();R.PixelOrigin=FVector2D(O->GetNumberField(TEXT("x")),O->GetNumberField(TEXT("y")))-Origin;
 R.PixelSize=FVector2D(O->GetNumberField(TEXT("width")),O->GetNumberField(TEXT("height")));return R;
}
}
UOpusWorldScreen* UOpusWorldUI::Create(APlayerController* Player,const FString& Template)
{return CreateTemplate(Player,Template,TEXT("UICreatorWorld"),-1);}
UOpusWorldScreen* UOpusWorldUI::CreateTemplate(APlayerController* Player,const FString& Template,const FString& Directory,int32 Layer,bool Viewport)
{
 if(!Player)return nullptr;const double LoadStart=FPlatformTime::Seconds();
 auto& Contract=Contracts.FindOrAdd(Directory);if(!Contract){FString S;const FString RuntimePath=FPaths::ProjectContentDir()/Directory/TEXT("runtime.json");const FString Path=FPaths::FileExists(RuntimePath)?RuntimePath:FPaths::ProjectContentDir()/Directory/TEXT("contract.json");FFileHelper::LoadFileToString(S,*Path);FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Contract);}
 if(!Contract)return nullptr;const bool Compact=Contract->HasField(TEXT("runtimeProjection"));const double ContractReady=FPlatformTime::Seconds();
 if(Compact){const TSharedPtr<FJsonObject>* Templates=nullptr;if(Contract->TryGetObjectField(TEXT("vectorTemplates"),Templates))OpusAuthored::RegisterVectorTemplates(Directory,*Templates);}
 USvgCanvasDocument* SavedAssets=nullptr;
 if(Directory==TEXT("UICreatorEvents")||Directory==TEXT("UICreatorLoot")||Directory==TEXT("UICreatorTalents")||Directory==TEXT("UICreatorEffects")||Directory==TEXT("UICreatorRecent")||Directory==TEXT("UICreatorMaps")||Directory==TEXT("UICreatorContext")||Directory==TEXT("UICreatorStudio")||Directory==TEXT("UICreatorHover")||Directory==TEXT("UICreatorHotbar"))
 {
  const FString AssetName=Compact?TEXT("DA_RuntimeCanvas"):TEXT("DA_CurrentCanvas");SavedAssets=LoadObject<USvgCanvasDocument>(nullptr,*(TEXT("/Game/")+Directory+TEXT("/")+AssetName+TEXT(".")+AssetName));
  if(!SavedAssets||SavedAssets->SourceHash!=Str(Contract,TEXT("sourceHash"))){UE_LOG(LogTemp,Error,TEXT("UICreatorLoot saved document is missing or stale"));return nullptr;}
  // Editor game sessions can asynchronously compile freshly imported PNGs.
  // Match the saved-WBP verifier: finish this small UI asset batch once, so a
  // valid texture UObject cannot produce a text-only panel on its first use.
  static TSet<FString> Prepared;
  if(!Compact&&!Prepared.Contains(SavedAssets->SourceHash))
  {
   TArray<UTexture*> Batch;for(const auto& Pair:SavedAssets->Rasters)if(Pair.Value.Texture)Batch.AddUnique(Pair.Value.Texture);
#if WITH_EDITOR
   FTextureCompilingManager::Get().FinishCompilation(Batch);
#endif
   Prepared.Add(SavedAssets->SourceHash);
  }
 }
 const double IdentityReady=FPlatformTime::Seconds();
 const auto& Nodes=Contract->GetArrayField(TEXT("nodes"));const auto& States=Contract->GetObjectField(TEXT("states"))->GetObjectField(TEXT("canvas"))->GetArrayField(TEXT("nodes"));
 TMap<FString,TSharedPtr<FJsonObject>> Layout;for(const auto& V:States)Layout.Add(Str(V->AsObject(),TEXT("id")),V->AsObject());
 TMap<FString,TSharedPtr<FJsonObject>> Authoring;const TArray<TSharedPtr<FJsonValue>>* Authored=nullptr;if(Contract->TryGetArrayField(TEXT("authoringNodes"),Authored))for(const auto& V:*Authored)Authoring.Add(Str(V->AsObject(),TEXT("id")),V->AsObject());
 if(!Layout.Contains(Template))return nullptr;auto RootRect=Layout[Template];if(RootRect->GetNumberField(TEXT("width"))<=0&&Authoring.Contains(Template))RootRect=Authoring[Template];const FVector2D Origin(RootRect->GetNumberField(TEXT("x")),RootRect->GetNumberField(TEXT("y")));
 auto* S=CreateWidget<UOpusWorldScreen>(Player);S->WidgetTree=NewObject<UWidgetTree>(S,TEXT("WidgetTree"));S->SetIsFocusable(false);
 S->ImportedAssets=SavedAssets;
 S->TemplateSize=FVector2D(RootRect->GetNumberField(TEXT("width")),RootRect->GetNumberField(TEXT("height")));S->SourceDocument=NewObject<USvgCanvasDocument>(S);S->SourceDocument->DesignSize=S->TemplateSize;
 S->SourceDocument->SourceHash=Str(Contract,TEXT("sourceHash"));
 if(SavedAssets)S->SourceDocument->Rasters=SavedAssets->Rasters;

 TMap<FString,USvgCanvasNode*> Created;TSet<UTexture*> UsedTextures;
 for(const auto& V:Nodes)
 {
  const auto P=V->AsObject();const FString Id=Str(P,TEXT("id")),Parent=Str(P,TEXT("parentId"));if(Id!=Template&&!Created.Contains(Parent))continue;if(!Layout.Contains(Id))continue;
  const auto L=(Layout[Id]->GetNumberField(TEXT("width"))<=0&&Authoring.Contains(Id))?Authoring[Id]:Layout[Id];auto* N=NewObject<UOpusActionNode>(S->WidgetTree);N->Screen=S;N->SourceId=Id;N->SourceType=Str(P,TEXT("type"));N->SourceName=Str(P,TEXT("name"));N->SourcePropertiesJson=Json(P);N->Binding=Str(P,TEXT("binding"));N->SourceText=Str(P,TEXT("text"));
  const TSharedPtr<FJsonObject>* A=nullptr;if(P->TryGetObjectField(TEXT("action"),A))N->ActionJson=Json(*A);
  N->DesignRect=FVector4(L->GetNumberField(TEXT("x"))-Origin.X,L->GetNumberField(TEXT("y"))-Origin.Y,L->GetNumberField(TEXT("width")),L->GetNumberField(TEXT("height")));N->DesignScale=1;
  // Template assembly must preserve the same saved motion references as the WBP importer.
  const TSharedPtr<FJsonObject>* Motion=nullptr;
  if(SavedAssets&&P->TryGetObjectField(TEXT("motion"),Motion)&&Str(*Motion,TEXT("preset"))!=TEXT("none"))
  {
   const FString MotionId=Str(*Motion,TEXT("assetId"));
   if(!MotionId.IsEmpty())
   {
    const FString Name=TEXT("MA_")+FMD5::HashAnsiString(*MotionId).Left(24);
    N->MotionAsset=LoadObject<USvgCanvasMotionAsset>(nullptr,*(TEXT("/Game/")+Directory+TEXT("/Motions/")+Name+TEXT(".")+Name));
    if(!N->MotionAsset||N->MotionAsset->SourceAssetId!=MotionId){UE_LOG(LogTemp,Error,TEXT("UICreator template %s has a missing saved motion: %s"),*Template,*MotionId);return nullptr;}
    (*Motion)->TryGetBoolField(TEXT("enabled"),N->MotionEnabled);
    (*Motion)->TryGetBoolField(TEXT("dismissOnHover"),N->MotionDismissOnHover);
    const FString Tint=Str(*Motion,TEXT("tint"));if(!Tint.IsEmpty())N->MotionTint=FLinearColor::FromSRGBColor(FColor::FromHex(Tint));
   }
  }
  N->AuthoredLayout=N->AuthoredAppearance=N->AuthoredVisibility=true;N->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  if(Id==Template)S->WidgetTree->RootWidget=N;else {auto* Slot=Created[Parent]->AddChildToCanvas(N);Slot->SetPosition(FVector2D(N->DesignRect.X-Created[Parent]->DesignRect.X,N->DesignRect.Y-Created[Parent]->DesignRect.Y));Slot->SetSize(FVector2D(N->DesignRect.Z,N->DesignRect.W));Slot->SetZOrder(L->GetIntegerField(TEXT("paintOrder")));}
  FString RasterKey=Str(L,TEXT("raster"));if(RasterKey.IsEmpty()&&Authoring.Contains(Id))RasterKey=Str(Authoring[Id],TEXT("raster"));
  const TSharedPtr<FJsonObject>* RuntimeSource=nullptr;P->TryGetObjectField(TEXT("source"),RuntimeSource);bool VectorOnly=false;FString RasterFrom;if(Compact&&RuntimeSource){(*RuntimeSource)->TryGetBoolField(TEXT("runtimeVector"),VectorOnly);(*RuntimeSource)->TryGetStringField(TEXT("runtimeRasterFrom"),RasterFrom);}
  if(!VectorOnly){
   if(!RasterFrom.IsEmpty()&&Authoring.Contains(RasterFrom)){const auto Ref=Authoring[RasterFrom];N->Normal=Raster(Contract,Directory,Str(Ref,TEXT("raster")),Origin);const FVector2D RefAt(Ref->GetNumberField(TEXT("x"))-Origin.X,Ref->GetNumberField(TEXT("y"))-Origin.Y),At(N->DesignRect.X,N->DesignRect.Y),Scale(N->DesignRect.Z/FMath::Max(1.,Ref->GetNumberField(TEXT("width"))),N->DesignRect.W/FMath::Max(1.,Ref->GetNumberField(TEXT("height"))));N->Normal.PixelOrigin=At+(N->Normal.PixelOrigin-RefAt)*Scale;N->Normal.PixelSize*=Scale;}
   else N->Normal=Raster(Contract,Directory,RasterKey,Origin);
  }
  const TSharedPtr<FJsonObject>* Brushes=nullptr;
  if(!VectorOnly&&RasterFrom.IsEmpty()&&L->TryGetObjectField(TEXT("brushRasters"),Brushes)){const FString NormalKey=Str(*Brushes,TEXT("normal"));if(!NormalKey.IsEmpty())N->Normal=Raster(Contract,Directory,NormalKey,Origin);N->Hovered=Raster(Contract,Directory,Str(*Brushes,TEXT("hovered")),Origin);N->Pressed=Raster(Contract,Directory,Str(*Brushes,TEXT("pressed")),Origin);N->Disabled=Raster(Contract,Directory,Str(*Brushes,TEXT("disabled")),Origin);N->ApplyRaster(N->Normal);}
  N->Surface=NewObject<USvgCanvasImage>(N);N->AddChildToCanvas(N->Surface);N->ApplyRaster(N->Normal);
  if(VectorOnly)N->Surface->SetVisibility(ESlateVisibility::Collapsed);
  for(const auto* R:{&N->Normal,&N->Hovered,&N->Pressed,&N->Disabled})if(R->Texture)UsedTextures.Add(R->Texture.Get());
  if(Compact&&Id!=Template){const FString Local=Str(L,TEXT("localVisibility"));if(Local==TEXT("hidden"))N->SetVisibility(ESlateVisibility::Hidden);else if(Local==TEXT("collapsed"))N->SetVisibility(ESlateVisibility::Collapsed);}

  if(N->SourceType==TEXT("Button"))
  {
   S->Interactive=true;N->Input=NewObject<UButton>(N);auto Style=N->Input->GetStyle();Style.Normal.DrawAs=Style.Hovered.DrawAs=Style.Pressed.DrawAs=Style.Disabled.DrawAs=ESlateBrushDrawType::NoDrawType;N->Input->SetStyle(Style);
   auto* Slot=N->AddChildToCanvas(N->Input);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));Slot->SetZOrder(50);N->BindInput();
   N->HoverOwner=N;N->HoverEvents=UOpusUI::HoverEvents(S);
   N->Input->OnHovered.AddDynamic(N,&UOpusActionNode::EnterItem);N->Input->OnUnhovered.AddDynamic(N,&UOpusActionNode::LeaveItem);
   N->Input->OnHovered.AddDynamic(N,&UOpusActionNode::FitSurface);N->Input->OnUnhovered.AddDynamic(N,&UOpusActionNode::FitSurface);
   N->Input->OnPressed.AddDynamic(N,&UOpusActionNode::FitSurface);N->Input->OnReleased.AddDynamic(N,&UOpusActionNode::FitSurface);
  }
  const TSharedPtr<FJsonObject>* Style=nullptr;bool Clip=false;if(P->TryGetObjectField(TEXT("style"),Style))(*Style)->TryGetBoolField(TEXT("clip"),Clip);if(Clip)N->SetClipping(EWidgetClipping::ClipToBoundsAlways);
  N->RememberImportedDefaults();Created.Add(Id,N);
 }
 const double NodesReady=FPlatformTime::Seconds();
#if WITH_EDITOR
 if(Compact&&!UsedTextures.IsEmpty())FTextureCompilingManager::Get().FinishCompilation(UsedTextures.Array());
#endif
 const double TexturesReady=FPlatformTime::Seconds();
 S->SetVisibility(S->Interactive?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::HitTestInvisible);if(Viewport)S->AddToViewport(Layer<0?(S->Interactive?30:10):Layer);S->SetDesiredSizeInViewport(S->TemplateSize);if(!Viewport)S->TakeWidget();Widgets.RemoveAll([](const auto& W){return !W.IsValid();});if(Viewport)Widgets.Add(S);if(Compact)UE_LOG(LogTemp,Display,TEXT("UI_LOAD directory=%s template=%s compact=1 nodes=%d textures=%d json_ms=%.2f identity_ms=%.2f widgets_ms=%.2f texture_wait_ms=%.2f viewport_ms=%.2f total_ms=%.2f"),*Directory,*Template,Created.Num(),UsedTextures.Num(),(ContractReady-LoadStart)*1000,(IdentityReady-ContractReady)*1000,(NodesReady-IdentityReady)*1000,(TexturesReady-NodesReady)*1000,(FPlatformTime::Seconds()-TexturesReady)*1000,(FPlatformTime::Seconds()-LoadStart)*1000);return S;
}
void UOpusActionNode::FitSurface()
{
 if(FlexibleInset<=0||!Surface)return;auto Brush=Surface->GetBrush();Brush.DrawAs=ESlateBrushDrawType::Box;
 Brush.Margin=FMargin(FlexibleInset/FMath::Max(1.f,float(Brush.ImageSize.X)),FlexibleInset/FMath::Max(1.f,float(Brush.ImageSize.Y)));
 Surface->SetBrush(Brush);auto* Canvas=Cast<UCanvasPanelSlot>(Surface->Slot);Canvas->SetAnchors(FAnchors(0,0,1,1));Canvas->SetOffsets(FMargin(0));
}
void UOpusWorldUI::SetHover(UOpusWorldScreen* S,const FString& B,const FString& Id)
{
 if(!S)return;auto& Index=HoverBindings.FindOrAdd(S);
 if(Index.IsEmpty())S->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<UOpusActionNode>(W);N&&!N->Binding.IsEmpty())Index.FindOrAdd(N->Binding).Add(N);});
 if(auto* Matches=Index.Find(B))for(const auto& N:*Matches)if(N.IsValid())N->HoverId=Id;
}
bool UOpusWorldUI::VisualReady(UOpusWorldScreen* S)
{
 if(!S||!S->IsVisible())return false;bool Ready=true;
 S->WidgetTree->ForEachWidget([&](UWidget* W){auto* N=Cast<USvgCanvasNode>(W);if(!N||!N->Normal.Texture||!N->Surface||!N->Surface->IsVisible())return;
  auto* Texture=Cast<UTexture2D>(N->Surface->GetBrush().GetResourceObject());const auto Size=N->Surface->GetCachedGeometry().GetLocalSize();
  if(!Texture||!Texture->GetResource()||Size.X<=0||Size.Y<=0)Ready=false;
 });return Ready;
}
void UOpusWorldUI::CopyTextColor(UOpusWorldScreen* Target,const FString& B,UOpusWorldScreen* Source,const FString& SourceBinding)
{
 if(!Target||!Source)return;FString Color;
 Source->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<USvgCanvasNode>(W);N&&N->Binding==SourceBinding){TSharedPtr<FJsonObject> P;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(N->SourcePropertiesJson),P);if(P)P->GetObjectField(TEXT("style"))->TryGetStringField(TEXT("color"),Color);}});
 if(Color.IsEmpty())return;Target->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<USvgCanvasNode>(W);N&&N->Binding==B)for(auto* C:N->GetAllChildren())if(auto* Box=Cast<UScaleBox>(C))if(auto* Text=Cast<UTextBlock>(Box->GetChildAt(0)))Text->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor::FromHex(Color))));});
}
void UOpusWorldUI::Emphasize(UOpusWorldScreen* S,const FString& B,bool Enabled)
{
 if(!S)return;S->WidgetTree->ForEachWidget([&](UWidget* W){auto* N=Cast<UOpusActionNode>(W);if(!N||N->Binding!=B)return;
 if(!S->BaseRasters.Contains(N->SourceId))S->BaseRasters.Add(N->SourceId,N->Normal);
 N->Normal=Enabled&&N->Hovered.Texture?N->Hovered:S->BaseRasters[N->SourceId];N->ApplyRaster(N->Input&&N->Input->IsHovered()?N->Hovered:N->Normal);N->FitSurface();});
}
void UOpusWorldUI::CopyBrush(UOpusWorldScreen* Target,const FString& Binding,UOpusWorldScreen* Source,const FString& SourceId)
{
 auto* From=Source?Source->FindSourceNode(SourceId):nullptr;if(!Target||!From)return;
 Target->WidgetTree->ForEachWidget([&](UWidget* W){auto* N=Cast<USvgCanvasNode>(W);if(!N||N->Binding!=Binding)return;
 const auto Offset=N->Normal.PixelOrigin-From->Normal.PixelOrigin;
 auto Copy=[&](FSvgCanvasRaster R){R.PixelOrigin+=Offset;return R;};
 N->Normal=Copy(From->Normal);N->Hovered=Copy(From->Hovered);N->Pressed=Copy(From->Pressed);N->Disabled=Copy(From->Disabled);
 N->ApplyRaster(N->Input&&N->Input->IsHovered()?N->Hovered:N->Normal);
 });
}
void UOpusWorldUI::MountRows(UOpusWorldScreen* S,const FString& B,const TArray<UOpusWorldScreen*>& Rows)
{
 if(!S)return;S->WidgetTree->ForEachWidget([&](UWidget* W){auto* N=Cast<USvgCanvasNode>(W);if(!N||N->Binding!=B)return;
 // A mounted row replaces authored HTML example rows. Keep their templates
 // in the document, but do not paint them beneath live transparent rows.
 TSharedPtr<FJsonObject> FlowProperties;const TSharedPtr<FJsonObject>* Html=nullptr;
 if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(N->SourcePropertiesJson),FlowProperties)&&FlowProperties->TryGetObjectField(TEXT("source"),Html)&&(*Html)->HasField(TEXT("htmlId")))
  for(auto* C:N->GetAllChildren())if(auto* Example=Cast<USvgCanvasNode>(C)){Example->AuthoredVisibility=true;Example->SetVisibility(ESlateVisibility::Collapsed);}
 UScrollBox* Scroll=nullptr;for(auto* C:N->GetAllChildren())if(C->GetFName()==TEXT("TemplateScroll"))Scroll=Cast<UScrollBox>(C);
 if(!Scroll)
 {
  Scroll=NewObject<UScrollBox>(N,TEXT("TemplateScroll"));Scroll->SetScrollbarThickness(FVector2D(5,5));Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
  // Match UICreator SvgRenderer's generic ScrollBox chrome (not a game skin).
  auto Bar=Scroll->GetWidgetBarStyle();FSlateRoundedBoxBrush Track(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("1b2733"))),3.f);FSlateRoundedBoxBrush Thumb(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("526679"))),3.f);Thumb.ImageSize=FVector2D(5,24);
  Bar.VerticalBackgroundImage=Bar.VerticalTopSlotImage=Bar.VerticalBottomSlotImage=Track;Bar.NormalThumbImage=Bar.HoveredThumbImage=Bar.DraggedThumbImage=Thumb;Scroll->SetWidgetBarStyle(Bar);
  auto Style=Scroll->GetWidgetStyle();Style.TopShadowBrush.DrawAs=Style.BottomShadowBrush.DrawAs=Style.LeftShadowBrush.DrawAs=Style.RightShadowBrush.DrawAs=ESlateBrushDrawType::NoDrawType;Scroll->SetWidgetStyle(Style);
  auto* Slot=N->AddChildToCanvas(Scroll);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));Slot->SetZOrder(80);
 }
 Scroll->ClearChildren();for(auto* Row:Rows)if(Row){auto* Box=NewObject<USizeBox>(Scroll);Box->SetWidthOverride(Row->TemplateSize.X);Box->SetHeightOverride(Row->TemplateSize.Y);Box->AddChild(Row);auto* Slot=Cast<UScrollBoxSlot>(Scroll->AddChild(Box));double Gap=4;TSharedPtr<FJsonObject> Props;const TSharedPtr<FJsonObject>* Style=nullptr;if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(N->SourcePropertiesJson),Props)&&Props->TryGetObjectField(TEXT("style"),Style))(*Style)->TryGetNumberField(TEXT("rowGap"),Gap);Slot->SetPadding(FMargin(0,0,0,Gap));}
 });
}
void UOpusWorldUI::FitLabel(UOpusWorldScreen* S,const FString& B,const FString& Text,float Padding)
{
 if(!S)return;UOpusUI::SetText(S,B,Text);S->ForceLayoutPrepass();double Width=0;USvgCanvasNode* Name=nullptr;
 S->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<USvgCanvasNode>(W);N&&N->Binding==B){Name=N;for(auto* C:N->GetAllChildren())if(auto* Fit=Cast<UScaleBox>(C))if(auto* T=Cast<UTextBlock>(Fit->GetChildAt(0)))Width=FMath::Max(Width,double(T->GetDesiredSize().X));}});
 Width=FMath::Clamp(FMath::CeilToDouble(Width+Padding),60.,260.);S->TemplateSize.X=Width;S->SetDesiredSizeInViewport(S->TemplateSize);
 S->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<USvgCanvasNode>(W)){if(N==Name)N->DesignRect.Z=Width-Padding;else if(N->DesignRect.Z==260)N->DesignRect.Z=Width;else return;if(auto* Slot=Cast<UCanvasPanelSlot>(N->Slot))Slot->SetSize(FVector2D(N->DesignRect.Z,N->DesignRect.W));if(auto* Action=Cast<UOpusActionNode>(N);Action&&Action->Input){Action->FlexibleInset=2;Action->FitSurface();}}});
}
void UOpusWorldUI::Fill(UOpusWorldScreen* S,const FString& Binding,float Ratio)
{
 if(!S)return;Ratio=FMath::IsFinite(Ratio)?FMath::Clamp(Ratio,0.f,1.f):0;S->FillValues.Add(Binding,Ratio);
 S->WidgetTree->ForEachWidget([&](UWidget* W){auto* N=Cast<USvgCanvasNode>(W);if(!N||N->Binding!=Binding)return;
  UCanvasPanel* Crop=nullptr;for(auto* C:N->GetAllChildren())if(C->GetFName()==TEXT("WorldFillClip"))Crop=Cast<UCanvasPanel>(C);
  if(!Crop){Crop=NewObject<UCanvasPanel>(N,TEXT("WorldFillClip"));Crop->SetClipping(EWidgetClipping::ClipToBoundsAlways);Crop->SetVisibility(ESlateVisibility::HitTestInvisible);N->AddChildToCanvas(Crop)->SetPosition(FVector2D::ZeroVector);
   auto* Image=NewObject<USvgCanvasImage>(Crop);Image->SetBrush(N->Surface->GetBrush());Image->SetVisibility(ESlateVisibility::HitTestInvisible);auto* Slot=Crop->AddChildToCanvas(Image);Slot->SetSize(FVector2D(N->DesignRect.Z,N->DesignRect.W));N->Surface->SetVisibility(ESlateVisibility::Collapsed);}
  Cast<UCanvasPanelSlot>(Crop->Slot)->SetSize(FVector2D(N->DesignRect.Z*Ratio,N->DesignRect.W));
 });
}
bool UOpusWorldUI::Place(UOpusWorldScreen* S,APlayerController* P,FVector Location,FVector2D Offset)
{
 if(!S||!P)return false;FVector2D At;const FVector2D View=UWidgetLayoutLibrary::GetViewportSize(P)/UWidgetLayoutLibrary::GetViewportScale(P);
 const bool Visible=UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(P,Location,At,true)&&At.X>=0&&At.Y>=0&&At.X<=View.X&&At.Y<=View.Y;
 S->SetVisibility(Visible?(S->Interactive?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::HitTestInvisible):ESlateVisibility::Collapsed);
 if(Visible)S->SetPositionInViewport(At-FVector2D(S->TemplateSize.X*.5,S->TemplateSize.Y)+Offset,false);return Visible;
}
bool UOpusWorldUI::PointerBlocked(APlayerController* P)
{
 for(const auto& S:Widgets)if(S.IsValid()&&S->GetOwningPlayer()==P&&S->Interactive&&S->IsVisible()&&S->IsHovered())return true;return false;
}
void UOpusWorldUI::PlaceLabels(APlayerController* P,const TArray<UOpusWorldScreen*>& Screens,const TArray<FVector>& Locations)
{
 if(!P)return;const FVector2D View=UWidgetLayoutLibrary::GetViewportSize(P)/UWidgetLayoutLibrary::GetViewportScale(P);TArray<FBox2D> Placed;
 for(int I=0;I<Screens.Num();++I)
 {
  auto* S=Screens[I];if(!S)continue;FVector2D At;S->SetVisibility(ESlateVisibility::Collapsed);
  if(!Locations.IsValidIndex(I)||I>=96||!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(P,Locations[I],At,true)||At.X<0||At.Y<0||At.X>View.X||At.Y>View.Y)continue;
  const FVector2D Size=S->TemplateSize;const FVector2D Anchor(FMath::Clamp(At.X-Size.X*.5,4.,FMath::Max(4.,View.X-Size.X-4)),FMath::Clamp(At.Y,4.,FMath::Max(4.,View.Y-Size.Y-4)));
  for(int N=0;N<100;++N)
  {
   const int Row=N%20,Column=N/20;const FVector2D Offset(FMath::CeilToDouble(Column/2.)*(Column%2?-1:1)*(Size.X+4),FMath::CeilToDouble(Row/2.)*(Row%2?-1:1)*(Size.Y+3));const FVector2D Pos=Anchor+Offset;
   if(Pos.X<4||Pos.Y<4||Pos.X+Size.X>View.X-4||Pos.Y+Size.Y>View.Y-4)continue;const FBox2D Rect(Pos,Pos+Size);
   bool Overlap=false;for(const auto& Other:Placed)if(Rect.ExpandBy(3).Intersect(Other)){Overlap=true;break;}if(Overlap)continue;
   S->SetPositionInViewport(Pos,false);S->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Placed.Add(Rect);break;
  }
 }
}
FVector UOpusWorldUI::HeadAnchor(USkeletalMeshComponent* M)
{
 if(!M)return FVector::ZeroVector;const FBoxSphereBounds B=M->CalcBounds(M->GetComponentTransform());return FVector(B.Origin.X,B.Origin.Y,B.Origin.Z+B.BoxExtent.Z+20);
}
double UOpusWorldUI::PresentationSeed(const FString& Key)
{uint32 H=2166136261u;for(TCHAR C:Key){H^=uint32(C);H*=16777619u;}return double(H)/4294967296.;}
