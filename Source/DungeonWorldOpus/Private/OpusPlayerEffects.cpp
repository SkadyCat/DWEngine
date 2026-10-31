#include "OpusPlayerEffects.h"
#include "DWRuleSession.h"
#include "SvgCanvasImage.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "Misc/Paths.h"
using namespace DWRules;

namespace {
FString Icon(const FString& Kind,const FString& Id){return FPaths::ProjectContentDir()/TEXT("OpusIcons")/Kind/(Id+TEXT(".png"));}
FLinearColor Tint(const TCHAR* Hex){return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));}
void Source(FOpusPlayerEffect& E,FDWRuleSession& S,int32 Plan)
{
 if(Plan<0)return;const auto Active=S.Host.World.GetPlan(Plan).ActiveId;
 for(const auto& Skill:S.Skills)if(Skill.Plan>=0&&(Skill.Plan==Plan||S.Host.World.GetPlan(Skill.Plan).ActiveId==Active))
 {const auto* Build=S.Host.Library.Builds.Find(Skill.Id);E.Source=TEXT("技能 · ")+(Build?Build->DisplayName:Skill.Id.ToString());E.Icon=Icon(TEXT("skill"),Skill.Id.ToString());return;}
}
int32 SelfSource(FDWRuleSession& S,TFunctionRef<bool(const FDWActionDef&,const FDWCompiledPlan&)> Match)
{
 int32 Found=INDEX_NONE;for(const auto& Skill:S.Skills)if(Skill.Plan>=0){const auto& Plan=S.Host.World.GetPlan(Skill.Plan);if(Plan.Actions.ContainsByPredicate([&](const auto& A){return Match(A,Plan);})){if(Found!=INDEX_NONE)return INDEX_NONE;Found=Skill.Plan;}}return Found;
}
}
double UOpusPlayerEffects::Clock(UDWRuleWorldSubsystem* R){return R&&R->PlayerSession()?R->PlayerSession()->Host.World.GetClock():0;}
TArray<FOpusPlayerEffect> UOpusPlayerEffects::Snapshot(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusPlayerEffect> Out;if(!R||!R->PlayerSession())return Out;auto& S=*R->PlayerSession();const auto& W=S.Host.World;const double Now=W.GetClock();
 const int32 Owner=S.Combat.Hero.Owner;TArray<FDWStatus> Statuses;W.LiveStatusSnapshot(S.TargetId,Statuses);
 for(const auto& D:S.Host.Library.Statuses)if(const auto* Live=Statuses.FindByPredicate([&](const auto& V){return V.Key==D.Key;}))
 {
  FOpusPlayerEffect E;E.Id=TEXT("status:")+D.Id.ToString();E.Name=D.DisplayName;E.Description=D.Description;E.Source=D.Category==TEXT("buff")?TEXT("状态 · 临时增益"):TEXT("状态 · 异常状态");
  static const TSet<FName> Symbols={TEXT("chill"),TEXT("freeze"),TEXT("shock"),TEXT("ignite"),TEXT("bleed"),TEXT("slow"),TEXT("poison"),TEXT("erosion"),TEXT("stun")};
  E.Icon=Icon(TEXT("status"),Symbols.Contains(D.Id)?D.Id.ToString():TEXT("buff"));E.Color=Tint(D.Id==TEXT("chill")?TEXT("5ab8f0"):D.Id==TEXT("freeze")?TEXT("d8f6ff"):D.Id==TEXT("shock")?TEXT("ffe14d"):D.Id==TEXT("ignite")?TEXT("ff7a2e"):D.Id==TEXT("bleed")?TEXT("e0334a"):D.Id==TEXT("poison")?TEXT("7ee36b"):D.Id==TEXT("stun")?TEXT("f7d046"):D.Category==TEXT("buff")?TEXT("64dfac"):TEXT("c38bff"));
  E.Stacks=Live->Stacks;E.Cap=D.MaxStacks;E.ExpiresAt=Live->Expire;E.Remaining=Live->Expire-Now;
  for(const auto Id:D.Traits){const auto* T=S.Host.Library.Traits.Find(Id);if(T)E.Description+=TEXT("\n")+T->DisplayName;}
  if(Live->Owner==Owner)Source(E,S,SelfSource(S,[&](const auto& A,const auto& P){return A.Op==EOp::ApplyStatus&&A.Origin==EOrigin::Owner&&A.StateKey==D.Key;}));Out.Add(E);
 }
 TArray<FDWMaintainedView> Maintained;W.MaintainedSnapshot(Owner,Maintained);
 for(const auto& Live:Maintained)
 {
  FOpusPlayerEffect E;E.Id=FString::Printf(TEXT("layers:%llu"),Live.Key);E.Name=TEXT("技能层数");E.Color=Tint(TEXT("e7c883"));E.Icon=Icon(TEXT("status"),TEXT("buff"));Source(E,S,W.GetRoot(Live.Root).Plan);
  if(E.Source.StartsWith(TEXT("技能 · ")))E.Name=E.Source.Mid(5);E.Stacks=Live.Stacks;E.Cap=Live.Cap;E.Remaining=FMath::Max(0.,Live.Expire-Now);E.ExpiresAt=Live.Expire;E.Timer=TEXT("下次减层");
  E.Description=FString::Printf(TEXT("停止补充后逐层衰减，每 %g 秒减少一层。"),Live.DecayInterval);Out.Add(E);
 }
 for(const auto& G:S.Host.Grants())if(G.Target==S.TargetId&&G.Expire>Now)
 {
  const FString Label=G.Label.IsEmpty()?TEXT("临时增益"):G.Label;auto* Existing=Out.FindByPredicate([&](const auto& E){return E.Id==TEXT("grant:")+Label;});
  if(!Existing){FOpusPlayerEffect E;E.Id=TEXT("grant:")+Label;E.Name=Label;E.Source=TEXT("临时增益 · ")+Label;E.Color=Tint(TEXT("64dfac"));E.Icon=Icon(TEXT("status"),TEXT("buff"));
   if(!G.Key.IsEmpty())Source(E,S,SelfSource(S,[&](const auto& A,const auto& P){return A.Op==EOp::GrantTraits&&P.Grants.IsValidIndex(A.Grant)&&P.Grants[A.Grant].Key==G.Key;}));Existing=&Out.Add_GetRef(E);}
  Existing->ExpiresAt=FMath::Max(Existing->ExpiresAt,double(G.Expire));Existing->Remaining=Existing->ExpiresAt-Now;
  for(const auto& T:G.Sources)if(const auto* D=S.Host.Library.Traits.Find(T.Id)){if(!Existing->Description.Contains(D->DisplayName))Existing->Description+=(Existing->Description.IsEmpty()?TEXT(""):TEXT("\n"))+D->DisplayName;}
 }
 for(const auto& Storm:W.GetSummons())if(Storm.bStorm&&!Storm.bDead&&Storm.Expire>Now&&W.GetRoot(Storm.Root).Owner==Owner)
 {
  const auto& P=W.GetPlan(W.GetRoot(Storm.Root).Plan);const FName Kind=P.KindNames.IsValidIndex(Storm.Kind)?P.KindNames[Storm.Kind]:NAME_None;FOpusPlayerEffect E;
  E.Id=TEXT("effect:")+Kind.ToString();E.Name=Kind.ToString();if(const auto* T=S.Host.Library.Traits.Find(Kind))E.Name=T->DisplayName;
  E.Source=TEXT("持续效果");E.Icon=Icon(TEXT("status"),TEXT("shock"));E.Color=Tint(TEXT("82bded"));E.Description=TEXT("持续生效 · 自动索敌落雷");Source(E,S,W.GetRoot(Storm.Root).Plan);E.ExpiresAt=Storm.Expire;E.Remaining=Storm.Expire-Now;
  E.Id+=TEXT(":")+FPaths::GetBaseFilename(E.Icon);
  if(auto* Existing=Out.FindByPredicate([&](const auto& V){return V.Id==E.Id&&V.Icon==E.Icon;})){Existing->ExpiresAt=FMath::Max(Existing->ExpiresAt,E.ExpiresAt);Existing->Remaining=Existing->ExpiresAt-Now;}else Out.Add(E);
 }
 if(S.Character.Stunned>0&&!Out.ContainsByPredicate([](const auto& E){return E.Id==TEXT("status:stun")||E.Id==TEXT("status:freeze");}))
 {FOpusPlayerEffect E;E.Id=TEXT("control:stun");E.Name=TEXT("眩晕");E.Source=TEXT("异常状态 · 受击控制");E.Icon=Icon(TEXT("status"),TEXT("stun"));E.Color=Tint(TEXT("f7d046"));E.Description=TEXT("当前无法行动。");E.Remaining=S.Character.Stunned;E.ExpiresAt=Now+E.Remaining;Out.Add(E);}
 return Out;
}
bool UOpusPlayerEffects::VerifySnapshot(UDWRuleWorldSubsystem* R)
{
 if(!R||!R->PlayerSession())return false;const auto Before=R->PlayerSession()->Host.World.StateHash();const auto Rows=Snapshot(R);
 for(const auto& E:Rows)if(E.Remaining<0||E.ExpiresAt<Clock(R)||E.Icon.IsEmpty())return false;
 return Before==R->PlayerSession()->Host.World.StateHash();
}

