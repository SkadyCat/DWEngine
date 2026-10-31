// RuleStudio3D entity overlays: monster 48x6, hero 68x8, native status badges.
// Presentation snapshots only; DWRules remains the sole owner of HP and shields.
struct FDWOMonsterHealthBar
{
 float Life=0,MaxLife=0,Shield=0,MaxShield=0;
 float Percent=0,LagPercent=0,ShieldPercent=0;
 bool Visible=false,Initialized=false;
 void Read(FDWRulePlayerPools Pools,float Dt)
 {
  MaxLife=Math::IsFinite(Pools.MaxLife)?Math::Max(0.0f,Pools.MaxLife):0;
  Life=Math::IsFinite(Pools.Life)?Math::Clamp(Pools.Life,0.0f,MaxLife):0;
  MaxShield=Math::IsFinite(Pools.MaxShield)?Math::Max(0.0f,Pools.MaxShield):0;
  Shield=Math::IsFinite(Pools.Shield)?Math::Clamp(Pools.Shield,0.0f,MaxShield):0;
  Percent=MaxLife>0?Life/MaxLife:0;
  ShieldPercent=MaxShield>0?Shield/MaxShield:0;
  Visible=Pools.bActive&&!Pools.bDead&&Life>0&&MaxLife>0;
  // Source trail loses 1.2 percentage points/frame at 60 Hz; keep its speed
  // independent of the UE frame rate. Healing snaps both strips upward.
  LagPercent=Initialized?Math::Max(Percent,LagPercent-Math::Max(0.0f,Dt)*0.72f):Percent;
  Initialized=true;
 }
 void Hide(){Visible=false;}
}

