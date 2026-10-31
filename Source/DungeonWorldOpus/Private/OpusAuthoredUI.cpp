// Generic authored vector/UI adapters. Gameplay policy stays in AngelScript.
#include "OpusAuthoredUI.h"
#include "OpusTalentBridge.h"
#include "OpusSvgBinding.h"
#include "DWRuleWorldSubsystem.h"
#include "DWRuleCompiler.h"
#include "DWRuleProgramReader.h"
#include "OpusTraitText.h"
#include "SvgCanvasScreen.h"
#include "SvgCanvasNode.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/NativeWidgetHost.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#endif
namespace {
using Obj=TSharedPtr<FJsonObject>;
Obj Parse(const FString& Text){Obj O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O);return O;}
FString S(const Obj& O,const TCHAR* Key){FString V;if(O)O->TryGetStringField(Key,V);return V;}
Obj Props(USvgCanvasNode* N){return N?Parse(N->SourcePropertiesJson):nullptr;}
FLinearColor CSS(FString Text){if(Text==TEXT("none"))return FLinearColor::Transparent;if(Text.StartsWith(TEXT("#"))){FColor C=FColor::FromHex(Text);return FLinearColor(C.R/255.f,C.G/255.f,C.B/255.f,C.A/255.f);}Text.ReplaceInline(TEXT("rgba("),TEXT(""));Text.ReplaceInline(TEXT("rgb("),TEXT(""));Text.ReplaceInline(TEXT(")"),TEXT(""));TArray<FString> A;Text.ParseIntoArray(A,TEXT(","));return A.Num()>=3?FLinearColor(FCString::Atof(*A[0])/255.f,FCString::Atof(*A[1])/255.f,FCString::Atof(*A[2])/255.f,A.Num()>3?FCString::Atof(*A[3]):1):FLinearColor::Transparent;}
struct Shape{double X=0,Y=0,R=0,Width=0;FString Fill,Stroke;float FO=1,SO=1;};
using CircleDefinitions=TArray<TArray<Shape>>;
TMap<FString,TSharedPtr<FJsonObject>> VectorTemplates;
TMap<FString,TSharedPtr<const CircleDefinitions>> CircleTemplates;
void ReadCircles(const Obj& Source,CircleDefinitions& Out){const TArray<TSharedPtr<FJsonValue>>* States=nullptr;if(Source&&Source->TryGetArrayField(TEXT("circleStates"),States))for(const auto& V:*States){TArray<Shape> Shapes;for(const auto& C:V->AsObject()->GetArrayField(TEXT("circles"))){auto O=C->AsObject();Shape Q;Q.X=O->GetNumberField(TEXT("cx"));Q.Y=O->GetNumberField(TEXT("cy"));Q.R=O->GetNumberField(TEXT("r"));Q.Width=O->GetNumberField(TEXT("width"));Q.Fill=S(O,TEXT("fill"));Q.Stroke=S(O,TEXT("stroke"));Q.FO=O->GetNumberField(TEXT("fillOpacity"));Q.SO=O->GetNumberField(TEXT("strokeOpacity"));Shapes.Add(MoveTemp(Q));}Out.Add(MoveTemp(Shapes));}}

