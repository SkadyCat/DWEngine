#include "OpusUI.h"
#include "OpusAssets.h"
#include "OpusSvgBinding.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "Components/EditableTextBox.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
namespace
{
struct FNodeIndex { TArray<TWeakObjectPtr<USvgCanvasNode>> All;TMap<FString,TArray<TWeakObjectPtr<USvgCanvasNode>>> Bindings;TMap<FString,FString> RowHashes;TArray<TWeakObjectPtr<UEditableTextBox>> Inputs;TMap<FString,TArray<TWeakObjectPtr<USvgCanvasNode>>> Actions;FString VisibleState;TSet<FString> VisibleSources; };
TMap<TWeakObjectPtr<USvgCanvasScreen>,FNodeIndex> Indexes;
FNodeIndex& Index(USvgCanvasScreen* S){auto& I=Indexes.FindOrAdd(S);if(I.All.IsEmpty()&&S&&S->WidgetTree)S->WidgetTree->ForEachWidget([&](UWidget* W){if(auto* N=Cast<USvgCanvasNode>(W)){I.All.Add(N);if(!N->Binding.IsEmpty())I.Bindings.FindOrAdd(N->Binding).Add(N);}});return I;}
TArray<USvgCanvasNode*> Nodes(USvgCanvasScreen* S,const FString& B){TArray<USvgCanvasNode*> Out;if(S)if(auto* Rows=Index(S).Bindings.Find(B))for(const auto& R:*Rows)if(R.IsValid())Out.Add(R.Get());return Out;}
TSharedPtr<FJsonObject> Props(USvgCanvasNode* N){TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(N->SourcePropertiesJson),O);return O?O:MakeShared<FJsonObject>();}
TSharedPtr<FJsonObject> Style(USvgCanvasNode* N){const TSharedPtr<FJsonObject>* O=nullptr;auto P=Props(N);return P->TryGetObjectField(TEXT("style"),O)?*O:MakeShared<FJsonObject>();}
FLinearColor Color(TSharedPtr<FJsonObject> S,const TCHAR* K,const TCHAR* Fallback){FString V=Fallback;S->TryGetStringField(K,V);return FLinearColor::FromSRGBColor(FColor::FromHex(V));}
void Fill(UCanvasPanelSlot* Slot){Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));Slot->SetZOrder(30);}
}
USvgCanvasScreen* UOpusUI::CreateHUD(APlayerController* P)
{
 auto* C=UOpusAssets::HUDClass(P);if(!P||!C)return nullptr;
 auto* S=CreateWidget<USvgCanvasScreen>(P,C);if(!S)return nullptr;S->InitialState=TEXT("selected-1");S->AddToViewport(50);
 P->bShowMouseCursor=true;FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);P->SetInputMode(Input);return S;
}
UOpusHoverEvents* UOpusUI::HoverEvents(USvgCanvasScreen* S)
{
 static TMap<TWeakObjectPtr<USvgCanvasScreen>,TWeakObjectPtr<UOpusHoverEvents>> Events;
 if(!S)return nullptr;auto& E=Events.FindOrAdd(S);if(!E.IsValid())E=NewObject<UOpusHoverEvents>(S);return E.Get();
}
FOpusUIAction UOpusUI::Action(USvgCanvasNode* N)
{
 FOpusUIAction A;if(!N)return A;TSharedPtr<FJsonObject> O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(N->ActionJson),O)||!O)return A;
 O->TryGetStringField(TEXT("type"),A.Type);O->TryGetStringField(TEXT("op"),A.Op);double Number=0;if(O->TryGetNumberField(TEXT("value"),Number)){A.Index=int32(Number);A.Value=FString::FromInt(A.Index);}else O->TryGetStringField(TEXT("value"),A.Value);return A;
}
void UOpusUI::SetText(USvgCanvasScreen* S,const FString& Binding,const FString& V)
{for(auto* N:Nodes(S,Binding))OpusSvg::Text(N,V);}
void UOpusUI::HidePrefix(USvgCanvasScreen* S,const FString& Prefix){if(S)for(const auto& W:Index(S).All)if(W.IsValid()&&W->SourceId.Contains(Prefix)){W->AuthoredVisibility=true;W->SetVisibility(ESlateVisibility::Collapsed);}}
void UOpusUI::HideSurface(USvgCanvasScreen* S,const FString& Id){if(S)if(auto* N=S->FindSourceNode(Id)){N->AuthoredAppearance=true;N->Normal.Texture=N->Hovered.Texture=N->Pressed.Texture=N->Disabled.Texture=nullptr;if(N->Surface)N->Surface->SetVisibility(ESlateVisibility::Collapsed);}}
void UOpusUI::SetImage(USvgCanvasScreen* S,const FString& B,const FString& Path){for(auto* N:Nodes(S,B))OpusSvg::Image(N,Path);}
void UOpusUI::SetProgress(USvgCanvasScreen* S,const FString& Binding,float V)
{for(auto* N:Nodes(S,Binding))OpusSvg::Progress(N,V);}
void UOpusUI::SetShown(USvgCanvasScreen* S,const FString& B,bool Shown){for(auto* N:Nodes(S,B)){N->AuthoredVisibility=true;N->SetVisibility(Shown?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);}}
bool UOpusUI::BlocksCombat(USvgCanvasScreen* S){return S&&!S->CurrentState.StartsWith(TEXT("selected-"))&&S->CurrentState!=TEXT("canvas");}
FString UOpusUI::SourceHash(USvgCanvasScreen* S){return S&&S->SourceDocument?S->SourceDocument->SourceHash:FString();}
int32 UOpusUI::StateCount(USvgCanvasScreen* S){return S&&S->SourceDocument?S->SourceDocument->States.Num():0;}
bool UOpusUI::HasVisibleBinding(USvgCanvasScreen* S,const FString& B)
{
 if(!S)return false;const auto* State=S->SourceDocument?S->SourceDocument->States.Find(S->CurrentState):nullptr;if(!State)return true;
 auto& I=Index(S);if(I.VisibleState!=S->CurrentState){I.VisibleState=S->CurrentState;I.VisibleSources.Empty();for(const auto& N:State->Nodes)if(N.Visibility==TEXT("visible")&&N.Rect.Z>0&&N.Rect.W>0)I.VisibleSources.Add(N.Id);}
 for(auto* N:Nodes(S,B))if(I.VisibleSources.Contains(N->SourceId))return true;return false;
}

