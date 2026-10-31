#include "OpusSvgBinding.h"
#include "Blueprint/WidgetTree.h"
#include "SvgCanvasImage.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Fonts/CompositeFont.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "ImageUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace OpusSvg
{
namespace
{
TSharedPtr<FJsonObject> Properties(const USvgCanvasNode* N)
{
 TSharedPtr<FJsonObject> P=MakeShared<FJsonObject>();FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(N->SourcePropertiesJson),P);return P;
}
TSharedPtr<FJsonObject> Style(const USvgCanvasNode* N)
{
 auto P=Properties(N);const TSharedPtr<FJsonObject>* V=nullptr;return P->TryGetObjectField(TEXT("style"),V)?*V:MakeShared<FJsonObject>();
}
double Number(const TSharedPtr<FJsonObject>& P,const TCHAR* Key,double Default){P->TryGetNumberField(Key,Default);return Default;}
FString String(const TSharedPtr<FJsonObject>& P,const TCHAR* Key,const TCHAR* Default){FString V=Default;P->TryGetStringField(Key,V);return V;}
bool Wrapped(const USvgCanvasNode* N)
{
 bool Value=false;Style(N)->TryGetBoolField(TEXT("wrap"),Value);if(Value)return true;
 const auto* Parent=Cast<USvgCanvasNode>(N->GetParent());if(!Parent)return false;
 const auto P=Properties(Parent);const TSharedPtr<FJsonObject> *Source=nullptr,*Computed=nullptr;
 return P->TryGetObjectField(TEXT("source"),Source)&&(*Source)->TryGetObjectField(TEXT("computed"),Computed)&&String(*Computed,TEXT("whiteSpace"),TEXT(""))==TEXT("normal");
}
void Fill(UCanvasPanelSlot* S,int32 Order=30){S->SetAnchors(FAnchors(0,0,1,1));S->SetOffsets(FMargin(0));S->SetZOrder(Order);}
void CopySlot(UCanvasPanelSlot* Dest,const UWidget* Source)
{
 if(const auto* S=Cast<UCanvasPanelSlot>(Source->Slot)){Dest->SetAnchors(S->GetAnchors());Dest->SetOffsets(S->GetOffsets());Dest->SetAlignment(S->GetAlignment());Dest->SetZOrder(S->GetZOrder());}
}
TArray<USvgCanvasNode*> Children(USvgCanvasNode* N)
{
 TArray<USvgCanvasNode*> Out;for(auto* W:N->GetAllChildren())if(auto* C=Cast<USvgCanvasNode>(W))Out.Add(C);return Out;
}
void Visit(USvgCanvasNode* N,TFunctionRef<void(USvgCanvasNode*)> F){F(N);for(auto* C:Children(N))Visit(C,F);}
FSlateFontInfo Font(const USvgCanvasNode* N)
{
 const auto St=Style(N);const bool Bold=Number(St,TEXT("fontWeight"),400)>=600;
 const FString Family=String(St,TEXT("fontFamily"),TEXT("Segoe UI"));
 // SVG/CSS sizes are pixels; Slate sizes are points at the 96-DPI UI baseline.
 const float Points=Number(St,TEXT("fontSize"),12)*N->DesignScale*72.f/96.f;
 static TMap<FString,TSharedPtr<FCompositeFont>> Fonts;
 const FString Key=Family+(Bold?TEXT("bold"):TEXT("regular"));auto& Composite=Fonts.FindOrAdd(Key);
 if(!Composite)
 {
  Composite=MakeShared<FCompositeFont>();const FString Dir=FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")),TEXT("Fonts"));
  const FString Latin=Family.Contains(TEXT("Consolas"))?(Bold?TEXT("consolab.ttf"):TEXT("consola.ttf")):(Bold?TEXT("seguisb.ttf"):TEXT("segoeui.ttf"));
  Composite->DefaultTypeface.Fonts.Emplace(TEXT("Regular"),FPaths::Combine(Dir,Latin),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  auto& Cjk=Composite->SubTypefaces.AddDefaulted_GetRef();Cjk.CharacterRanges.Add(FInt32Range(0x2000,0xffff));
  Cjk.Typeface.Fonts.Emplace(TEXT("Regular"),FPaths::Combine(Dir,Bold?TEXT("msyhbd.ttc"):TEXT("msyh.ttc")),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
 }
 FSlateFontInfo Result(Composite,Points,TEXT("Regular"));Result.LetterSpacing=FMath::RoundToInt(Number(St,TEXT("letterSpacing"),0)/FMath::Max(1.,Number(St,TEXT("fontSize"),12))*1000);return Result;
}
struct FList
{
 TArray<TWeakObjectPtr<USvgCanvasNode>> Templates,Rows;
 TWeakObjectPtr<UCanvasPanel> Canvas;
 TWeakObjectPtr<USizeBox> Extent;
 int32 Columns=1;double StepX=0,StepY=0,Width=0,Height=0,BaseX=0,BaseY=0;bool Grid=false;
};
struct FScreen
{
 TMap<FString,FSvgCanvasNodeState> Visible;
 TMap<FString,FList> Lists;
 TMap<FString,FLinearColor> QualityColors;
};
TMap<TWeakObjectPtr<USvgCanvasScreen>,FScreen> Screens;
FScreen& Data(USvgCanvasScreen* S)
{
 auto& D=Screens.FindOrAdd(S);if(D.Visible.IsEmpty()){
  for(const auto& Scene:S->SourceDocument->States)for(const auto& V:Scene.Value.Nodes)if(V.Visibility==TEXT("visible")&&V.Rect.Z>0&&!D.Visible.Contains(V.Id))D.Visible.Add(V.Id,V);
  TSharedPtr<FJsonObject> Project;const TSharedPtr<FJsonObject> *Presentation=nullptr,*Equipment=nullptr,*Colors=nullptr;
  if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S->SourceDocument->ProjectJson),Project)&&Project&&Project->TryGetObjectField(TEXT("presentation"),Presentation)&&(*Presentation)->TryGetObjectField(TEXT("equipmentStyle"),Equipment)&&(*Equipment)->TryGetObjectField(TEXT("colors"),Colors))
   for(const auto& Pair:(*Colors)->Values)D.QualityColors.Add(FString(Pair.Key.ToView()),FLinearColor::FromSRGBColor(FColor::FromHex(Pair.Value->AsString())));
 }
 return D;
}
void Hydrate(USvgCanvasScreen* S,USvgCanvasNode* N)
{
 const FSvgCanvasNodeState* V=nullptr;if(const auto* Scene=S->SourceDocument->States.Find(S->CurrentState))V=Scene->Nodes.FindByPredicate([&](const auto& Item){return Item.Id==N->SourceId&&Item.Visibility==TEXT("visible");});if(!V)V=Data(S).Visible.Find(N->SourceId);if(!V)return;
 N->DesignRect=V->Rect;N->DesignScale=V->LayoutScale;
 auto Raster=[&](const FString& K){const auto* R=S->SourceDocument->Rasters.Find(K);return R?*R:FSvgCanvasRaster();};
 N->Normal=Raster(V->Raster);N->Hovered=Raster(V->HoveredRaster);N->Pressed=Raster(V->PressedRaster);N->Disabled=Raster(V->DisabledRaster);
 N->ApplyRaster(N->Normal);
 if(auto* P=Cast<USvgCanvasNode>(N->GetParent()))N->ApplyLayout(P->DesignRect,V->PaintOrder);
}
USvgCanvasNode* Clone(USvgCanvasScreen* S,USvgCanvasNode* Source,UCanvasPanel* Parent)
{
 Hydrate(S,Source);auto* N=NewObject<UOpusActionNode>(S->WidgetTree);N->Screen=S;
 N->SourceId=Source->SourceId;N->SourceName=Source->SourceName;N->SourceType=Source->SourceType;N->SourcePropertiesJson=Source->SourcePropertiesJson;N->SourcePrefab=Source->SourcePrefab;
 N->SourceText=Source->SourceText;N->Binding=Source->Binding;N->ActionJson=Source->ActionJson;N->DesignRect=Source->DesignRect;N->DesignScale=Source->DesignScale;
 N->SourceAnchors=Source->SourceAnchors;N->SourceOffsets=Source->SourceOffsets;N->SourceAlignment=Source->SourceAlignment;
 N->Normal=Source->Normal;N->Hovered=Source->Hovered;N->Pressed=Source->Pressed;N->Disabled=Source->Disabled;
 N->AuthoredLayout=N->AuthoredAppearance=N->AuthoredVisibility=true;
 CopySlot(Parent->AddChildToCanvas(N),Source);
 if(auto* M=Cast<USvgCanvasMotionNode>(Source)){N->MotionAsset=M->MotionAsset;N->MotionEnabled=M->MotionEnabled;N->MotionDismissOnHover=M->MotionDismissOnHover;N->MotionTint=M->MotionTint;N->SoundEvents=M->SoundEvents;N->UiSoundEnabled=M->UiSoundEnabled;N->UiSoundVolume=M->UiSoundVolume;N->UiSoundPitch=M->UiSoundPitch;}
 if(Source->Surface){N->Surface=NewObject<UImage>(N,Source->Surface->GetClass());CopySlot(N->AddChildToCanvas(N->Surface),Source->Surface);N->ApplyRaster(N->Normal);}
 if(Source->Input){N->Input=NewObject<UButton>(N);N->Input->SetStyle(Source->Input->GetStyle());CopySlot(N->AddChildToCanvas(N->Input),Source->Input);N->BindInput();N->OnInvoked.AddDynamic(N,&UOpusActionNode::Relay);N->Input->OnHovered.AddDynamic(N,&UOpusActionNode::EnterItem);N->Input->OnUnhovered.AddDynamic(N,&UOpusActionNode::LeaveItem);}
 for(auto* C:Children(Source))Clone(S,C,N);
 return N;
}
bool HasRaster(USvgCanvasScreen* S,USvgCanvasNode* N){const auto* V=Data(S).Visible.Find(N->SourceId);return V&&V->Rect.Z>0;}
TArray<USvgCanvasNode*> Templates(USvgCanvasScreen* S,USvgCanvasNode* N,const FString& Binding)
{
 TArray<USvgCanvasNode*> Out;
 const auto Props=Properties(N);const TSharedPtr<FJsonObject>* Src=nullptr;bool NativeRows=false;if(Props->TryGetObjectField(TEXT("source"),Src))(*Src)->TryGetBoolField(TEXT("nativeRows"),NativeRows);
 for(auto* C:Children(N))
 {
  if(C->SourceId.EndsWith(TEXT("::native-scrollbar")))continue;
  bool Include=NativeRows||HasRaster(S,C);
  if(Binding==TEXT("ground-loot"))Include=C->Binding.StartsWith(TEXT("loot:"));
  if(Binding==TEXT("craft-materials"))Include=C->Binding.StartsWith(TEXT("material-choice:"))||C->Binding.StartsWith(TEXT("omen-choice:"));
  if(Binding==TEXT("inspector-materials"))Include=C->Binding.StartsWith(TEXT("material-choice:"));
  if(Binding==TEXT("inspector-omens"))Include=C->Binding.StartsWith(TEXT("omen-choice:"));
  if(Binding==TEXT("craft-affixes"))Include=C->Binding.StartsWith(TEXT("item-affix:"));
  if(Binding==TEXT("hunter-currencies"))Include=C->Binding.StartsWith(TEXT("currency-filter:"));
  if(Binding==TEXT("summon-list"))Include=C->SourceId.Contains(TEXT("::battle-summon-"));
  if(Include)Out.Add(C);
 }
 if(Binding==TEXT("shop-stock"))if(Out.Num()==1)Out=Children(Out[0]);
 return Out;
}
void Update(USvgCanvasNode* Root,const FOpusUIRow& R,const FString& Binding,bool Empty)
{
 const bool Inspectable=!Empty&&!R.Value.IsEmpty()&&(R.Op==TEXT("select")||R.Op==TEXT("craft-select"));
 auto* HoverRoot=Cast<UOpusActionNode>(Root);if(HoverRoot){HoverRoot->HoverId=Inspectable?R.Value:TEXT("");HoverRoot->PlacementKey=R.PlacementKey;HoverRoot->BagCell=R.BagCell;HoverRoot->EquipmentSlot=R.Slot;}
 Root->SetToolTipText(Inspectable?FText::GetEmpty():FText::FromString(R.Label+(R.Detail.IsEmpty()?TEXT(""):TEXT("\n")+R.Detail)));
 bool NameUsed=false,DetailUsed=false;TArray<FString> Lines;R.Detail.ParseIntoArrayLines(Lines);
 Visit(Root,[&](USvgCanvasNode* N)
 {
  const FString Id=N->SourceId;const FString B=N->Binding;
  if(B==TEXT("row:new"))if(auto* Motion=Cast<USvgCanvasMotionNode>(N))
  {
   const bool Show=!Empty&&R.NewItem;Motion->SetMotionEnabled(Show);
   Motion->SetVisibility(Show?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
   if(auto* Owner=Cast<UOpusActionNode>(Root);Owner&&Owner->Screen)if(const auto* Color=Data(Owner->Screen).QualityColors.Find(R.Rarity.IsEmpty()?TEXT("common"):R.Rarity))Motion->SetMotionTint(*Color);
  }
  if(auto* Live=Cast<UOpusActionNode>(N)){Live->HoverOwner=HoverRoot;Live->HoverEvents=Inspectable?UOpusUI::HoverEvents(Live->Screen):nullptr;}
  // Geometry and all button states come from the authored UICreator family.
  // Only the data key is selected here; no native redraw of equipment frames.
  const auto P=Properties(N);const TSharedPtr<FJsonObject>* Families=nullptr;
  if(P->TryGetObjectField(TEXT("dynamicBrushRasters"),Families))if(auto* Live=Cast<UOpusActionNode>(N);Live&&Live->Screen&&Live->Screen->SourceDocument)
  {
   FString Quality=Empty||R.Value.IsEmpty()?TEXT("empty"):R.Rarity.IsEmpty()?TEXT("common"):R.Rarity;
   if(R.Selected&&!Empty&&(*Families)->HasField(Quality+TEXT("-selected")))Quality+=TEXT("-selected");
   const TSharedPtr<FJsonObject>* Palette=nullptr;
   if((*Families)->TryGetObjectField(Quality,Palette))
   {
    const TSharedPtr<FJsonObject>* Origin=nullptr;FVector2D Delta=FVector2D::ZeroVector;
    if(P->TryGetObjectField(TEXT("dynamicBrushOrigin"),Origin))Delta=FVector2D(N->DesignRect.X-Number(*Origin,TEXT("x"),0),N->DesignRect.Y-Number(*Origin,TEXT("y"),0));
    auto Skin=[&](const TCHAR* State){FSvgCanvasRaster Out;if(const auto* Raster=Live->Screen->SourceDocument->Rasters.Find(String(*Palette,State,TEXT("")))){Out=*Raster;Out.PixelOrigin+=Delta;}return Out;};
    N->Normal=Skin(TEXT("normal"));if(N->Input){N->Hovered=Skin(TEXT("hovered"));N->Pressed=Skin(TEXT("pressed"));N->Disabled=Skin(TEXT("disabled"));}
    N->AuthoredAppearance=true;N->ApplyRaster(N->Normal);
   }
  }
  if(N->Input){Action(N,Empty?TEXT("none"):R.Op,R.Value);N->Input->SetIsEnabled(!Empty&&R.Enabled);}
  if(N->SourceType==TEXT("TextBlock"))
  {
   FString V;bool Use=true;
   if(B==TEXT("choice-name")||Id.Contains(TEXT("-name"))||Id.Contains(TEXT("_name_")))V=R.Label;
   else if(B==TEXT("choice-source"))V=R.Detail;
   else if(B==TEXT("choice-equipped"))V=R.Equipped?TEXT("已装配"):TEXT("");
   else if(Id.Contains(TEXT("-buy-label")))V=R.Badge;
   else if(Id.Contains(TEXT("-glyph-")))V=R.Label.Left(1);
   else if(Id.EndsWith(TEXT("-tier")))V=R.Tier>0?FString::Printf(TEXT("T%d"),R.Tier):TEXT("");
   else if(Id.EndsWith(TEXT("-mark")))V=R.Equipped?TEXT("已配"):TEXT("");
   else if(Id.EndsWith(TEXT("-count")))V=R.Count>1?FString::FromInt(R.Count):TEXT("");
   else if(Id.Contains(TEXT("-material-n-"))||Id.Contains(TEXT("materials-count-")))V=FString::Printf(TEXT("持有 %d"),R.Count);
   else if(Id.EndsWith(TEXT("-type"))||Id.EndsWith(TEXT("-kind")))V=R.Meta;
   else if(Id.Contains(TEXT("-sub-"))||Id.EndsWith(TEXT("-desc")))V=R.Detail;
   else if(Id.Contains(TEXT("-effect-"))){const int32 Line=FCString::Atoi(*Id.RightChop(Id.Find(TEXT("-effect-"))+8));if(Lines.IsValidIndex(Line))V=Lines[Line];DetailUsed=true;}
   else if(Id.EndsWith(TEXT("-merged")))V=TEXT("");
   else if(Id.Contains(TEXT("chevron"))||Id.Contains(TEXT("open-")))Use=false;
   else if(!NameUsed){V=R.Label;NameUsed=true;}else if(!DetailUsed){V=R.Detail;DetailUsed=true;}
   if(Use)Text(N,Empty?TEXT(""):V);
   if(Id.EndsWith(TEXT("-tier"))||Id.EndsWith(TEXT("-mark"))||Id.EndsWith(TEXT("-count"))){N->AuthoredVisibility=true;N->SetVisibility(!Empty&&!V.TrimStartAndEnd().IsEmpty()?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);}
   if(Binding==TEXT("gear-list")&&Id.EndsWith(TEXT("-name")))if(auto* Live=Cast<UOpusActionNode>(N);Live&&Live->Screen)
    if(const auto* Color=Data(Live->Screen).QualityColors.Find(R.Rarity.IsEmpty()?TEXT("common"):R.Rarity))
     for(auto* W:N->GetAllChildren())if(auto* Fit=Cast<UScaleBox>(W))if(auto* Label=Cast<UTextBlock>(Fit->GetChildAt(0)))Label->SetColorAndOpacity(FSlateColor(*Color));
  }
  if(N->SourceType==TEXT("Svg")&&B!=TEXT("equipment-icon-frame"))if(B.Contains(TEXT("icon"))||Id.Contains(TEXT("-icon"))||Id.Contains(TEXT("_icon_")))Image(N,Empty?TEXT(""):R.Icon);
 });
}
}
void Text(USvgCanvasNode* N,const FString& Value)
{
 UTextBlock* Text=nullptr;for(auto* W:N->GetAllChildren())if(W->GetFName()==TEXT("OpusLiveTextFit"))if(auto* Fit=Cast<UScaleBox>(W))Text=Cast<UTextBlock>(Fit->GetChildAt(0));
 if(!Text)
 {
  const auto St=Style(N);
  // Dynamic text replaces only the glyphs. Preserve its UICreator-authored
  // background (e.g. the stack badge), which was baked into the original PNG.
  const FString Background=String(St,TEXT("fill"),TEXT("transparent"));
  if(Background!=TEXT("transparent")&&Background!=TEXT("none"))
  {
   auto* Back=NewObject<UImage>(N,TEXT("OpusLiveTextBackground"));Back->SetVisibility(ESlateVisibility::HitTestInvisible);
   Back->SetBrush(FSlateRoundedBoxBrush(FLinearColor::FromSRGBColor(FColor::FromHex(Background)),float(Number(St,TEXT("radius"),0)*N->DesignScale)));
   Fill(N->AddChildToCanvas(Back),10);
  }
  auto* Fit=NewObject<UScaleBox>(N,TEXT("OpusLiveTextFit"));Fit->SetStretch(EStretch::ScaleToFit);Fit->SetStretchDirection(EStretchDirection::DownOnly);Fit->SetVisibility(ESlateVisibility::HitTestInvisible);
  auto* Slot=N->AddChildToCanvas(Fit);Fill(Slot);const double Pad=Number(St,TEXT("textPadding"),0)*N->DesignScale,PadX=Number(St,TEXT("textPaddingX"),Pad/N->DesignScale)*N->DesignScale;
  Slot->SetOffsets(FMargin(PadX,Pad,PadX,Pad));
  Text=NewObject<UTextBlock>(Fit);Text->SetFont(Font(N));Text->SetLineHeightPercentage(Number(St,TEXT("lineHeight"),Number(St,TEXT("fontSize"),12)*1.4)/(Number(St,TEXT("fontSize"),12)*1.4));Text->SetVisibility(ESlateVisibility::HitTestInvisible);Text->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor::FromHex(String(St,TEXT("color"),TEXT("#e5edf6"))))));
  const FString Align=String(St,TEXT("textAlign"),TEXT("left")),Vertical=String(St,TEXT("verticalAlign"),TEXT("center"));const bool Wrap=Wrapped(N);
  // UICreator supplies an explicit wrap width. AutoWrap reads a zero-width ScaleBox prepass and can leave short labels split across lines.
  Text->SetAutoWrapText(false);if(Wrap)Text->SetWrapTextAt(FMath::Max(1.,N->DesignRect.Z-PadX*2));Text->SetJustification(Align==TEXT("right")?ETextJustify::Right:Align==TEXT("center")?ETextJustify::Center:ETextJustify::Left);
  auto* Inner=Cast<UScaleBoxSlot>(Fit->AddChild(Text));Inner->SetHorizontalAlignment(Align==TEXT("right")?HAlign_Right:Align==TEXT("center")?HAlign_Center:HAlign_Left);Inner->SetVerticalAlignment(Vertical==TEXT("top")?VAlign_Top:Vertical==TEXT("bottom")?VAlign_Bottom:VAlign_Center);
 }
 if(N->SourceId.EndsWith(TEXT("::battle-gear-name"))){auto St=Style(N);if(Value==TEXT("空槽"))if(auto* Live=Cast<UOpusActionNode>(N))if(Live->Screen)if(auto* Empty=Live->Screen->FindSourceNode(TEXT("battle_live_3-1::battle-gear-name")))St=Style(Empty);Text->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor::FromHex(String(St,TEXT("color"),TEXT("#8798ad"))))));}
 const bool Wrap=Wrapped(N);const auto CurrentStyle=Style(N);// UICreator supplies an explicit wrap width. AutoWrap reads a zero-width ScaleBox prepass and can leave short labels split across lines.
  Text->SetAutoWrapText(false);
 if(Wrap)Text->SetWrapTextAt(FMath::Max(1.,N->DesignRect.Z-2*Number(CurrentStyle,TEXT("textPaddingX"),0)*N->DesignScale));
 N->AuthoredAppearance=true;if(N->Surface)N->Surface->SetVisibility(ESlateVisibility::Collapsed);if(Text->GetText().ToString()!=Value)Text->SetText(FText::FromString(Value));
 bool FitBackground=false;CurrentStyle->TryGetBoolField(TEXT("textBackgroundFit"),FitBackground);
 if(FitBackground)for(auto* W:N->GetAllChildren())if(W->GetFName()==TEXT("OpusLiveTextBackground"))
 {
  const bool HasText=!Value.TrimStartAndEnd().IsEmpty();W->SetVisibility(HasText?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
  if(!HasText)continue;
  Text->ForceLayoutPrepass();const FVector2D Desired=Text->GetDesiredSize();
  const double Pad=Number(CurrentStyle,TEXT("textPadding"),0)*N->DesignScale,PadX=Number(CurrentStyle,TEXT("textPaddingX"),Pad/N->DesignScale)*N->DesignScale;
  const double Scale=FMath::Min3(1.,FMath::Max(0.,N->DesignRect.Z-2*PadX)/FMath::Max(1.,Desired.X),FMath::Max(0.,N->DesignRect.W-2*Pad)/FMath::Max(1.,Desired.Y));
  const double Width=FMath::Min(N->DesignRect.Z,FMath::CeilToDouble(Desired.X*Scale)+2*PadX);
  const FString Align=String(CurrentStyle,TEXT("textAlign"),TEXT("left"));
  const double X=Align==TEXT("right")?N->DesignRect.Z-Width:Align==TEXT("center")?(N->DesignRect.Z-Width)*.5:0;
  if(auto* BackSlot=Cast<UCanvasPanelSlot>(W->Slot)){BackSlot->SetAnchors(FAnchors(0,0));BackSlot->SetPosition(FVector2D(X,0));BackSlot->SetSize(FVector2D(Width,N->DesignRect.W));}
 }
}
USvgCanvasNode* DuplicateTree(USvgCanvasScreen* S,USvgCanvasNode* Source,UCanvasPanel* Parent){return Clone(S,Source,Parent);}
void PlaceNode(USvgCanvasNode* N,float X,float Y,float W,float H)
{
 if(!N)return;auto* Parent=Cast<USvgCanvasNode>(N->GetParent());const FVector4 Before=N->DesignRect;
 const FVector2D At((Parent?Parent->DesignRect.X:0)+X,(Parent?Parent->DesignRect.Y:0)+Y),Delta=At-FVector2D(Before.X,Before.Y);
 // Brush origins belong to the exported canvas. Rebase every descendant when
 // its owner moves; hover/press must use the same local artwork as normal.
 Visit(N,[&](auto* C){C->DesignRect.X+=Delta.X;C->DesignRect.Y+=Delta.Y;for(auto* R:{&C->Normal,&C->Hovered,&C->Pressed,&C->Disabled})R->PixelOrigin+=Delta;});
 const FVector2D Scale(Before.Z>0?W/Before.Z:1,Before.W>0?H/Before.W:1);
 for(auto* R:{&N->Normal,&N->Hovered,&N->Pressed,&N->Disabled})if(R->Texture){R->PixelOrigin=At+(R->PixelOrigin-At)*Scale;R->PixelSize*=Scale;}
 N->DesignRect=FVector4(At.X,At.Y,W,H);N->AuthoredLayout=true;
 if(auto* Slot=Cast<UCanvasPanelSlot>(N->Slot)){Slot->SetPosition(FVector2D(X,Y));Slot->SetSize(FVector2D(W,H));}
}
float TextHeight(USvgCanvasNode* N,const FString& Value,float Width,float Extra)
{
 Width=FMath::Max(1.f,Width-float(2*Number(Style(N),TEXT("textPaddingX"),Number(Style(N),TEXT("textPadding"),0))*N->DesignScale));
 const auto F=Font(N);const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 int32 Lines=1;FString Current;for(TCHAR Ch:Value){if(Ch=='\n'){Lines++;Current.Empty();continue;}if(!Current.IsEmpty()&&Measure->Measure(Current+FString::Chr(Ch),F).X>Width){Lines++;Current.Empty();}Current+=Ch;}
 return FMath::CeilToFloat(Lines*Number(Style(N),TEXT("lineHeight"),Number(Style(N),TEXT("fontSize"),11)*1.4))+Extra;
}
void Image(USvgCanvasNode* N,const FString& Path)
{
 const auto St=Style(N);bool EquipmentAlpha=false;St->TryGetBoolField(TEXT("equipmentAlpha"),EquipmentAlpha);
 FString Artwork=Path;FPaths::NormalizeFilename(Artwork);
 if(EquipmentAlpha&&Artwork.Contains(TEXT("/OpusIcons/equipment/")))
 {
  const FString Masked=FPaths::Combine(FPaths::ProjectContentDir(),TEXT("OpusIconsMasked/equipment"),FPaths::GetCleanFilename(Artwork));
  if(FPaths::FileExists(Masked))Artwork=Masked;
 }
 static TMap<FString,TWeakObjectPtr<UTexture2D>> Cache;UTexture2D* Texture=nullptr;if(auto* C=Cache.Find(Artwork))Texture=C->Get();
 if(!Texture&&!Artwork.IsEmpty()){Texture=FImageUtils::ImportFileAsTexture2D(Artwork);Cache.Add(Artwork,Texture);}
 UImage* Image=nullptr;for(auto* W:N->GetAllChildren())if(W->GetFName()==TEXT("OpusLiveImage"))Image=Cast<UImage>(W);
 if(!Image){Image=NewObject<UImage>(N,TEXT("OpusLiveImage"));Fill(N->AddChildToCanvas(Image));}
 Image->SetBrushFromTexture(Texture);
 // Dynamic artwork follows the same per-image clip as UICreator's SVG/PNG.
 // Never clip the parent slot: its sibling selection glow extends beyond it.
 bool Clip=false;St->TryGetBoolField(TEXT("clip"),Clip);
 const float Radius=FMath::Min(float(Number(St,TEXT("radius"),0)*N->DesignScale),float(FMath::Min(N->DesignRect.Z,N->DesignRect.W)*.5));
 FSlateBrush Brush=Image->GetBrush();Brush.DrawAs=Clip&&Radius>0?ESlateBrushDrawType::RoundedBox:ESlateBrushDrawType::Image;Brush.OutlineSettings=FSlateBrushOutlineSettings(Clip&&Radius>0?Radius:0);Image->SetBrush(Brush);
 Image->SetVisibility(Texture?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);N->AuthoredAppearance=true;if(N->Surface)N->Surface->SetVisibility(ESlateVisibility::Collapsed);
}
void Progress(USvgCanvasNode* N,float Value)
{
 UCanvasPanel* Crop=nullptr;for(auto* W:N->GetAllChildren())if(W->GetFName()==TEXT("OpusProgressClip"))Crop=Cast<UCanvasPanel>(W);
 if(!Crop)
 {
  // Keep the SVG image's NoGamma/premultiplied-alpha renderer and rounded ends.
  const auto St=Style(N);const FColor Track=FColor::FromHex(String(St,TEXT("fill"),TEXT("#18232d"))),FillColor=FColor::FromHex(String(St,TEXT("color"),TEXT("#54df9d")));
  auto* Empty=NewObject<USvgCanvasImage>(N);Empty->SetBrush(N->Surface->GetBrush());Empty->SetColorAndOpacity(FLinearColor(float(Track.R)/FMath::Max(1,int(FillColor.R)),float(Track.G)/FMath::Max(1,int(FillColor.G)),float(Track.B)/FMath::Max(1,int(FillColor.B)),1));Empty->SetVisibility(ESlateVisibility::HitTestInvisible);Fill(N->AddChildToCanvas(Empty),29);
  Crop=NewObject<UCanvasPanel>(N,TEXT("OpusProgressClip"));Crop->SetClipping(EWidgetClipping::ClipToBoundsAlways);Crop->SetVisibility(ESlateVisibility::HitTestInvisible);auto* ClipSlot=N->AddChildToCanvas(Crop);ClipSlot->SetPosition(FVector2D::ZeroVector);ClipSlot->SetZOrder(30);
  auto* Image=NewObject<USvgCanvasImage>(Crop);Image->SetBrush(N->Surface->GetBrush());Image->SetVisibility(ESlateVisibility::HitTestInvisible);auto* ImageSlot=Crop->AddChildToCanvas(Image);ImageSlot->SetPosition(FVector2D::ZeroVector);ImageSlot->SetSize(FVector2D(N->DesignRect.Z,N->DesignRect.W));
 }
 N->AuthoredAppearance=true;if(N->Surface)N->Surface->SetVisibility(ESlateVisibility::Collapsed);Cast<UCanvasPanelSlot>(Crop->Slot)->SetSize(FVector2D(N->DesignRect.Z*FMath::Clamp(Value,0.f,1.f),N->DesignRect.W));
}
void Action(USvgCanvasNode* N,const FString& Op,const FString& Value)
{
 auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("type"),TEXT("opus"));A->SetStringField(TEXT("op"),Op);A->SetStringField(TEXT("value"),Value);N->ActionJson.Empty();FJsonSerializer::Serialize(A,TJsonWriterFactory<>::Create(&N->ActionJson));
}
void Rows(USvgCanvasScreen* S,USvgCanvasNode* N,const FString& Binding,const TArray<FOpusUIRow>& Values)
{
 if(Binding==TEXT("character-stats")){for(const auto& V:Values)if(auto* Field=S->FindSourceNode(V.Value))Text(Field,V.Detail);return;}
 auto& L=Data(S).Lists.FindOrAdd(N->GetPathName()+TEXT(":")+Binding);
 if(!L.Canvas.IsValid())
 {
  auto Source=Templates(S,N,Binding);if(Source.IsEmpty())return;
  Hydrate(S,N);L.Grid=Binding==TEXT("bag-grid")||Binding==TEXT("character-grid")||Binding==TEXT("gear-list");
  const TSharedPtr<FJsonObject>* RowSource=nullptr;const TSharedPtr<FJsonObject>* Grid=nullptr;const auto RowProps=Properties(N);if(RowProps->TryGetObjectField(TEXT("source"),RowSource)&&(*RowSource)->TryGetObjectField(TEXT("grid"),Grid))L.Grid=true;
  for(auto* T:Source){Hydrate(S,T);L.Templates.Add(T);}
  const auto* First=Source[0];L.Width=First->DesignRect.Z;L.Height=First->DesignRect.W;L.StepX=L.Width;L.StepY=L.Height;
  double Gap=0;auto P=Properties(N);const TSharedPtr<FJsonObject>* Layout=nullptr;if(P->TryGetObjectField(TEXT("layout"),Layout))Gap=Number(*Layout,TEXT("gap"),0)*N->DesignScale;L.StepY+=Gap;
  L.BaseX=First->DesignRect.X-N->DesignRect.X;L.BaseY=First->DesignRect.Y-N->DesignRect.Y;
  L.Columns=1;for(int I=1;I<Source.Num();I++){const double Dx=Source[I]->DesignRect.X-First->DesignRect.X,Dy=Source[I]->DesignRect.Y-First->DesignRect.Y;if(FMath::Abs(Dy)<1&&Dx>1){L.Columns++;if(L.StepX==L.Width)L.StepX=Dx;}else if(Dy>1){L.StepY=Dy;break;}}
  auto* Canvas=NewObject<UCanvasPanel>(N);L.Canvas=Canvas;
  const bool MaterialRegion=Binding==TEXT("craft-materials")||Binding==TEXT("inspector-materials")||Binding==TEXT("inspector-omens");
  if(N->SourceType==TEXT("ScrollBox")||MaterialRegion||Binding==TEXT("character-grid"))
  {
   auto* Scroll=NewObject<UScrollBox>(N);Scroll->SetScrollbarThickness(FVector2D(3,3));Scroll->SetScrollbarPadding(FMargin(0));Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);Fill(N->AddChildToCanvas(Scroll),30);
   double Width=N->DesignRect.Z;
   if(MaterialRegion){double Right=First->DesignRect.X,Bottom=First->DesignRect.Y;for(auto* T:Source){Right=FMath::Max(Right,T->DesignRect.X+T->DesignRect.Z);Bottom=FMath::Max(Bottom,T->DesignRect.Y+T->DesignRect.W);}Width=Right-First->DesignRect.X;auto* Slot=Cast<UCanvasPanelSlot>(Scroll->Slot);Slot->SetAnchors(FAnchors(0,0));Slot->SetPosition(FVector2D(L.BaseX,L.BaseY));Slot->SetSize(FVector2D(Width,Bottom-First->DesignRect.Y));L.BaseX=L.BaseY=0;}
   auto* Size=NewObject<USizeBox>(Scroll);L.Extent=Size;Size->SetWidthOverride(Width-4);Size->AddChild(Canvas);Scroll->AddChild(Size);
  }
  else Fill(N->AddChildToCanvas(Canvas),30);
  for(auto* T:Source){T->AuthoredVisibility=true;T->SetVisibility(ESlateVisibility::Collapsed);}
  for(auto* C:Children(N))if(C->SourceId.EndsWith(TEXT("::native-scrollbar"))){C->AuthoredVisibility=true;C->SetVisibility(ESlateVisibility::Collapsed);}
 }
 const bool EffectCards=Binding==TEXT("character-traits-list");
 const int32 Required=L.Grid?FMath::Max(L.Templates.Num(),Values.Num()):Values.Num();
 while(L.Rows.Num()<Required)
 {
  const int32 I=L.Rows.Num();auto* LayoutTemplate=L.Templates[FMath::Min(I,L.Templates.Num()-1)].Get();auto* Template=L.Grid&&Binding!=TEXT("gear-list")?L.Templates[0].Get():LayoutTemplate;
  // Later fixture cards were clipped by the authored ScrollBox during export.
  // Reuse the fully exported skill/equipment prototypes, then project real data.
  if(EffectCards)Template=L.Templates[FMath::Min(I==0?0:1,L.Templates.Num()-1)].Get();
  // Empty cells must retain the full icon/text geometry. A later drop can fill
  // them in place; collapsed empty prototypes export those children at 0x0.
  // Update clears their artwork and selects the authored empty brush instead.
  if(!Template)break;auto* Row=Clone(S,Template,L.Canvas.Get());
  auto* Slot=Cast<UCanvasPanelSlot>(Row->Slot);Slot->SetAnchors(FAnchors(0,0));Slot->SetAlignment(FVector2D::ZeroVector);
  FVector2D P(I%L.Columns*L.StepX,I/L.Columns*L.StepY);if(L.Grid&&I<L.Templates.Num()){const auto* First=L.Templates[0].Get();P=FVector2D(LayoutTemplate->DesignRect.X-First->DesignRect.X,LayoutTemplate->DesignRect.Y-First->DesignRect.Y);}
  Slot->SetPosition(P+FVector2D(L.BaseX,L.BaseY));Slot->SetSize(FVector2D(L.Width,L.Height));Slot->SetZOrder(I);L.Rows.Add(Row);
 }
 float EffectY=0;
 for(int32 I=0;I<L.Rows.Num();I++)if(auto* Row=L.Rows[I].Get())
 {
  // Selection/hover skins are exported per scene; keep those skins when the
  // same data row is shown in another skill slot or modal state.
  Visit(Row,[&](USvgCanvasNode* C){if(C->Input)if(auto* Original=S->FindSourceNode(C->SourceId))
  {
   Hydrate(S,Original);C->Normal=Original->Normal;C->Hovered=Original->Hovered;C->Pressed=Original->Pressed;C->Disabled=Original->Disabled;
   if(Binding==TEXT("picker-list"))if(const auto* Scene=S->SourceDocument->States.Find(Values.IsValidIndex(I)&&Values[I].Equipped?TEXT("picker-1"):TEXT("picker-2")))
    if(const auto* V=Scene->Nodes.FindByPredicate([&](const auto& Item){return Item.Id==C->SourceId;}))if(const auto* Raster=S->SourceDocument->Rasters.Find(V->Raster))C->Normal=*Raster;
   C->ApplyRaster(C->Normal);
  }});
  const bool Present=Values.IsValidIndex(I);Row->SetVisibility(Present||(L.Grid&&I<Required)?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
  if(EffectCards&&Present)
  {
   USvgCanvasNode *Body=nullptr,*Background=nullptr;
   Visit(Row,[&](auto* C){const auto Id=C->SourceId;
    if(Id.EndsWith(TEXT("-name")))Text(C,Values[I].Label);
    else if(Id.EndsWith(TEXT("-kind")))Text(C,Values[I].Meta);
    else if(Id.EndsWith(TEXT("-icon")))Image(C,Values[I].Icon);
    else if(Id.EndsWith(TEXT("-effect-0")))Body=C;
    else if(Id.Contains(TEXT("-effect-"))||Id.Contains(TEXT("-line-"))||Id.EndsWith(TEXT("-merged")))C->SetVisibility(ESlateVisibility::Collapsed);
    if(Id.EndsWith(TEXT("-bg")))Background=C;
   });
   // The exported cards contain fixture-specific row counts. Render the real
   // description once and measure it; do not force all cards to the first height.
   const float BodyHeight=Body?TextHeight(Body,Values[I].Detail,L.Width-28,12):0;
   const float CardHeight=48+BodyHeight+8;
   PlaceNode(Row,L.BaseX,EffectY,L.Width,CardHeight);
   if(Body){Body->SetVisibility(ESlateVisibility::SelfHitTestInvisible);PlaceNode(Body,12,48,L.Width-28,BodyHeight);Text(Body,Values[I].Detail);}
   if(Background){PlaceNode(Background,0,0,L.Width,CardHeight);Background->ApplyRaster(Background->Normal);}
   EffectY+=CardHeight+14;
  }
  else if(Binding==TEXT("gear-list"))
  {
   const TCHAR* Slots[]={TEXT("weapon"),TEXT("offhand"),TEXT("helmet"),TEXT("chest"),TEXT("gloves"),TEXT("boots"),TEXT("belt"),TEXT("amulet"),TEXT("ring")};const TCHAR* Names[]={TEXT("武器"),TEXT("副手"),TEXT("头盔"),TEXT("胸甲"),TEXT("手套"),TEXT("靴子"),TEXT("腰带"),TEXT("项链"),TEXT("戒指")};
   const auto* Found=Values.FindByPredicate([&](const auto& R){return I<9&&R.Slot==Slots[I];});FOpusUIRow Gear=Found?*Found:FOpusUIRow();if(I<9){Gear.Meta=Names[I];Gear.Slot=Slots[I];}if(!Found)Gear.Label=TEXT("空槽");Update(Row,Gear,Binding,false);
  }
  else if(Present)Update(Row,Values[I],Binding,false);else if(L.Grid)Update(Row,FOpusUIRow(),Binding,true);
 }
 if(auto* E=L.Extent.Get())E->SetHeightOverride(EffectCards?FMath::Max(0.f,EffectY-14):Required?((Required-1)/L.Columns*L.StepY+L.Height):0);
}
void ArrangeRows(USvgCanvasScreen* S,USvgCanvasNode* N,const FString& Binding,const TArray<FVector4>& Rects,const TArray<float>& Angles)
{
 auto* L=Data(S).Lists.Find(N->GetPathName()+TEXT(":")+Binding);if(!L)return;
 for(int32 I=0;I<L->Rows.Num()&&I<Rects.Num();I++)if(auto* Row=L->Rows[I].Get()){
  const auto R=Rects[I];PlaceNode(Row,N->DesignRect.X+R.X,N->DesignRect.Y+R.Y,R.Z,R.W);
  if(auto* Slot=Cast<UCanvasPanelSlot>(Row->Slot))Slot->SetPosition(FVector2D(R.X,R.Y));
  Row->SetRenderTransformPivot(FVector2D(0,.5));Row->SetRenderTransformAngle(Angles.IsValidIndex(I)?Angles[I]:0);Row->ApplyRaster(Row->Normal);
 }
}
void Input(USvgCanvasNode* N,UEditableTextBox* E)
{
 const auto P=Properties(N);const TSharedPtr<FJsonObject>* Src=nullptr;if(P->TryGetObjectField(TEXT("source"),Src))E->SetHintText(FText::FromString(String(*Src,TEXT("placeholder"),TEXT(""))));
 auto* TextSource=N;for(auto* C:Children(N))if(C->SourceType==TEXT("TextBlock")){TextSource=C;C->AuthoredVisibility=true;C->SetVisibility(ESlateVisibility::Collapsed);}
 auto St=E->GetWidgetStyle();St.TextStyle.Font=Font(TextSource);St.ForegroundColor=FSlateColor(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("#cdd8e8"))));St.BackgroundImageNormal.DrawAs=St.BackgroundImageHovered.DrawAs=St.BackgroundImageFocused.DrawAs=St.BackgroundImageReadOnly.DrawAs=ESlateBrushDrawType::NoDrawType;St.Padding=FMargin(4,0);E->SetWidgetStyle(St);
}
}
