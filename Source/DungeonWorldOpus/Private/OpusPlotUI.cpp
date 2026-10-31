// A generic UICreator vector surface: authored art/style + AS-owned primitives.
#include "OpusMapUI.h"
#include "SvgCanvasNode.h"
#include "Components/NativeWidgetHost.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
namespace {
using Obj=TSharedPtr<FJsonObject>;
FString Str(const Obj& O,const TCHAR* K){FString S;if(O)O->TryGetStringField(K,S);return S;}
FLinearColor Color(FString S){if(S.IsEmpty()||S==TEXT("none"))return FLinearColor::Transparent;if(S.StartsWith(TEXT("#"))){const auto C=FColor::FromHex(S);return FLinearColor(C.R/255.f,C.G/255.f,C.B/255.f,C.A/255.f);}S.ReplaceInline(TEXT("rgba("),TEXT(""));S.ReplaceInline(TEXT(")"),TEXT(""));TArray<FString> V;S.ParseIntoArray(V,TEXT(","));return V.Num()<3?FLinearColor::Transparent:FLinearColor(FCString::Atof(*V[0])/255,FCString::Atof(*V[1])/255,FCString::Atof(*V[2])/255,V.Num()>3?FCString::Atof(*V[3]):1);}
class SAuthoredPlot : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SAuthoredPlot){} SLATE_END_ARGS()
 void Construct(const FArguments&){}
 TArray<FOpusPlotPoint> Points;TArray<FOpusPlotPath> Paths;TArray<FOpusPlotRect> Rects;TArray<TArray<TArray<FVector2D>>> Glyphs;bool CircleClip=false;
 FSlateBrush Terrain,Fog,Glow;TStrongObjectPtr<UTexture2D> FogTexture,GlowTexture;
 FVector4 Bounds;FVector2D Center;float Zoom=1,FogOpacity=.85f,FontSize=11;FString LabelFill;FLinearColor FogColor;uint32 FogHash=0;
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(1,1);}
 UTexture2D* Texture(int Size,TArray<FColor>& Pixels){auto* T=UTexture2D::CreateTransient(Size,Size,PF_B8G8R8A8);if(!T)return nullptr;T->SRGB=false;T->Filter=TF_Bilinear;T->NeverStream=true;auto& M=T->GetPlatformData()->Mips[0];void* Data=M.BulkData.Lock(LOCK_READ_WRITE);FMemory::Memcpy(Data,Pixels.GetData(),Pixels.Num()*sizeof(FColor));M.BulkData.Unlock();T->UpdateResource();return T;}
 void UpdateFog(){uint32 H=GetTypeHash(Bounds);for(const auto& P:Points)if(P.FogRadius>0){H=HashCombine(H,GetTypeHash(P.Position));H=HashCombine(H,GetTypeHash(P.FogRadius));H=HashCombine(H,GetTypeHash(P.FogStrength));}if(FogTexture.IsValid()&&H==FogHash)return;FogHash=H;constexpr int Size=768;TArray<float> Alpha;Alpha.Init(FogOpacity,Size*Size);const float K=Size/Bounds.Z;
  for(const auto& P:Points)if(P.FogRadius>0){const FVector2D C=(P.Position-FVector2D(Bounds.X,Bounds.Y))*K;const float R=P.FogRadius*K;for(int Y=FMath::Max(0,FMath::FloorToInt(C.Y-R));Y<FMath::Min(Size,FMath::CeilToInt(C.Y+R));Y++)for(int X=FMath::Max(0,FMath::FloorToInt(C.X-R));X<FMath::Min(Size,FMath::CeilToInt(C.X+R));X++){const float D=FVector2D::Distance(FVector2D(X+.5,Y+.5),C)/R;if(D<1)Alpha[Y*Size+X]*=1-P.FogStrength*FMath::Clamp((1-D)/.7f,0.f,1.f);}}
  TArray<FColor> Pixels;Pixels.SetNum(Size*Size);for(int I=0;I<Pixels.Num();I++)Pixels[I]=FColor(FogColor.R*255,FogColor.G*255,FogColor.B*255,FMath::RoundToInt(Alpha[I]*255));FogTexture.Reset(Texture(Size,Pixels));Fog.SetResourceObject(FogTexture.Get());Fog.ImageSize=FVector2D(Size,Size);Fog.DrawAs=ESlateBrushDrawType::Image;
 }
 void InitGlow(){if(GlowTexture.IsValid())return;constexpr int Size=128;TArray<FColor> Pixels;Pixels.SetNum(Size*Size);for(int Y=0;Y<Size;Y++)for(int X=0;X<Size;X++){float D=FVector2D(X+.5-Size*.5,Y+.5-Size*.5).Size()/(Size*.5);Pixels[Y*Size+X]=FColor(255,255,255,FMath::RoundToInt(FMath::Clamp((1-D)/.875f,0.f,1.f)*255));}GlowTexture.Reset(Texture(Size,Pixels));Glow.SetResourceObject(GlowTexture.Get());Glow.ImageSize=FVector2D(Size,Size);Glow.DrawAs=ESlateBrushDrawType::Image;}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override{
  const auto Size=G.GetLocalSize();auto At=[&](FVector2D P){return (P-Center)*Zoom+Size*.5;};const auto FX=ESlateDrawEffect::NoGamma;
  auto Box=[&](const FSlateBrush& B,FVector2D P,FVector2D S,FLinearColor C,int L){FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(S,FSlateLayoutTransform(P)),&B,FX,C);};
  const FVector2D P=At(FVector2D(Bounds.X,Bounds.Y));const FVector2D S=FVector2D(Bounds.Z,Bounds.W)*Zoom;Box(Terrain,P,S,FLinearColor::White,Layer);Box(Fog,P,S,FLinearColor::White,Layer+1);
  const float ClipRadius=FMath::Min(Size.X,Size.Y)*.5f;
  for(const auto& R:Rects){const auto A=At(FVector2D(R.Rect.X,R.Rect.Y)),B=A+FVector2D(R.Rect.Z,R.Rect.W)*Zoom;const auto Tone=Color(R.Fill);FSlateRoundedBoxBrush Brush(Tone,0.f);
   if(!CircleClip){Box(Brush,A,B-A,Tone,Layer+2);continue;}
   const FVector2D C=Size*.5;const float Far=FMath::Max(FMath::Max((A-C).Size(),(B-C).Size()),FMath::Max((FVector2D(A.X,B.Y)-C).Size(),(FVector2D(B.X,A.Y)-C).Size()));
   if(Far<=ClipRadius){Box(Brush,A,B-A,Tone,Layer+2);continue;}
   // Clip live geometry to the authored circle, without opaque corner covers.
   for(float Y=FMath::Max(0.f,float(A.Y));Y<FMath::Min(float(Size.Y),float(B.Y));Y+=1.f){const float H=FMath::Min(1.f,float(B.Y)-Y),DY=Y+H*.5f-Size.Y*.5f;const float DX=FMath::Sqrt(FMath::Max(0.f,ClipRadius*ClipRadius-DY*DY));const float Left=FMath::Max(float(A.X),float(Size.X*.5f-DX)),Right=FMath::Min(float(B.X),float(Size.X*.5f+DX));if(Right>Left)Box(Brush,FVector2D(Left,Y),FVector2D(Right-Left,H),Tone,Layer+2);}
  }
  auto Stroke=[&](const TArray<FVector2D>& V,FLinearColor C,float W,int L){if(V.Num()>1)FSlateDrawElement::MakeLines(Out,L,G.ToPaintGeometry(),V,FX,C,true,W);};
  auto Circle=[&](FVector2D C,float R,FLinearColor F,FLinearColor Border,float W,int L){if(F.A>0){FSlateRoundedBoxBrush B(F,R);Box(B,C-FVector2D(R,R),FVector2D(R*2,R*2),F,L);}if(Border.A>0&&W>0){TArray<FVector2D> V;for(int I=0;I<=64;I++){const double A=I*2*PI/64;V.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*R);}Stroke(V,Border,W,L+1);}};
  for(const auto& Path:Paths){TArray<FVector2D> V;for(auto A:Path.Points)V.Add(At(A));if(Path.Dash<=0)Stroke(V,Color(Path.Color),Path.Width,Layer+2);else {float Distance=0;for(int I=1;I<V.Num();I++){auto Delta=V[I]-V[I-1];const float Len=Delta.Size();if(Len<=0)continue;for(float D=0;D<Len;){const float Phase=FMath::Fmod(Distance+D,Path.Dash*4),Next=FMath::Min(Len-D,(Phase<Path.Dash?Path.Dash:Path.Dash*4)-Phase);if(Next<=.001f){D+=.001f;continue;}if(Phase<Path.Dash)Stroke({V[I-1]+Delta*(D/Len),V[I-1]+Delta*((D+Next)/Len)},Color(Path.Color),Path.Width,Layer+2);D+=Next;}Distance+=Len;}}}
  const FSlateFontInfo Font(FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")),TEXT("Fonts/msyh.ttc")),FMath::RoundToInt(FontSize*.75f));const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
  for(const auto& Point:Points){const auto C=At(Point.Position);const float R=Point.Radius*Zoom;if(CircleClip&&(C-Size*.5).Size()+R>ClipRadius)continue;if(C.X<-80||C.Y<-80||C.X>Size.X+80||C.Y>Size.Y+80)continue;
   if(!Point.Glow.IsEmpty()){const float GR=R*3.2f;Box(Glow,C-FVector2D(GR,GR),FVector2D(GR*2,GR*2),Color(Point.Glow),Layer+3);}if(Point.Ring>0){Circle(C,Point.Ring*Zoom,FLinearColor::Transparent,Color(Point.RingColor),1,Layer+4);auto Tone=Color(Point.RingColor);Tone.A*=.35f;for(int Half=0;Half<2;Half++){TArray<FVector2D> Arc;for(int I=0;I<=20;I++){double A=Half*PI-.7+1.6*I/20;Arc.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*(Point.Ring*Zoom+4));}Stroke(Arc,Tone,1,Layer+4);}}
   Circle(C,R,Color(Point.Fill),Color(Point.Stroke),Point.Width,Layer+5);
   const float GR=Point.GlyphRadius*Zoom;if(Point.Glyph==-2)Circle(C,GR,Color(Point.Stroke),FLinearColor::Transparent,0,Layer+7);else if(Point.Glyph==-3)Stroke({C+FVector2D(-R*.4,0),C+FVector2D(-R*.1,R*.3),C+FVector2D(R*.45,-R*.35)},Color(Point.Stroke),1.8f,Layer+7);else if(Glyphs.IsValidIndex(Point.Glyph)){for(const auto& Path:Glyphs[Point.Glyph]){TArray<FVector2D> V;for(auto A:Path){const double Angle=FMath::DegreesToRadians(Point.Angle);V.Add(C+FVector2D(A.X*FMath::Cos(Angle)-A.Y*FMath::Sin(Angle),A.X*FMath::Sin(Angle)+A.Y*FMath::Cos(Angle))*GR);}Stroke(V,Color(Point.Stroke),1.3f*Zoom,Layer+7);}if(Point.Glyph==3)Circle(C,GR*.35f,FLinearColor::Transparent,Color(Point.Stroke),1.3f*Zoom,Layer+7);}
   if(!Point.Label.IsEmpty()){const auto Extent=Measure->Measure(Point.Label,Font);const auto Top=C+FVector2D(-Extent.X*.5,R+9);FSlateRoundedBoxBrush B(Color(LabelFill),0.f);Box(B,Top-FVector2D(8,0),FVector2D(Extent.X+16,20),Color(LabelFill),Layer+8);FSlateDrawElement::MakeText(Out,Layer+9,G.ToPaintGeometry(FSlateLayoutTransform(Top+FVector2D(0,4))),Point.Label,Font,FX,Color(Point.LabelColor));}
  }return Layer+10;
 }
};
TMap<TWeakObjectPtr<USvgCanvasNode>,TSharedPtr<SAuthoredPlot>> Plots;
}
void UOpusMapUI::Plot(UOpusWorldScreen* Screen,const FString& Id,const TArray<FOpusPlotPoint>& Points,const TArray<FOpusPlotPath>& Paths,FVector2D Center,float Zoom){
 auto* N=Screen?Screen->FindSourceNode(Id):nullptr;if(!N)return;for(auto It=Plots.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();auto& W=Plots.FindOrAdd(N);if(!W){Obj O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(N->SourcePropertiesJson),O);if(!O)return;const Obj* Source=nullptr;const Obj* Style=nullptr;if(!O->TryGetObjectField(TEXT("source"),Source)||!(*Source)->TryGetObjectField(TEXT("plotSurface"),Style))return;
  W=SNew(SAuthoredPlot);auto St=*Style;auto B=St->GetObjectField(TEXT("bounds"));W->Bounds=FVector4(B->GetNumberField(TEXT("x")),B->GetNumberField(TEXT("y")),B->GetNumberField(TEXT("width")),B->GetNumberField(TEXT("height")));St->TryGetBoolField(TEXT("circle"),W->CircleClip);W->FogColor=Color(Str(St,TEXT("fogColor")));W->FogOpacity=St->GetNumberField(TEXT("fogOpacity"));W->FontSize=St->GetNumberField(TEXT("fontSize"));W->LabelFill=Str(St,TEXT("labelFill"));
  for(const auto& Glyph:St->GetArrayField(TEXT("glyphs"))){TArray<TArray<FVector2D>> Shape;for(const auto& Path:Glyph->AsArray()){TArray<FVector2D> V;for(const auto& Point:Path->AsArray()){const auto& A=Point->AsArray();V.Add(FVector2D(A[0]->AsNumber(),A[1]->AsNumber()));}Shape.Add(V);}W->Glyphs.Add(Shape);}
  if(Screen->ImportedAssets)if(const auto* State=Screen->ImportedAssets->States.Find(TEXT("canvas")))if(const auto* Node=State->Nodes.FindByPredicate([&](const auto& V){return V.Id==Str(St,TEXT("terrainResource"));}))if(const auto* R=Screen->ImportedAssets->Rasters.Find(Node->Raster)){W->Terrain.SetResourceObject(R->Texture);W->Terrain.ImageSize=R->PixelSize;W->Terrain.DrawAs=ESlateBrushDrawType::Image;if(R->Texture)R->Texture->Filter=TF_Bilinear;}
  W->InitGlow();auto* Host=NewObject<UNativeWidgetHost>(N);Host->SetContent(W.ToSharedRef());Host->SetVisibility(ESlateVisibility::HitTestInvisible);auto* Slot=N->AddChildToCanvas(Host);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));if(N->Surface)N->Surface->SetVisibility(ESlateVisibility::Collapsed);if(auto* Preview=Screen->FindSourceNode(TEXT("map-device-preview-atlas-terrain-preview")))Preview->SetVisibility(ESlateVisibility::Collapsed);
 }W->Points=Points;W->Paths=Paths;W->Center=Center;W->Zoom=Zoom;W->UpdateFog();W->Invalidate(EInvalidateWidgetReason::Paint);
}

void UOpusMapUI::Rectangles(UOpusWorldScreen* Screen,const FString& Id,const TArray<FOpusPlotRect>& Rectangles){auto* N=Screen?Screen->FindSourceNode(Id):nullptr;if(auto* W=Plots.Find(N);W&&W->IsValid()){(*W)->Rects=Rectangles;if(N->Surface&&Rectangles.Num()>0)N->Surface->SetVisibility(ESlateVisibility::HitTestInvisible);(*W)->Invalidate(EInvalidateWidgetReason::Paint);}}

