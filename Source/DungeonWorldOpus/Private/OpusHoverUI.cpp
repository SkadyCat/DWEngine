#include "OpusHoverUI.h"
#include "OpusSvgBinding.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "SvgCanvasImage.h"
namespace
{
TMap<TWeakObjectPtr<UOpusWorldScreen>,TArray<TWeakObjectPtr<USvgCanvasNode>>> Rows;
void Visit(USvgCanvasNode* Root,TFunctionRef<void(USvgCanvasNode*)> Callback)
{if(!Root)return;Callback(Root);for(auto* W:Root->GetAllChildren())if(auto* N=Cast<USvgCanvasNode>(W))Visit(N,Callback);}
USvgCanvasNode* FindIn(USvgCanvasNode* Root,const FString& Suffix)
{USvgCanvasNode* Found=nullptr;Visit(Root,[&](auto* N){if(N->SourceId.EndsWith(Suffix))Found=N;});return Found;}
USvgCanvasNode* Find(UOpusWorldScreen* S,const TCHAR* Suffix){return FindIn(Cast<USvgCanvasNode>(S->GetRootWidget()),Suffix);}
void SetNodeRect(USvgCanvasNode* N,float X,float Y,float W,float H)
{
 OpusSvg::PlaceNode(N,X,Y,W,H);
}
void Shown(UWidget* W,bool Yes){if(W)W->SetVisibility(Yes?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);}
void Text(UOpusWorldScreen* S,const TCHAR* Key,const FString& Value){if(auto* N=Find(S,Key))OpusSvg::Text(N,Value);}
void Hover(UOpusWorldScreen* S,USvgCanvasNode* Root,const FString& Id)
{
 auto* Owner=Cast<UOpusActionNode>(Root);if(!Owner)return;Owner->HoverId=Id;
 Visit(Root,[&](auto* N){if(auto* A=Cast<UOpusActionNode>(N)){A->HoverOwner=Owner;A->HoverEvents=UOpusUI::HoverEvents(S);}});
}
void Size(UOpusWorldScreen* S,float Height)
{
 S->TemplateSize.Y=Height;S->SetDesiredSizeInViewport(S->TemplateSize);if(auto* Root=Cast<USvgCanvasNode>(S->GetRootWidget()))Root->DesignRect.W=Height;
 auto* Frame=Find(S,TEXT("battle-item-hover-frame"));if(!Frame)Frame=Find(S,TEXT("inspector-detail-frame"));SetNodeRect(Frame,0,0,S->TemplateSize.X,Height);
 if(Frame&&Frame->Surface)
 {
  auto Brush=Frame->Surface->GetBrush();Brush.DrawAs=ESlateBrushDrawType::Box;Brush.Margin=FMargin(8.f/FMath::Max(1.f,float(Brush.ImageSize.X)),8.f/FMath::Max(1.f,float(Brush.ImageSize.Y)));Frame->Surface->SetBrush(Brush);
  auto* Slot=Cast<UCanvasPanelSlot>(Frame->Surface->Slot);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));
 }
 S->ForceLayoutPrepass();
}
}
void UOpusHoverUI::Populate(UOpusWorldScreen* S,FOpusEntry Item,const FString& Meta,const TArray<FOpusHoverLine>& Lines,bool Pinned,const TArray<FOpusEntry>& Materials,FOpusCraftState Craft,const FString& Currency,const FString& Omen,const FString& Feedback,bool ReadOnly)
{
 if(!S)return;auto& Live=Rows.FindOrAdd(S);for(auto W:Live)if(W.IsValid())W->RemoveFromParent();Live.Reset();
 Text(S,TEXT("battle-item-hover-name"),(Item.Tier>0&&Item.Category!=TEXT("map")?FString::Printf(TEXT("T%d · "),Item.Tier):TEXT(""))+Item.Name);
 Text(S,TEXT("battle-item-hover-meta"),Meta);Text(S,TEXT("battle-item-hover-hint"),Pinned?TEXT("已固定 · 点击空白处或 × 关闭"):TEXT("Ctrl 固定 · 移出物品关闭"));
 if(auto* Icon=Find(S,TEXT("battle-item-hover-icon")))OpusSvg::Image(Icon,Item.Icon);
 auto* Body=Find(S,TEXT("battle-item-hover-content"));auto* Normal=Find(S,TEXT("battle-item-hover-line-0"));auto* Muted=Find(S,TEXT("battle-item-hover-line-1"));auto* Base=Find(S,TEXT("battle-item-hover-base"));auto* Affix=Find(S,TEXT("battle-item-hover-affix-0"));
 auto* Bound=Find(S,TEXT("battle-item-hover-targets"));auto* Hint=Find(S,TEXT("battle-item-hover-reroll-hint"));
 if(!Body||!Normal||!Muted||!Affix)return;if(!Base)Base=Normal;
 for(auto* N:{Normal,Muted,Base,Affix,Bound,Hint})Shown(N,false);
 const float Width=Body->DesignRect.Z;float Cursor=0;
 auto Paragraph=[&](USvgCanvasNode* Source,const FString& Value)
 {
  if(!Source||Value.IsEmpty())return;auto* N=OpusSvg::DuplicateTree(S,Source,Body);Live.Add(N);Shown(N,true);
  const float Height=OpusSvg::TextHeight(N,Value,Width);SetNodeRect(N,0,Cursor,Width,Height);OpusSvg::Text(N,Value);Cursor+=Height;
 };
 for(int32 Ordinal=0;Ordinal<Lines.Num();Ordinal++)
 {
  const auto& L=Lines[Ordinal];
  if(L.Style!=2&&L.Style!=4)
  {
   Paragraph(L.Style==3?Base:L.Style==1?Muted:Normal,L.Text);
   if(Pinned&&Craft.Instance&&L.Style==1&&L.Text.Contains(TEXT("条随机词条")))
   {
    Paragraph(Bound,FString::Printf(TEXT("已选定洗炼 %d/3 条"),Craft.BoundCount));
    if(!Craft.EligibleAffixes.IsEmpty())Paragraph(Hint,TEXT("点击闪烁词条，用已激活灵石选定或洗炼"));
   }
   continue;
  }
  auto* N=OpusSvg::DuplicateTree(S,Affix,Body);Live.Add(N);Shown(N,true);
  auto* Value=FindIn(N,TEXT("-text"));auto* Tier=FindIn(N,TEXT("-tier"));auto* Grade=FindIn(N,TEXT("-grade"));auto* Eligible=FindIn(N,TEXT("-eligible"));
  const float Height=FMath::Max(32.f,OpusSvg::TextHeight(Value,L.Text,254,16));SetNodeRect(N,0,Cursor,Width,Height);
  Visit(N,[&](auto* C){if(C!=N&&C->SourceType==TEXT("CanvasPanel"))SetNodeRect(C,0,0,Width,Height);});
  SetNodeRect(Value,8,0,254,Height);SetNodeRect(Tier,302,0,30,Height);SetNodeRect(Grade,267,0,30,Height);SetNodeRect(Eligible,0,1,Width,Height-2);
  OpusSvg::Text(Value,L.Text);OpusSvg::Text(Tier,L.Style==4?TEXT("固定"):L.Tier>0?FString::Printf(TEXT("T%d"),L.Tier):TEXT(""));
  OpusSvg::Text(Grade,L.Grade==1?TEXT("下品"):L.Grade==2?TEXT("中品"):L.Grade==3?TEXT("上品"):TEXT(""));
  const bool CanReroll=Pinned&&!ReadOnly&&L.Style==2&&Craft.Blocked.IsEmpty()&&Craft.EligibleAffixes.Contains(L.Index);Shown(Eligible,CanReroll);
  const bool HasDescription=!L.Description.IsEmpty()&&L.Description!=L.Text;
  if(N->Input){OpusSvg::Action(N,L.Style==2?TEXT("hover-affix"):TEXT("hover-effect"),FString::FromInt(L.Style==2?L.Index:Ordinal));N->Input->SetIsEnabled(Pinned&&(CanReroll||HasDescription));}
  if(HasDescription)Hover(S,N,TEXT("line:")+FString::FromInt(Ordinal));Cursor+=Height;
 }
 SetNodeRect(Body,14,96,Width,Cursor);float End=96+Cursor;
 auto* Crafting=Find(S,TEXT("battle-item-hover-crafting"));Shown(Crafting,Pinned&&!ReadOnly&&Craft.Instance);
 if(Pinned&&!ReadOnly&&Craft.Instance)
 {
  auto MaterialGroup=[&](bool Omens,float Y)
  {
   const FString Kind=Omens?TEXT("omen"):TEXT("currency");auto* Group=Find(S,Omens?TEXT("battle-item-hover-omens"):TEXT("battle-item-hover-currencies"));
   auto* Prototype=Find(S,Omens?TEXT("battle-item-hover-omen-0"):TEXT("battle-item-hover-currency-0"));Shown(Prototype,false);
   TArray<FOpusEntry> Choices;for(const auto& E:Materials)if(E.Category==Kind&&E.Count>0)Choices.Add(E);
   Choices.StableSort([](const auto& A,const auto& B){return (A.CurrencyGrade>0?A.CurrencyGrade:4)<(B.CurrencyGrade>0?B.CurrencyGrade:4);});
   FString ActiveName;int32 Index=0;
   for(const auto& E:Choices)
   {
    auto* N=OpusSvg::DuplicateTree(S,Prototype,Group);Live.Add(N);Shown(N,true);SetNodeRect(N,(Index%6)*50,24+(Index/6)*50,44,44);
    Visit(N,[&](auto* C){if(C!=N&&C->SourceType==TEXT("CanvasPanel"))SetNodeRect(C,0,0,44,44);});
    OpusSvg::Image(FindIn(N,TEXT("-icon")),E.Icon);OpusSvg::Text(FindIn(N,TEXT("-count")),FString::FromInt(E.Count));
    OpusSvg::Text(FindIn(N,TEXT("-grade")),E.CurrencyGrade==1?TEXT("下"):E.CurrencyGrade==2?TEXT("中"):E.CurrencyGrade==3?TEXT("上"):TEXT(""));
    const bool Active=E.Id==(Omens?Omen:Currency);Shown(FindIn(N,TEXT("-active")),Active);Shown(FindIn(N,TEXT("-check")),Active);if(Active)ActiveName=E.Name;
    OpusSvg::Action(N,Omens?TEXT("hover-omen"):TEXT("hover-currency"),E.Id);Hover(S,N,Kind+TEXT(":")+E.Id);Index++;
   }
   if(Choices.IsEmpty())
   {
    auto* N=OpusSvg::DuplicateTree(S,Muted,Group);Live.Add(N);Shown(N,true);SetNodeRect(N,0,24,332,24);OpusSvg::Text(N,Omens?TEXT("暂无持有的预兆"):TEXT("暂无持有的通货"));
   }
   Text(S,Omens?TEXT("battle-item-hover-omen-active"):TEXT("battle-item-hover-currency-active"),ActiveName.IsEmpty()?(Omens?TEXT("可选 · 点击图标激活"):TEXT("点击图标激活")):ActiveName+TEXT(" · 已激活"));
   const float Height=Choices.IsEmpty()?50:24+FMath::DivideAndRoundUp(Choices.Num(),6)*50;SetNodeRect(Group,0,Y,332,Height);return Height;
  };
  const float CH=MaterialGroup(false,10),OH=MaterialGroup(true,20+CH);float CY=30+CH+OH;
  FString Note=Currency.IsEmpty()?FString():
   Craft.Operation==TEXT("roll_affixes")?FString::Printf(TEXT("每次消耗 1 个通货%s，重新随机生成 %d 条词缀，并清除洗炼选定记录；保留基础属性及固定传奇词条。词条 T1–T%d。"),Omen.IsEmpty()?TEXT(""):TEXT("和 1 个预兆"),Craft.Count,Item.Tier):
   Craft.Operation==TEXT("add_affixes")?FString::Printf(TEXT("每次消耗 1 个通货%s，保留原词条并随机增加 %d 条。词条 T1–T%d。"),Omen.IsEmpty()?TEXT(""):TEXT("和 1 个预兆"),Craft.Count,Item.Tier):
   Craft.Operation==TEXT("remove_affixes")?FString::Printf(TEXT("每次消耗 1 个通货，随机移除 %d 条随机词缀，保留基础属性及固定传奇词条；不搭配预兆。移除已选定词条时，该条及后续品级的洗炼绑定重置。"),Craft.Count):
   FString::Printf(TEXT("每次消耗 1 个通货%s。首次选定后，该条只用对应品级灵石洗炼；每件装备最多选定3条。词条 T1–T%d。"),Omen.IsEmpty()?TEXT(""):TEXT("和 1 个预兆"),Item.Tier);
  auto* NoteNode=Find(S,TEXT("battle-item-hover-craft-note"));Shown(NoteNode,!Note.IsEmpty());const float NH=Note.IsEmpty()?0:OpusSvg::TextHeight(NoteNode,Note,332);SetNodeRect(NoteNode,0,CY,332,NH);OpusSvg::Text(NoteNode,Note);CY+=NH;
  auto* Blocked=Find(S,TEXT("battle-item-hover-blocked"));Shown(Blocked,!Craft.Blocked.IsEmpty());const float BH=OpusSvg::TextHeight(Blocked,Craft.Blocked,332);SetNodeRect(Blocked,0,CY,332,BH);OpusSvg::Text(Blocked,Craft.Blocked);if(!Craft.Blocked.IsEmpty())CY+=BH;
  const bool Whole=Craft.Operation==TEXT("add_affixes")||Craft.Operation==TEXT("roll_affixes")||Craft.Operation==TEXT("remove_affixes");auto* WholeNode=Find(S,TEXT("battle-item-hover-whole"));Shown(WholeNode,Whole);
  SetNodeRect(WholeNode,0,CY,332,30);Text(S,TEXT("battle-item-hover-whole-label"),FString::Printf(TEXT("%s %d 条词缀"),Craft.Operation==TEXT("remove_affixes")?TEXT("随机移除"):Craft.Operation==TEXT("add_affixes")?TEXT("随机增加"):TEXT("重新随机生成"),Craft.Count));if(WholeNode&&WholeNode->Input)WholeNode->Input->SetIsEnabled(Craft.Blocked.IsEmpty());if(Whole)CY+=40;
  SetNodeRect(Crafting,14,End+10,332,CY);End+=CY+20;
 }
 auto* Identify=Find(S,TEXT("battle-item-hover-identify"));const bool IdentifyVisible=Pinned&&!ReadOnly&&Item.Kind==TEXT("equipment")&&!Craft.Instance;Shown(Identify,IdentifyVisible);SetNodeRect(Identify,14,End+6,110,30);if(IdentifyVisible)End+=42;
 auto* FeedbackNode=Find(S,TEXT("battle-item-hover-feedback"));Shown(FeedbackNode,!Feedback.IsEmpty());const float FH=OpusSvg::TextHeight(FeedbackNode,Feedback,332);SetNodeRect(FeedbackNode,14,End,332,FH);OpusSvg::Text(FeedbackNode,Feedback);if(!Feedback.IsEmpty())End+=FH+4;
 auto* Actions=Find(S,TEXT("battle-item-hover-actions"));const bool MapItem=Item.Category==TEXT("map");const bool Footer=Pinned&&!ReadOnly&&((Item.Kind==TEXT("equipment")&&Craft.Instance)||MapItem);Shown(Actions,Footer);SetNodeRect(Actions,14,End+8,332,42);if(Footer)End+=58;
 if(auto* Button=Find(S,TEXT("battle-item-hover-equip")))OpusSvg::Action(Button,MapItem?TEXT("hover-use-map"):TEXT("hover-equip"),Item.Id);
 Text(S,TEXT("battle-item-hover-equip-label"),MapItem?TEXT("使用地图"):Item.Equipped?TEXT("卸下"):TEXT("装备"));Shown(Find(S,TEXT("battle-item-hover-close")),Pinned);
 Size(S,End+14);S->SetVisibility(Pinned?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::HitTestInvisible);
}
void UOpusHoverUI::PopulateDetail(UOpusWorldScreen* S,const FString& Title,const FString& Meta,const FString& Description,const FString& Icon,const FString& Hint,bool Pinned)
{
 if(!S)return;Text(S,TEXT("inspector-detail-title"),Title);Text(S,TEXT("inspector-detail-meta"),Meta);Text(S,TEXT("inspector-detail-hint"),Hint);
 auto* Image=Find(S,TEXT("inspector-detail-icon"));Shown(Image,!Icon.IsEmpty());if(!Icon.IsEmpty())OpusSvg::Image(Image,Icon);
 auto* Body=Find(S,TEXT("inspector-detail-description"));const float H=OpusSvg::TextHeight(Body,Description,272);SetNodeRect(Body,14,76,272,H);OpusSvg::Text(Body,Description);
 SetNodeRect(Find(S,TEXT("inspector-detail-hint")),14,84+H,272,38);Shown(Find(S,TEXT("inspector-detail-close")),Pinned);Size(S,132+H);S->SetVisibility(Pinned?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::HitTestInvisible);
}
FVector4 UOpusHoverUI::AnchorRect(UWidget* Anchor,APlayerController* P)
{
 if(!Anchor||!P)return FVector4();const auto View=UWidgetLayoutLibrary::GetViewportWidgetGeometry(P);const auto G=Anchor->GetCachedGeometry();
 FVector2D At=View.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector)),End=View.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));return FVector4(At.X,At.Y,End.X,End.Y);
}
FVector2D UOpusHoverUI::ViewSize(APlayerController* P){return P?UWidgetLayoutLibrary::GetViewportSize(P)/UWidgetLayoutLibrary::GetViewportScale(P):FVector2D::ZeroVector;}
void UOpusHoverUI::Place(UOpusWorldScreen* S,FVector2D P,float Scale){if(S){S->SetRenderTransformPivot(FVector2D::ZeroVector);S->SetRenderScale(FVector2D(Scale));S->SetPositionInViewport(P,false);}}
