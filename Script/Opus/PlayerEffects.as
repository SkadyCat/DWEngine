struct FDWOEffectTimer
{
 double ExpiresAt=0,Duration=1;
}
// Read-only player state presentation. UICreator owns icon, frame and hover templates.
class UDWOPlayerEffects : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TMap<FString,UOpusWorldScreen> Icons;
 UPROPERTY() UOpusWorldScreen Tooltip;
 UPROPERTY() UOpusActionNode HoverAnchor;
 TArray<FOpusPlayerEffect> Rows;
 TMap<FString,FDWOEffectTimer> Timers;
 FString Hovered;
 double RefreshIn=0;
 void Initialize(ADWOTowerGameMode T){Tower=T;if(UOpusNative::Flag("DWOPlayerEffectsTest")&&UOpusNative::Flag("DWOTransient")){auto Test=UDWOPlayerEffectsTest::Create(T);Test.Initialize(T);}}
 void CloseTooltip(){if(Tooltip!=nullptr)Tooltip.RemoveFromParent();Tooltip=nullptr;HoverAnchor=nullptr;Hovered="";}
 UFUNCTION() void Enter(USvgCanvasNode Node)
 {
  auto N=Cast<UOpusActionNode>(Node);if(N==nullptr||N.HoverId.IsEmpty())return;Hovered=N.HoverId;HoverAnchor=N;RefreshTooltip();
 }
 UFUNCTION() void Leave(USvgCanvasNode Node){if(Node==HoverAnchor)CloseTooltip();}
 void RefreshTooltip()
 {
  if(Hovered.IsEmpty())return;FOpusPlayerEffect E;bool Found=false;for(auto R:Rows)if(R.Id==Hovered){E=R;Found=true;break;}if(!Found){CloseTooltip();return;}
  if(Tooltip==nullptr){Tooltip=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"rs-effect-tooltip","UICreatorEffects",160);if(Tooltip==nullptr){Tower.Fail("Missing UICreator status hover template");return;}Tooltip.SetVisibility(ESlateVisibility::HitTestInvisible);}
  UOpusUI::SetImage(Tooltip,"player-effect:icon",E.Icon);UOpusUI::SetText(Tooltip,"player-effect:name",E.Name);UOpusUI::SetText(Tooltip,"player-effect:source",E.Source);
  UOpusUI::SetText(Tooltip,"player-effect:stack-details",E.Stacks>0?"当前层数    "+E.Stacks+(E.Cap>1?" / "+E.Cap:""):"");
  UOpusUI::SetText(Tooltip,"player-effect:time",E.Timer+"    "+Seconds(Math::Max(0.0,E.ExpiresAt-UOpusPlayerEffects::Clock(Tower.Rules))));UOpusUI::SetText(Tooltip,"player-effect:description",E.Description);
  UOpusPlayerEffects::FitTooltip(Tooltip);
 }
 FString Seconds(double Time) const{int Tenths=Math::CeilToInt(Math::Max(0.0,Time)*10);return ""+(Tenths/10)+"."+(Tenths%10)+"s";}
 void Refresh()
 {
  Rows=UOpusPlayerEffects::Snapshot(Tower.Rules);TArray<FString> Live;
  for(auto E:Rows)
  {
   Live.Add(E.Id);FDWOEffectTimer Time;if(Timers.Contains(E.Id))Time=Timers[E.Id];
   if(!Timers.Contains(E.Id)||Math::Abs(Time.ExpiresAt-E.ExpiresAt)>0.001){Time.ExpiresAt=E.ExpiresAt;Time.Duration=Math::Max(0.001,E.Remaining);Timers.Add(E.Id,Time);}
   if(!Icons.Contains(E.Id))
   {
    auto Icon=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"rs-player-effect","UICreatorEffects",140);
    if(Icon==nullptr){Tower.Fail("Missing UICreator player effect template");return;}
    UOpusWorldUI::SetHover(Icon,"player-effect:hover",E.Id);auto Events=UOpusUI::HoverEvents(Icon);Events.Entered.AddUFunction(this,n"Enter");Events.Left.AddUFunction(this,n"Leave");Icons.Add(E.Id,Icon);
   }
   UOpusUI::SetImage(Icons[E.Id],"player-effect:icon",E.Icon);UOpusUI::SetText(Icons[E.Id],"player-effect:stacks",E.Stacks>0?""+E.Stacks:"");
  }
  TArray<FString> Old;Icons.GetKeys(Old);for(auto Key:Old)if(!Live.Contains(Key)){auto Icon=Icons[Key];UOpusUI::HoverEvents(Icon).Entered.UnbindObject(this);UOpusUI::HoverEvents(Icon).Left.UnbindObject(this);Icon.RemoveFromParent();Icons.Remove(Key);Timers.Remove(Key);if(Hovered==Key)CloseTooltip();}
  RefreshTooltip();
 }
 void Place()
 {
  auto PC=Gameplay::GetPlayerController(0);auto Anchor=Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-player-effects-anchor");auto A=UOpusHoverUI::AnchorRect(Anchor,PC);
  bool Shown=!UOpusUI::BlocksCombat(Tower.Hud.Screen)&&!(Tower.Hud.Talents!=nullptr&&Tower.Hud.Talents.Visible)&&A.Z>A.X;
  int Columns=Math::Max(1,int((A.Z-A.X)/54.0)),Index=0;double Now=UOpusPlayerEffects::Clock(Tower.Rules);
  for(auto E:Rows)if(Icons.Contains(E.Id))
  {
   auto Icon=Icons[E.Id];Icon.SetVisibility(Shown?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
   UOpusHoverUI::Place(Icon,FVector2D(A.X+(Index%Columns)*54,A.Y+int(Index/Columns)*54),1);Index++;
   UOpusPlayerEffects::Countdown(Icon,float32(Math::Clamp((E.ExpiresAt-Now)/Timers[E.Id].Duration,0.0,1.0)),E.Color);
  }
  if(!Shown){CloseTooltip();return;}
  if(Tooltip!=nullptr&&HoverAnchor!=nullptr)
  {
   auto V=UOpusHoverUI::ViewSize(PC);auto R=UOpusHoverUI::AnchorRect(HoverAnchor,PC);double W=Tooltip.TemplateSize.X,H=Tooltip.TemplateSize.Y;
   double X=Math::Clamp(R.X,12.0,Math::Max(12.0,V.X-W-12)),Y=R.W+10;if(Y+H>V.Y-12)Y=Math::Max(12.0,R.Y-H-10);UOpusHoverUI::Place(Tooltip,FVector2D(X,Y),1);
  }
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Tower==nullptr||Tower.Rules==nullptr||Tower.Hud==nullptr||Tower.Hud.Screen==nullptr||UOpusNative::Flag("DWOSvgReference"))return;
  if(Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-player-effects-anchor")==nullptr)return;
  RefreshIn-=Dt;if(RefreshIn<=0){RefreshIn=.1;Refresh();}Place();
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){CloseTooltip();TArray<FString> Keys;Icons.GetKeys(Keys);for(auto Key:Keys)Icons[Key].RemoveFromParent();Icons.Empty();}
}