bool UOpusUI::BlocksPointer(USvgCanvasScreen* S){if(!S)return false;for(const auto& W:Index(S).All)if(W.IsValid()&&W->Input&&W->Input->IsHovered())return true;return false;}
void UOpusUI::SetActionText(USvgCanvasScreen* S,const FString& Type,int32 At,const FString& Text){if(!S)return;FString Binding=TEXT("opus-action-")+Type+FString::FromInt(At);auto& I=Index(S);if(!I.Actions.Contains(Binding)){auto& Found=I.Actions.Add(Binding);for(const auto& W:I.All)if(W.IsValid()){auto A=Action(W.Get());if(A.Type==Type&&A.Index==At)Found.Add(W);}I.Bindings.FindOrAdd(Binding).Append(Found);}SetText(S,Binding,Text);}
void UOpusUI::SetRows(USvgCanvasScreen* S,const FString& Binding,const TArray<FOpusUIRow>& Rows,int32 Columns)
{if(!S||!HasVisibleBinding(S,Binding))return;FString Hash=S->CurrentState;for(const auto& R:Rows)Hash+=R.Label+R.Detail+R.Op+R.Value+R.Icon+R.Kind+R.Rarity+R.Meta+R.Slot+R.Badge+R.PlacementKey+FString::FromInt(R.BagCell)+(R.Selected?TEXT("s"):TEXT("n"))+FString::FromInt(R.Count)+FString::FromInt(R.Tier)+(R.Equipped?TEXT("1"):TEXT("0"))+(R.Enabled?TEXT("1"):TEXT("0"))+(R.NewItem?TEXT("1"):TEXT("0"));auto& Cache=Index(S).RowHashes;if(Cache.Contains(Binding)&&Cache[Binding]==Hash)return;Cache.Add(Binding,Hash);for(auto* N:Nodes(S,Binding))OpusSvg::Rows(S,N,Binding,Rows);}