struct Curve{FVector2D P[4];};
struct Path{FString Key;TArray<FVector2D> Points;TArray<Curve> Curves;};
struct Polygon{TArray<FVector2D> Points;TArray<SlateIndex> Indices;FString Fill,Stroke;double Width=0;};
class SAuthoredVector : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SAuthoredVector){} SLATE_END_ARGS()
 void Construct(const FArguments&){}
 CircleDefinitions States;TSharedPtr<const CircleDefinitions> SharedStates;TArray<Path> Paths;TArray<Polygon> Polygons;TSet<FString> Lit,Route;FLinearColor Color;int Mask=0;float Zoom=1;double Scale=1;FVector2D Origin;
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(1,1);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool)const override{
  // Slate's rounded-box shader and line builder use a fixed antialias filter.
  // Paint the uniformly zoomed graph in final pixels so that filter is not
  // enlarged by its render transform (5.63x previously meant a ~5.63px fringe).
  const FVector2D PixelOrigin=G.LocalToAbsolute(FVector2D::ZeroVector);
  const double PixelScale=(G.LocalToAbsolute(FVector2D(1,0))-PixelOrigin).Size();
  const FVector2D Size=FVector2D(G.GetLocalSize())*PixelScale;
  const FGeometry Pixels=FGeometry::MakeRoot(Size,FSlateLayoutTransform(PixelOrigin));
  const float StrokeScale=PixelScale/FMath::Max(.01f,Zoom);
  const auto FX=ESlateDrawEffect::NoGamma|ESlateDrawEffect::NoPixelSnapping;
  const auto& CircleStates=SharedStates?*SharedStates:States;
  if(CircleStates.IsValidIndex(Mask))for(const auto& V:CircleStates[Mask]){FLinearColor F=V.Fill==TEXT("node")?Color:CSS(V.Fill),C=V.Stroke==TEXT("node")?Color:CSS(V.Stroke);F.A*=V.FO;C.A*=V.SO;const float BaseRadius=V.R*Size.X,W=C.A>0?V.Width*StrokeScale:0,R=BaseRadius+W*.5f;FSlateRoundedBoxBrush Brush(F,R,C,W);FSlateDrawElement::MakeBox(Out,L,Pixels.ToPaintGeometry(FVector2D(R*2,R*2),FSlateLayoutTransform(FVector2D(V.X*Size.X-R,V.Y*Size.Y-R))),&Brush,FX,F);}
  for(const auto& P:Paths){const bool On=Lit.Contains(P.Key),Way=Route.Contains(P.Key);FLinearColor C=On?CSS(TEXT("#efc975")):Way?CSS(TEXT("#e9c37b")):CSS(TEXT("#b19c6f"));C.A=On?1:Way?.9:.52;const float Thickness=(On?1.55f:Way?1.f:.65f)*StrokeScale;
   if(!P.Curves.IsEmpty())for(const auto& Q:P.Curves){FVector2D CP[4];for(int I=0;I<4;I++)CP[I]=G.LocalToAbsolute(Origin+Q.P[I]*Scale)-PixelOrigin;FSlateDrawElement::MakeCubicBezierSpline(Out,L,Pixels.ToPaintGeometry(),CP[0],CP[1],CP[2],CP[3],Thickness,FX,C);}
   else {TArray<FVector2D> Points;for(const auto& V:P.Points)Points.Add(G.LocalToAbsolute(Origin+V*Scale)-PixelOrigin);FSlateDrawElement::MakeLines(Out,L,Pixels.ToPaintGeometry(),Points,FX,C,true,Thickness);}
  }
  // Exact polygons supplied by UICreator retain their SVG viewBox geometry at any zoom.
  for(const auto& P:Polygons){
   const FLinearColor Fill=CSS(P.Fill),Stroke=CSS(P.Stroke);
   if(Fill.A>0&&!P.Indices.IsEmpty()){
    TArray<FSlateVertex> Vertices;for(const auto& V:P.Points)Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Pixels.GetAccumulatedRenderTransform(),FVector2f(V*Size),FVector2f(.5f,.5f),Fill.ToFColorSRGB()));
    const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
    FSlateDrawElement::MakeCustomVerts(Out,L,Resource,Vertices,P.Indices,nullptr,0,0,FX);
   }
   if(Stroke.A>0&&P.Width>0&&!P.Points.IsEmpty()){TArray<FVector2D> Outline;for(const auto& V:P.Points)Outline.Add(V*Size);const FVector2D FirstPoint=Outline[0];Outline.Add(FirstPoint);FSlateDrawElement::MakeLines(Out,L,Pixels.ToPaintGeometry(),Outline,FX,Stroke,true,P.Width*Size.X);}
  }
  return L;
 }
};
TMap<TWeakObjectPtr<USvgCanvasNode>,TSharedPtr<SAuthoredVector>> Vectors;
TSharedPtr<SAuthoredVector> Vector(USvgCanvasNode* N){if(!N)return nullptr;auto& W=Vectors.FindOrAdd(N);if(W){if(N->Surface)N->Surface->SetVisibility(ESlateVisibility::Collapsed);return W;}const auto P=Props(N);const Obj* Source=nullptr;if(!P||!P->TryGetObjectField(TEXT("source"),Source))return nullptr;W=SNew(SAuthoredVector);
 const FString Template=S(*Source,TEXT("circleTemplate"));
 if(!Template.IsEmpty()){auto& Shared=CircleTemplates.FindOrAdd(Template);if(!Shared){auto Parsed=MakeShared<CircleDefinitions>();if(const auto* Definition=VectorTemplates.Find(Template))ReadCircles(*Definition,*Parsed);Shared=Parsed;}W->SharedStates=Shared;}
 else ReadCircles(*Source,W->States);
 const TArray<TSharedPtr<FJsonValue>>* Paths=nullptr;if((*Source)->TryGetArrayField(TEXT("vectorPaths"),Paths)){W->Scale=(*Source)->GetNumberField(TEXT("graphScale"));const auto O=(*Source)->GetObjectField(TEXT("origin"));W->Origin=FVector2D(O->GetNumberField(TEXT("x")),O->GetNumberField(TEXT("y")));for(const auto& V:*Paths){Path Q;const auto O2=V->AsObject();Q.Key=S(O2,TEXT("key"));const TArray<TSharedPtr<FJsonValue>>* Curves=nullptr;if(O2->TryGetArrayField(TEXT("curves"),Curves))for(const auto& Segment:*Curves){const auto& CP=Segment->AsArray();if(CP.Num()!=4)continue;Curve C;for(int I=0;I<4;I++){const auto PT=CP[I]->AsObject();C.P[I]=FVector2D(PT->GetNumberField(TEXT("x")),PT->GetNumberField(TEXT("y")));}Q.Curves.Add(C);}const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;if(O2->TryGetArrayField(TEXT("points"),Points))for(const auto& X:*Points){const auto PT=X->AsObject();Q.Points.Add(FVector2D(PT->GetNumberField(TEXT("x")),PT->GetNumberField(TEXT("y"))));}W->Paths.Add(Q);}}
 const TArray<TSharedPtr<FJsonValue>>* Polygons=nullptr;if((*Source)->TryGetArrayField(TEXT("vectorPolygons"),Polygons))for(const auto& V:*Polygons){const auto O=V->AsObject();if(!O)continue;Polygon Q;Q.Fill=S(O,TEXT("fill"));Q.Stroke=S(O,TEXT("stroke"));O->TryGetNumberField(TEXT("width"),Q.Width);const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;const TArray<TSharedPtr<FJsonValue>>* Indices=nullptr;if(!O->TryGetArrayField(TEXT("points"),Points)||!O->TryGetArrayField(TEXT("indices"),Indices))continue;for(const auto& X:*Points){const auto PointObject=X->AsObject();if(PointObject)Q.Points.Add(FVector2D(PointObject->GetNumberField(TEXT("x")),PointObject->GetNumberField(TEXT("y"))));}bool Valid=Indices->Num()%3==0&&Q.Points.Num()<=MAX_uint16;for(const auto& X:*Indices){double I=X->AsNumber();if(I!=FMath::FloorToDouble(I)||!Q.Points.IsValidIndex(int32(I))){Valid=false;break;}Q.Indices.Add(SlateIndex(I));}if(Valid)W->Polygons.Add(MoveTemp(Q));}
 auto* Host=NewObject<UNativeWidgetHost>(N);Host->SetContent(W.ToSharedRef());Host->SetVisibility(ESlateVisibility::HitTestInvisible);auto* Slot=N->AddChildToCanvas(Host);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));if(N->Surface)N->Surface->SetVisibility(ESlateVisibility::Collapsed);return W;}