class UDWOMonsterHealthHUD : UUserWidget
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TMap<int,UOpusWorldScreen> Bars;
 UPROPERTY() UOpusWorldScreen PlayerBar;
 UPROPERTY() FDWOMonsterHealthBar PlayerHealth;
 UPROPERTY() TMap<FString,UOpusWorldScreen> StatusBadges;
 TMap<FString,int> StatusSeconds,StatusStacks;
 default bIsFocusable=false;
 default Visibility=ESlateVisibility::HitTestInvisible;
 // Geometry, gradients, rounded edges and textures belong to the frozen
 // UICreator export. This widget only owns data binding and instance lifetime.
 void Prepare(ADWOEnemy Enemy)
 {
  if(Bars.Contains(Enemy.EntityId))return;
  auto Screen=UOpusWorldUI::Create(GetOwningPlayer(),"monster");if(Screen==nullptr){Tower.Fail("Missing UICreator monster health template");return;}
  UOpusUI::SetShown(Screen,"world-name",false);Bars.Add(Enemy.EntityId,Screen);
  Screen.SetVisibility(ESlateVisibility::Collapsed);
 }
 TArray<FName> StatusIds() const
 {
  TArray<FName> Ids;
  Ids.Add(n"chill");Ids.Add(n"freeze");Ids.Add(n"shock");Ids.Add(n"ignite");Ids.Add(n"bleed");
  Ids.Add(n"slow");Ids.Add(n"poison");Ids.Add(n"erosion");Ids.Add(n"stun");Ids.Add(n"taunt");return Ids;
 }
 // Timers and stacks come from the rule simulation, including while paused.
 // The templates own glyphs, colors, text sizes, row stride and vertical anchors.
 void RefreshStatuses(int EntityId,UOpusWorldScreen Bar,FString Prefix,FVector Head,bool Shielded,TArray<FString>& Live)
 {
  TArray<FName> Active;for(auto Id:StatusIds())if(Tower.Rules.GetStatus(EntityId,Id)>0)Active.Add(Id);
  auto Track=Bar.FindSourceNode(Prefix+"-track");
  auto Anchor=Bar.FindSourceNode(Prefix+(Shielded?"-status-shield":"-status-base"));
  if(Track==nullptr||Anchor==nullptr)return;
  for(int I=0;I<Active.Num();I++)
  {
   FName Id=Active[I];FString Key=""+EntityId+":"+Id;
   if(!StatusBadges.Contains(Key))
   {
    auto Badge=UOpusWorldUI::Create(GetOwningPlayer(),"status-"+Id);
    if(Badge==nullptr){Tower.Fail("Missing UICreator status template: "+Id);return;}
    StatusBadges.Add(Key,Badge);
   }
   Live.Add(Key);auto Badge=StatusBadges[Key];
   float Remaining=Tower.Rules.GetStatusRemaining(EntityId,Id);
   int Seconds=Math::IsFinite(Remaining)?Math::Max(0,int(Math::CeilToInt(Remaining))):0;
   int Stacks=Tower.Rules.GetStatusStacks(EntityId,Id);
   if(!StatusSeconds.Contains(Key)||StatusSeconds[Key]!=Seconds){UOpusUI::SetText(Badge,"world-status-seconds",Seconds>0?""+Seconds:"");StatusSeconds.Add(Key,Seconds);}
   if(!StatusStacks.Contains(Key)||StatusStacks[Key]!=Stacks){UOpusUI::SetText(Badge,"world-status-stacks",Stacks>1?""+Stacks:"");StatusStacks.Add(Key,Stacks);}
   auto Step=Badge.FindSourceNode("status-"+Id+"-step");if(Step==nullptr){Tower.Fail("Missing UICreator status stride");return;}
   UOpusWorldUI::Place(Badge,GetOwningPlayer(),Head,FVector2D((I-(Active.Num()-1)*0.5)*Step.DesignRect.Z,Anchor.DesignRect.Y-Track.DesignRect.Y-Track.DesignRect.W*0.5));
  }
 }
 void RefreshPlayer(float Dt,TArray<FString>& LiveStatuses)
 {
  if(!IsValid(Tower.Hero))return;
  PlayerHealth.Read(Tower.Rules.GetPlayerPools(),Dt);
  if(!PlayerHealth.Visible){if(PlayerBar!=nullptr)PlayerBar.SetVisibility(ESlateVisibility::Collapsed);return;}
  if(PlayerBar==nullptr){PlayerBar=UOpusWorldUI::Create(GetOwningPlayer(),"hero");if(PlayerBar==nullptr){Tower.Fail("Missing UICreator player health template");return;}}
  UOpusWorldUI::Fill(PlayerBar,"world-health",PlayerHealth.Percent);
  UOpusWorldUI::Fill(PlayerBar,"world-lag",PlayerHealth.LagPercent);
  UOpusWorldUI::Fill(PlayerBar,"world-shield",PlayerHealth.ShieldPercent);
  UOpusUI::SetShown(PlayerBar,"world-shield",PlayerHealth.MaxShield>0);
  UOpusUI::SetShown(PlayerBar,"world-shield-track",PlayerHealth.MaxShield>0);
  UOpusWorldUI::Fill(PlayerBar,"world-stun",float(Tower.Hero.Stun.BuildupFraction()));
  // Use the existing wizard's animated model bounds, not the hidden placeholder.
  FVector Head=Tower.Hero.Visual.Ready?UOpusWorldUI::HeadAnchor(Tower.Hero.Wizard):Tower.Hero.ActorLocation+FVector(0,0,140);
  auto Track=PlayerBar.FindSourceNode("hero-track");
  float Offset=Track!=nullptr?PlayerBar.TemplateSize.Y-Track.DesignRect.Y-Track.DesignRect.W*0.5:0;
  UOpusWorldUI::Place(PlayerBar,GetOwningPlayer(),Head,FVector2D(0,Offset));
  RefreshStatuses(1,PlayerBar,"hero",Head,PlayerHealth.MaxShield>0,LiveStatuses);
 }
 void RefreshBars(float Dt=0)
 {
  if(Tower==nullptr||Tower.Rules==nullptr)return;
  auto PC=GetOwningPlayer();if(PC==nullptr)return;
  TArray<int> Live;
  TArray<FString> LiveStatuses;
  RefreshPlayer(Dt,LiveStatuses);
  for(auto Enemy:Tower.Enemies)
  {
   if(!IsValid(Enemy)||Enemy.Dead||!Enemy.HealthBar.Visible)continue;
   Live.Add(Enemy.EntityId);
   if(Enemy.Staged)continue;
   if(!Bars.Contains(Enemy.EntityId))
   {
    auto Screen=UOpusWorldUI::Create(PC,"monster");if(Screen==nullptr){Tower.Fail("Missing UICreator monster health template");return;}
    UOpusUI::SetShown(Screen,"world-name",false);Bars.Add(Enemy.EntityId,Screen);
   }
   auto Screen=Bars[Enemy.EntityId];
   UOpusWorldUI::Fill(Screen,"world-health",Enemy.HealthBar.Percent);
   UOpusWorldUI::Fill(Screen,"world-lag",Enemy.HealthBar.LagPercent);
   UOpusWorldUI::Fill(Screen,"world-shield",Enemy.HealthBar.ShieldPercent);
   UOpusUI::SetShown(Screen,"world-shield",Enemy.HealthBar.MaxShield>0);
   auto Track=Screen.FindSourceNode("monster-track");
   // Center the authored strip over the model, independently of the template's
   // transparent name area. No authored node is resized or repositioned.
   float Offset=Track!=nullptr?Screen.TemplateSize.Y-Track.DesignRect.Y-Track.DesignRect.W*0.5:0;
   UOpusWorldUI::Place(Screen,PC,Enemy.ActorLocation+FVector(0,0,Enemy.HealthBarHeight),FVector2D(0,Offset));
   RefreshStatuses(Enemy.EntityId,Screen,"monster",Enemy.ActorLocation+FVector(0,0,Enemy.HealthBarHeight),Enemy.HealthBar.MaxShield>0,LiveStatuses);
  }
  TArray<int> Old;Bars.GetKeys(Old);for(int Id:Old)if(!Live.Contains(Id)){Bars[Id].RemoveFromParent();Bars.Remove(Id);}
  TArray<FString> OldStatuses;StatusBadges.GetKeys(OldStatuses);
  for(auto Key:OldStatuses)if(!LiveStatuses.Contains(Key)){StatusBadges[Key].RemoveFromParent();StatusBadges.Remove(Key);StatusSeconds.Remove(Key);StatusStacks.Remove(Key);}
 }
 UFUNCTION(BlueprintOverride) void Tick(FGeometry MyGeometry,float InDeltaTime){RefreshBars(Tower!=nullptr&&Tower.CanFight()?InDeltaTime:0);}
 UFUNCTION(BlueprintOverride) void Destruct()
 {
  for(auto Pair:Bars)if(Pair.Value!=nullptr)Pair.Value.RemoveFromParent();Bars.Empty();
  for(auto Pair:StatusBadges)if(Pair.Value!=nullptr)Pair.Value.RemoveFromParent();StatusBadges.Empty();StatusSeconds.Empty();StatusStacks.Empty();
  if(PlayerBar!=nullptr)PlayerBar.RemoveFromParent();PlayerBar=nullptr;
 }
}