void UOpusUI::BindNode(USvgCanvasScreen* S,const FString& Id,const FString& B){if(S)if(auto* N=S->FindSourceNode(Id)){Index(S).Bindings.FindOrAdd(B).AddUnique(N);}}
void UOpusUI::Region(USvgCanvasScreen* S,const FString& ParentId,const FString& B,FVector4 Rect,const FString& Prefix)
{
 if(!S)return;auto* Parent=S->FindSourceNode(ParentId);if(!Parent)return;
 for(auto* W:Parent->GetAllChildren())if(auto* N=Cast<USvgCanvasNode>(W);N&&!Prefix.IsEmpty()&&N->SourceId.Contains(Prefix)){N->AuthoredVisibility=true;N->SetVisibility(ESlateVisibility::Collapsed);}
 auto* N=NewObject<USvgCanvasNode>(Parent);N->Binding=B;N->DesignScale=Parent->DesignScale;N->DesignRect=FVector4(0,0,Rect.Z*Parent->DesignScale,Rect.W*Parent->DesignScale);
 auto* Slot=Parent->AddChildToCanvas(N);Slot->SetPosition(FVector2D(Rect.X,Rect.Y)*Parent->DesignScale);Slot->SetSize(FVector2D(Rect.Z,Rect.W)*Parent->DesignScale);Slot->SetZOrder(80);Index(S).Bindings.FindOrAdd(B).Add(N);Index(S).All.Add(N);
}
void UOpusUI::SetInput(USvgCanvasScreen* S,const FString& B,const FString& Initial)
{
 for(auto* N:Nodes(S,B)){UEditableTextBox* E=nullptr;for(auto* W:N->GetAllChildren())if(W->GetFName()==TEXT("OpusInput"))E=Cast<UEditableTextBox>(W);if(E)continue;
 E=NewObject<UEditableTextBox>(N,TEXT("OpusInput"));E->SetText(FText::FromString(Initial));Index(S).Inputs.Add(E);auto St=E->GetWidgetStyle();OpusSvg::Input(N,E);auto* Slot=N->AddChildToCanvas(E);Fill(Slot);Slot->SetZOrder(60);}
}
FString UOpusUI::Input(USvgCanvasScreen* S,const FString& B){for(auto* N:Nodes(S,B))for(auto* W:N->GetAllChildren())if(auto* E=Cast<UEditableTextBox>(W))return E->GetText().ToString();return FString();}
int32 UOpusUI::IntInput(USvgCanvasScreen* S,const FString& B,int32 F){const FString V=Input(S,B);return V.IsNumeric()?FCString::Atoi(*V):F;}
bool UOpusUI::TextFocused(USvgCanvasScreen* S){if(!S)return false;for(const auto& E:Index(S).Inputs)if(E.IsValid()&&E->HasKeyboardFocus())return true;return false;}

void UOpusUI::SetAction(USvgCanvasScreen* S,const FString& B,const FString& Op,const FString& Value){for(auto* N:Nodes(S,B))OpusSvg::Action(N,Op,Value);}
void UOpusUI::SetSelection(USvgCanvasScreen* S,const FString& Prefix,int32 Selected,int32 Count)
{
 struct FSkins{FSvgCanvasRaster On,Off;FVector2D OnOrigin,OffOrigin;};static TMap<TWeakObjectPtr<USvgCanvasScreen>,TMap<FString,FSkins>> Skins;
 if(!S)return;auto& ScreenSkins=Skins.FindOrAdd(S);if(!ScreenSkins.Contains(Prefix)){auto First=Nodes(S,Prefix+TEXT("0")),Second=Nodes(S,Prefix+TEXT("1"));if(First.IsEmpty()||Second.IsEmpty())return;ScreenSkins.Add(Prefix,{First[0]->Normal,Second[0]->Normal,FVector2D(First[0]->DesignRect.X,First[0]->DesignRect.Y),FVector2D(Second[0]->DesignRect.X,Second[0]->DesignRect.Y)});}
 const auto& Skin=ScreenSkins[Prefix];for(int I=0;I<Count;I++)for(auto* N:Nodes(S,Prefix+FString::FromInt(I))){N->AuthoredAppearance=true;N->Normal=I==Selected?Skin.On:Skin.Off;N->Normal.PixelOrigin+=FVector2D(N->DesignRect.X,N->DesignRect.Y)-(I==Selected?Skin.OnOrigin:Skin.OffOrigin);N->ApplyRaster(N->Normal);}
}

void UOpusUI::SetRect(USvgCanvasScreen* S,const FString& B,FVector4 R){for(auto* N:Nodes(S,B))OpusSvg::PlaceNode(N,R.X*N->DesignScale,R.Y*N->DesignScale,R.Z*N->DesignScale,R.W*N->DesignScale);}
void UOpusUI::SetEnabled(USvgCanvasScreen* S,const FString& B,bool Enabled){for(auto* N:Nodes(S,B))if(N->Input)N->Input->SetIsEnabled(Enabled);}