TMap<TWeakObjectPtr<UComboBoxString>,TArray<FString>> OptionValues;
}
void OpusAuthored::RegisterVectorTemplates(const FString& Scope,const TSharedPtr<FJsonObject>& Definitions){if(Definitions)for(const auto& Pair:Definitions->Values){const FString Key=Scope+TEXT(":")+FString(Pair.Key.ToView());if(!VectorTemplates.Contains(Key))VectorTemplates.Add(Key,Pair.Value->AsObject());}for(auto It=Vectors.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();}
void UOpusTalentBridge::CircleState(USvgCanvasScreen* Screen,const FString& Id,int32 Mask,FLinearColor Color,float Zoom){auto W=Vector(Screen?Screen->FindSourceNode(Id):nullptr);if(W){W->Mask=FMath::Clamp(Mask,0,15);W->Color=Color;W->Zoom=Zoom;W->Invalidate(EInvalidateWidgetReason::Paint);}}
void UOpusTalentBridge::LineState(USvgCanvasScreen* Screen,const FString& Id,const TArray<FString>& Lit,const TArray<FString>& Route,float Zoom){auto W=Vector(Screen?Screen->FindSourceNode(Id):nullptr);if(W){W->Lit.Empty();W->Route.Empty();for(const auto& P:W->Paths){FString A,B;P.Key.Split(TEXT(":"),&A,&B);if(Lit.Contains(A)&&Lit.Contains(B))W->Lit.Add(P.Key);if(Route.Contains(A)&&Route.Contains(B))W->Route.Add(P.Key);}W->Zoom=Zoom;W->Invalidate(EInvalidateWidgetReason::Paint);}}
void UOpusTalentBridge::Rect(USvgCanvasScreen* Screen,const FString& Id,FVector4 V){auto* N=Screen?Screen->FindSourceNode(Id):nullptr;if(!N)return;if(Cast<USvgCanvasNode>(N->GetParent())){const bool Painted=N->Surface&&N->Surface->GetVisibility()!=ESlateVisibility::Collapsed;OpusSvg::PlaceNode(N,V.X,V.Y,V.Z,V.W);if(Painted)N->ApplyRaster(N->Normal);return;}USvgCanvasNode* Owner=nullptr;for(auto* P=N->GetParent();P;P=P->GetParent())if((Owner=Cast<USvgCanvasNode>(P)))break;const FVector2D At(V.X+(Owner?Owner->DesignRect.X:0),V.Y+(Owner?Owner->DesignRect.Y:0)),Delta=At-FVector2D(N->DesignRect.X,N->DesignRect.Y);TFunction<void(USvgCanvasNode*)> Move=[&](auto* C){C->DesignRect.X+=Delta.X;C->DesignRect.Y+=Delta.Y;for(auto* R:{&C->Normal,&C->Hovered,&C->Pressed,&C->Disabled})R->PixelOrigin+=Delta;for(auto* W:C->GetAllChildren())if(auto* Child=Cast<USvgCanvasNode>(W))Move(Child);};Move(N);N->DesignRect.Z=V.Z;N->DesignRect.W=V.W;if(auto* Slot=Cast<UCanvasPanelSlot>(N->Slot)){Slot->SetPosition(FVector2D(V.X,V.Y));Slot->SetSize(FVector2D(V.Z,V.W));}if(auto* Canvas=Cast<UCanvasPanel>(N->GetParent()))if(auto* Extent=Cast<USizeBox>(Canvas->GetParent())){double Bottom=0;for(auto* C:Canvas->GetAllChildren())if(auto* Slot=Cast<UCanvasPanelSlot>(C->Slot))Bottom=FMath::Max(Bottom,Slot->GetPosition().Y+Slot->GetSize().Y);Extent->SetHeightOverride(Bottom);}}

void UOpusTalentBridge::InputValue(USvgCanvasScreen* Screen,const FString& Id,const FString& Value){auto* N=Screen?Screen->FindSourceNode(Id):nullptr;if(N)for(auto* C:N->GetAllChildren())if(auto* E=Cast<UEditableTextBox>(C))E->SetText(FText::FromString(Value));}
void UOpusTalentBridge::Options(USvgCanvasScreen* Screen,const FString& Id,const TArray<FString>& Labels,const TArray<FString>& Values,const FString& Current){auto* N=Screen?Screen->FindSourceNode(Id):nullptr;if(!N||Labels.Num()!=Values.Num())return;UComboBoxString* Box=nullptr;for(auto* C:N->GetAllChildren())if(auto* B=Cast<UComboBoxString>(C))Box=B;if(!Box){Box=NewObject<UComboBoxString>(N);Box->Font=FSlateFontInfo(FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")),TEXT("Fonts/msyh.ttc")),8);Box->ForegroundColor=FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("a9b8ce")));auto* Slot=N->AddChildToCanvas(Box);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));Slot->SetZOrder(80);auto St=Box->GetWidgetStyle();FSlateRoundedBoxBrush Bg(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("101720"))),0.f,FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("344054"))),1);St.ComboButtonStyle.ButtonStyle.Normal=St.ComboButtonStyle.ButtonStyle.Hovered=St.ComboButtonStyle.ButtonStyle.Pressed=Bg;Box->SetWidgetStyle(St);for(auto* C:N->GetAllChildren())if(C!=Box)C->SetVisibility(ESlateVisibility::Collapsed);}Box->ClearOptions();for(const auto& Label:Labels)Box->AddOption(Label);OptionValues.Add(Box,Values);const int I=Values.Find(Current);Box->SetSelectedIndex(I<0?0:I);}
FString UOpusTalentBridge::Option(USvgCanvasScreen* Screen,const FString& Id){auto* N=Screen?Screen->FindSourceNode(Id):nullptr;if(N)for(auto* C:N->GetAllChildren())if(auto* B=Cast<UComboBoxString>(C))if(auto* V=OptionValues.Find(B)){const int I=B->GetSelectedIndex();if(V->IsValidIndex(I))return (*V)[I];}return {};}
void UOpusTalentBridge::Scroll(USvgCanvasScreen* Screen,const FString& Id){auto* N=Screen?Screen->FindSourceNode(Id):nullptr;if(!N)return;for(auto* C:N->GetAllChildren())if(Cast<UScrollBox>(C))return;const auto Children=N->GetAllChildren();auto* Scroll=NewObject<UScrollBox>(N);Scroll->SetScrollbarThickness(FVector2D(5,5));Scroll->SetScrollbarPadding(FMargin(0));auto* Canvas=NewObject<UCanvasPanel>(Scroll);double Bottom=N->DesignRect.W;for(auto* C:Children)if(auto* Child=Cast<USvgCanvasNode>(C)){auto* Old=Cast<UCanvasPanelSlot>(Child->Slot);FVector2D Pos=Old->GetPosition(),Size=Old->GetSize();int Order=Old->GetZOrder();Child->RemoveFromParent();auto* Slot=Canvas->AddChildToCanvas(Child);Slot->SetPosition(Pos);Slot->SetSize(Size);Slot->SetZOrder(Order);Bottom=FMath::Max(Bottom,Pos.Y+Size.Y);}auto* Size=NewObject<USizeBox>(Scroll);Size->SetHeightOverride(Bottom);Size->AddChild(Canvas);Scroll->AddChild(Size);auto* Slot=N->AddChildToCanvas(Scroll);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));Slot->SetZOrder(80);}
TArray<FOpusTalentRecord> UOpusTalentBridge::Sectors(){FString Text;FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Rules/Talents/constellation.json")));auto O=Parse(Text);TArray<FOpusTalentRecord> Out;if(O)for(const auto& V:O->GetArrayField(TEXT("sectors"))){auto P=V->AsObject();FOpusTalentRecord R;R.Id=S(P,TEXT("id"));R.Name=S(P,TEXT("name"));R.Color=S(P,TEXT("color"));Out.Add(R);}return Out;}
TArray<FOpusTalentRecord> UOpusTalentBridge::Builds(UDWRuleWorldSubsystem* Rules){TArray<FOpusTalentRecord> Out;if(Rules)for(const auto& V:Rules->Library().Builds){FOpusTalentRecord R;R.Id=V.Key.ToString();R.Name=V.Value.DisplayName.IsEmpty()?R.Id:V.Value.DisplayName;Out.Add(R);}Out.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});return Out;}
FString UOpusTalentBridge::Effects(UDWRuleWorldSubsystem* Rules,const FString& Traits){if(!Rules)return {};TArray<TSharedPtr<FJsonValue>> Array;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Traits),Array))return {};FString Result;TFunction<void(const TSharedPtr<FJsonValue>&,int)> Visit=[&](const auto& V,int Depth){DWRules::FDWTraitEntry Entry;FString Error;if(!FDWRuleProgramReader::ReadTraitEntry(V,Entry,Error))return;const auto* T=Rules->Library().Traits.Find(Entry.Id);Result+=(Depth?TEXT("↳ "):TEXT("◇ "))+(T?OpusTraitText::Format(T->DisplayName.IsEmpty()?T->Description:T->DisplayName,Entry,Rules->Library()):Entry.Id.ToString())+TEXT("\n");const TArray<TSharedPtr<FJsonValue>>* Children=nullptr;if(V->AsObject()->TryGetArrayField(TEXT("with"),Children))for(const auto& Child:*Children)Visit(Child,Depth+1);};for(const auto& V:Array)Visit(V,0);return Result;}
FString UOpusTalentBridge::Preview(UDWRuleWorldSubsystem* Rules,const FString& Build,const TArray<FString>& Ids,const TArray<FString>& Traits){if(!Rules||Ids.Num()!=Traits.Num())return TEXT("规则未就绪");TArray<DWRules::FDWCollectionPart> Parts;FString Error;for(int I=0;I<Ids.Num();I++){DWRules::FDWCollectionPart P;P.Collection.Id=FName(*Ids[I]);TArray<TSharedPtr<FJsonValue>> Array;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Traits[I]),Array))return TEXT("特性格式错误");for(const auto& V:Array){DWRules::FDWTraitEntry E;if(!FDWRuleProgramReader::ReadTraitEntry(V,E,Error))return Error;P.Collection.Traits.Add(E);}Parts.Add(P);}DWRules::FDWBuildSource Source;TArray<DWRules::FDWContribution> Contributions;if(!Rules->Library().Aggregate(FName(*Build),Parts,0,Source,Contributions,Error))return Error;const auto Plan=DWRules::FDWRuleCompiler::Compile(Source,Error);if(!Plan)return Error;DWRules::FDWSheet Sheet;TArray<DWRules::FDWHeldTrait> Held;for(const auto& C:Contributions)Held.Add(C.Held);if(!Sheet.Compute(Held,Error))return Error;return FString::Printf(TEXT("编译通过 · %d 个来源 · %d 条规则 · %d 个触发"),Contributions.Num(),Plan->Rules.Num(),Sheet.Triggers.Num());}
FString UOpusTalentBridge::PlanJson(const FString& Text,bool Encode){if(Encode){TArray<FString> Lines;Text.ParseIntoArrayLines(Lines);if(Lines.IsEmpty())return {};TArray<FString> H;Lines[0].ParseIntoArray(H,TEXT("|"));if(H.Num()!=3)return {};auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("schema"),1);O->SetStringField(TEXT("treeId"),H[0]);O->SetNumberField(TEXT("revision"),FCString::Atoi(*H[1]));O->SetNumberField(TEXT("budget"),FCString::Atoi(*H[2]));TArray<TSharedPtr<FJsonValue>> Alloc;auto Choices=MakeShared<FJsonObject>();const auto Catalog=UOpusTalentBridge::Catalog(),Attrs=UOpusTalentBridge::Attributes();for(int I=1;I<Lines.Num();I++){TArray<FString> P;Lines[I].ParseIntoArray(P,TEXT("|"));if(P.Num()!=2)continue;Alloc.Add(MakeShared<FJsonValueString>(P[0]));const auto* N=Catalog.FindByPredicate([&](const auto& R){return R.Id==P[0];});int A=FCString::Atoi(*P[1]);if(N&&N->Kind==TEXT("attribute")&&Attrs.IsValidIndex(A))Choices->SetStringField(P[0],Attrs[A].Id);}O->SetArrayField(TEXT("allocated"),Alloc);O->SetObjectField(TEXT("choices"),Choices);FString Out;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Out));return Out;}
 const auto O=Parse(Text);if(!O)return {};double Schema=0,Revision=0,Budget=0;if(!O->TryGetNumberField(TEXT("schema"),Schema)||Schema!=1||!O->TryGetNumberField(TEXT("revision"),Revision)||Revision!=3||!O->TryGetNumberField(TEXT("budget"),Budget)||Budget!=FMath::FloorToDouble(Budget)||S(O,TEXT("treeId"))!=TEXT("opus_constellation"))return {};const TArray<TSharedPtr<FJsonValue>>* Alloc=nullptr;const Obj* Choices=nullptr;if(!O->TryGetArrayField(TEXT("allocated"),Alloc)||!O->TryGetObjectField(TEXT("choices"),Choices))return {};const auto Catalog=UOpusTalentBridge::Catalog(),Attrs=UOpusTalentBridge::Attributes();FString Out=FString::Printf(TEXT("opus_constellation|3|%d\n"),int(Budget));for(const auto& V:*Alloc){FString Id;if(!V->TryGetString(Id))return {};const auto* N=Catalog.FindByPredicate([&](const auto& R){return R.Id==Id;});if(!N)return {};int A=0;if(N->Kind==TEXT("attribute")){FString Choice=S(*Choices,*Id);A=Attrs.IndexOfByPredicate([&](const auto& R){return R.Id==Choice;});if(A<0)return {};}Out+=Id+TEXT("|")+FString::FromInt(A)+TEXT("\n");}for(const auto& V:(*Choices)->Values){const auto* N=Catalog.FindByPredicate([&](const auto& R){return R.Id==FString(V.Key.ToView());});if(!N||N->Kind!=TEXT("attribute")||!Attrs.ContainsByPredicate([&](const auto& R){return R.Id==V.Value->AsString();}))return {};}return Out;}