class SOpusOutlineProgress : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SOpusOutlineProgress){} SLATE_END_ARGS()
 void Construct(const FArguments&){}
 float Ratio=1;FLinearColor Color=FLinearColor::White;
 virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(48,48);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& List,int32 Layer,const FWidgetStyle& Style,bool) const override
 {
  // Same rounded-square path as UICreator, starting at top center clockwise.
  TArray<FVector2D> Path;auto Point=[&](double X,double Y){Path.Emplace(X*G.GetLocalSize().X/48,Y*G.GetLocalSize().Y/48);};
  Point(24,2);Point(39,2);auto Arc=[&](double X,double Y,double Start){for(int I=1;I<=12;I++){const double A=FMath::DegreesToRadians(Start+90.*I/12);Point(X+7*FMath::Cos(A),Y+7*FMath::Sin(A));}};
  Arc(39,9,-90);Point(46,39);Arc(39,39,0);Point(9,46);Arc(9,39,90);Point(2,9);Arc(9,9,180);Point(24,2);
  const float Thickness=2*G.GetLocalSize().X/48;FSlateDrawElement::MakeLines(List,Layer,G.ToPaintGeometry(),Path,ESlateDrawEffect::None,Tint(TEXT("3d4858"))*Style.GetColorAndOpacityTint(),true,Thickness);
  double Total=0;for(int I=1;I<Path.Num();I++)Total+=(Path[I]-Path[I-1]).Size();double Left=Total*FMath::Clamp(Ratio,0.f,1.f);TArray<FVector2D> Active;Active.Add(Path[0]);
  for(int I=1;I<Path.Num()&&Left>0;I++){const double Length=(Path[I]-Path[I-1]).Size();Active.Add(FMath::Lerp(Path[I-1],Path[I],FMath::Min(1.,Left/Length)));Left-=Length;}
  if(Active.Num()>1)FSlateDrawElement::MakeLines(List,Layer+1,G.ToPaintGeometry(),Active,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint(),true,Thickness);return Layer+1;
 }
};
TSharedRef<SWidget> UOpusOutlineProgress::RebuildWidget(){SAssignNew(Outline,SOpusOutlineProgress);Outline->Ratio=Ratio;Outline->Color=Color;return Outline.ToSharedRef();}
void UOpusOutlineProgress::SetValue(float V,FLinearColor C){Ratio=FMath::Clamp(V,0.f,1.f);Color=C;if(Outline){Outline->Ratio=Ratio;Outline->Color=C;Outline->Invalidate(EInvalidateWidgetReason::Paint);}}
void UOpusOutlineProgress::ReleaseSlateResources(bool B){Super::ReleaseSlateResources(B);Outline.Reset();}
void UOpusPlayerEffects::Countdown(UOpusWorldScreen* S,float Ratio,FLinearColor C)
{
 if(!S)return;auto* N=S->FindSourceNode(TEXT("rs-effect-countdown"));if(!N)return;UOpusOutlineProgress* P=nullptr;
 for(auto* Child:N->GetAllChildren())if((P=Cast<UOpusOutlineProgress>(Child)))break;
 if(!P){P=NewObject<UOpusOutlineProgress>(N);P->SetVisibility(ESlateVisibility::HitTestInvisible);auto* Slot=N->AddChildToCanvas(P);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));}
 N->AuthoredAppearance=true;if(N->Surface)N->Surface->SetVisibility(ESlateVisibility::Collapsed);P->SetValue(Ratio,C);S->FillValues.Add(TEXT("player-effect:progress"),Ratio);
}
void UOpusPlayerEffects::FitTooltip(UOpusWorldScreen* S)
{
 if(!S)return;auto* Description=S->FindSourceNode(TEXT("rs-effect-tip-description"));auto* Frame=S->FindSourceNode(TEXT("rs-effect-tip-frame"));if(!Description||!Frame)return;
 S->ForceLayoutPrepass();UTextBlock* Text=nullptr;for(auto* W:Description->GetAllChildren())if(auto* Fit=Cast<UScaleBox>(W))Text=Cast<UTextBlock>(Fit->GetChildAt(0));if(!Text)return;
 const float Height=FMath::Max(30.f,float(Text->GetDesiredSize().Y)+4),WindowHeight=122+Height+14;
 if(auto* Slot=Cast<UCanvasPanelSlot>(Description->Slot))Slot->SetSize(FVector2D(252,Height));Description->DesignRect.W=Height;
 if(auto* Slot=Cast<UCanvasPanelSlot>(Frame->Slot))Slot->SetSize(FVector2D(280,WindowHeight));Frame->DesignRect.W=WindowHeight;
 if(Frame->Surface){auto Brush=Frame->Surface->GetBrush();Brush.DrawAs=ESlateBrushDrawType::Box;Brush.Margin=FMargin(8.f/FMath::Max(1.f,float(Brush.ImageSize.X)),8.f/FMath::Max(1.f,float(Brush.ImageSize.Y)));Frame->Surface->SetBrush(Brush);if(auto* Slot=Cast<UCanvasPanelSlot>(Frame->Surface->Slot)){Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));}}
 S->TemplateSize=FVector2D(280,WindowHeight);S->SetDesiredSizeInViewport(S->TemplateSize);
}
