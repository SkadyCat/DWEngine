#include "OpusMapUI.h"
#include "OpusSvgBinding.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Components/Image.h"
#include "Serialization/JsonSerializer.h"

void UOpusMapUI::Fit(UOpusWorldScreen* S,APlayerController* P)
{
 if(!S||!P)return;const auto V=UWidgetLayoutLibrary::GetViewportSize(P)/FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(P));
 const float K=FMath::Min(1.,FMath::Min((V.X-32)/S->TemplateSize.X,(V.Y-40)/S->TemplateSize.Y));
 S->SetDesiredSizeInViewport(S->TemplateSize);S->SetRenderTransformPivot(FVector2D::ZeroVector);S->SetRenderScale(FVector2D(K,K));S->SetPositionInViewport((V-S->TemplateSize*K)*.5,false);
 // The backdrop is authored with the page, fitted to the viewport independently.
 auto* Root=Cast<USvgCanvasNode>(S->GetRootWidget());if(!Root)return;
 TSharedPtr<FJsonObject> Props;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Root->SourcePropertiesJson),Props);const TSharedPtr<FJsonObject>* Source=nullptr;FString Backdrop;
 if(Props&&Props->TryGetObjectField(TEXT("source"),Source))(*Source)->TryGetStringField(TEXT("backdrop"),Backdrop);
 if(Backdrop.IsEmpty())return;auto* Image=Cast<UImage>(S->WidgetTree->FindWidget(TEXT("AuthoredBackdrop")));if(!Image){Image=NewObject<UImage>(S->WidgetTree,TEXT("AuthoredBackdrop"));Image->SetColorAndOpacity(FLinearColor::FromSRGBColor(FColor::FromHex(Backdrop)));Image->SetVisibility(ESlateVisibility::HitTestInvisible);Root->AddChildToCanvas(Image);}
 if(auto* Slot=Cast<UCanvasPanelSlot>(Image->Slot)){Slot->SetPosition(-(V-S->TemplateSize*K)*.5/K);Slot->SetSize(V/K);Slot->SetZOrder(-100000);}
}
bool UOpusMapUI::MouseDown(){return FSlateApplication::IsInitialized()&&FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);}
USvgCanvasNode* UOpusMapUI::Hit(USvgCanvasScreen* S,APlayerController* P)
{
 if(!S||!P)return nullptr;const auto At=S->DispatchingPointer?UWidgetLayoutLibrary::GetViewportWidgetGeometry(P).LocalToAbsolute(S->PointerPosition):FSlateApplication::Get().GetCursorPos();USvgCanvasNode* Found=nullptr;
 S->WidgetTree->ForEachWidget([&](UWidget* W){auto* N=Cast<USvgCanvasNode>(W);if(!N||!N->Input||!N->Input->GetIsEnabled())return;
 for(UWidget* Parent=N;Parent;Parent=Parent->GetParent())if(!Parent->IsVisible())return;
 const auto& G=N->GetCachedGeometry();if(G.IsUnderLocation(At))Found=N;
 });return Found;
}
FVector4 UOpusMapUI::Bounds(USvgCanvasScreen* S,const FString& B)
{
 FVector4 R(0,0,0,0);if(S)S->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<USvgCanvasNode>(W);N&&N->Binding==B)R=N->DesignRect;});return R;
}
void UOpusMapUI::Arrange(USvgCanvasScreen* S,const FString& B,const TArray<FVector4>& R,const TArray<float>& A)
{if(S)S->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<USvgCanvasNode>(W);N&&N->Binding==B)OpusSvg::ArrangeRows(S,N,B,R,A);});}