FString UOpusTalentBridge::PlanFile(const FString& Text,bool Save){TArray<FString> Files;
#if WITH_EDITOR
 if(auto* Desktop=FDesktopPlatformModule::Get()){bool Ok=Save?Desktop->SaveFileDialog(nullptr,TEXT("导出天赋方案"),FPaths::ProjectSavedDir()/TEXT("Opus"),TEXT("opus-talents.json"),TEXT("JSON (*.json)|*.json"),0,Files):Desktop->OpenFileDialog(nullptr,TEXT("导入天赋方案"),FPaths::ProjectSavedDir()/TEXT("Opus"),TEXT(""),TEXT("JSON (*.json)|*.json"),0,Files);if(!Ok||Files.IsEmpty())return {};}
#endif
 if(Files.IsEmpty())Files.Add(FPaths::ProjectSavedDir()/TEXT("Opus/opus-talents.json"));if(Save)return FFileHelper::SaveStringToFile(Text,*Files[0],FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)?Files[0]:TEXT("");FString Result;FFileHelper::LoadFileToString(Result,*Files[0]);return Result;}

FLinearColor UOpusTalentBridge::CssColor(const FString& V){return CSS(V);}
float UOpusTalentBridge::Measure(USvgCanvasScreen* Screen,const FString& Id,const FString& Text,float Width){auto* N=Screen?Screen->FindSourceNode(Id):nullptr;return N?OpusSvg::TextHeight(N,Text,Width,0):0;}
void UOpusTalentBridge::CopyFrom(USvgCanvasScreen* Target,const FString& Id,USvgCanvasScreen* Source,const FString& SourceId){auto* T=Target?Target->FindSourceNode(Id):nullptr;auto* F=Source?Source->FindSourceNode(SourceId):nullptr;if(!T||!F)return;const FVector2D Scale(T->DesignRect.Z/FMath::Max(1.,F->DesignRect.Z),T->DesignRect.W/FMath::Max(1.,F->DesignRect.W));T->Normal=F->Normal;T->Normal.PixelOrigin=FVector2D(T->DesignRect.X,T->DesignRect.Y)+(F->Normal.PixelOrigin-FVector2D(F->DesignRect.X,F->DesignRect.Y))*Scale;T->Normal.PixelSize*=Scale;T->Hovered=T->Pressed=T->Disabled=T->Normal;T->ApplyRaster(T->Normal);}
