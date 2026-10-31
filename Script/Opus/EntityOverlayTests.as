// Opt-in transient fixture. It uses native casts, status application and pools;
// no timers, health or effects are simulated in the presentation layer.
class UDWOEntityOverlayTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Target;
 int Stage=-1;float Clock=0,LastCast=-10,FrozenRemaining=0,FrozenLag=0;
 bool CapturedStatuses=false;
 bool Check(bool Good,FString What)
 {
  if(!Good){UOpusNative::WriteLog("ENTITY_UI_FAIL "+What);UOpusNative::Quit(false);}return Good;
 }
 void Initialize(ADWOTowerGameMode InTower)
 {
  Tower=InTower;Tower.MonsterHealthHUD.RefreshBars();auto HUD=Tower.MonsterHealthHUD;
  if(!Check(HUD.PlayerBar!=nullptr&&HUD.PlayerHealth.Visible&&!HUD.PlayerBar.Interactive,"player bar in manual Ready state passes input"))return;
  if(!Check(HUD.PlayerBar.FindSourceNode("hero-fill").Normal.Texture!=nullptr&&HUD.PlayerBar.FindSourceNode("hero-fill").DesignRect.Z==68,"player uses exported UICreator strip"))return;
  if(!Check((HUD.PlayerBar.FindSourceNode("hero-shield-track").GetVisibility()!=ESlateVisibility::Collapsed)==(HUD.PlayerHealth.MaxShield>0),"shield track follows native capacity"))return;
  UOpusNative::WriteLog("ENTITY_UI_PASS ReadyPlayerAndInput");Tower.StartChallenge();
 }
 void Populate()
 {
  FString Error;if(!Check(UOpusNative::ApplyPreset(Tower.Rules,n"autocast_shield",Error),"native shield preset "+Error))return;
  Tower.SetPaused(true);Tower.Rules.RespawnPlayer();
  // Exercise the real cross-floor carry adapter with an existing damaged
  // native pool snapshot. Read-only reflected rule fields remain untouched.
  UOpusNative::CarryPools(Tower.Rules,Tower.Rules.GetTargetPools(Target.EntityId));
  auto HUD=Tower.MonsterHealthHUD;
  for(auto Id:HUD.StatusIds())for(int Entity:Ids())
  {
   int Count=Id==n"poison"?100:Id==n"slow"?3:1;
   for(int I=0;I<Count;I++)if(!Check(Tower.Rules.ApplyStatus(Entity,Id,1,1.2),"native status accepted "+Id+" entity="+Entity))return;
  }
  HUD.RefreshBars();
  if(!Check(HUD.StatusBadges.Num()==18,"all nine actual statuses on hero and monster"))return;
  if(!Check(HUD.StatusStacks["1:slow"]==3&&HUD.StatusStacks["1:poison"]==100,"native stack counts"))return;
  for(auto Pair:HUD.StatusBadges)
  {
   if(!Check(Pair.Value.IsInViewport()&&(Pair.Value.GetVisibility()==ESlateVisibility::HitTestInvisible||Pair.Value.GetVisibility()==ESlateVisibility::Collapsed)&&!Pair.Value.Interactive,"badges pass pointer input, including offscreen culling"))return;
   if(!Check(UOpusUI::SourceHash(Pair.Value)==UOpusUI::SourceHash(HUD.PlayerBar),"badge and player share frozen UICreator provenance"))return;
  }
  auto Pools=Tower.Rules.GetPlayerPools();
  if(!Check(Pools.MaxShield>0&&Math::Abs(HUD.PlayerBar.FillValues["world-health"]-Pools.Life/Pools.MaxLife)<.00001&&Math::Abs(HUD.PlayerBar.FillValues["world-shield"]-Pools.Shield/Pools.MaxShield)<.00001,"native player health and shield ratios"))return;
  if(!Check(HUD.PlayerBar.FindSourceNode("hero-shield-track").GetVisibility()!=ESlateVisibility::Collapsed&&HUD.PlayerHealth.LagPercent>HUD.PlayerHealth.Percent,"shield track and damage trail"))return;
  FrozenRemaining=Tower.Rules.GetStatusRemaining(1,n"slow");FrozenLag=HUD.PlayerHealth.LagPercent;
  UOpusNative::WriteLog("ENTITY_UI_PASS NativePoolsAndNineStatuses stacks=3,100 widgets="+HUD.StatusBadges.Num());
  Stage=1;Clock=0;
 }
 TArray<int> Ids(){TArray<int> Out;Out.Add(1);Out.Add(Target.EntityId);return Out;}
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Tower==nullptr)return;Clock+=DeltaSeconds;
  if(Clock>18){Check(false,"integration timed out stage="+Stage);return;}
  if(Stage==-1)
  {
   if(Tower.Preparing)return;
   Target=Tower.Enemies[0];Target.SetActorLocation(Tower.Hero.ActorLocation+FVector(350,0,0));
   for(auto E:Tower.Enemies){E.Speed=0;FString Error;UOpusNative::HitPower(Tower.Rules,E.EntityId,0,Error);}
   Stage=0;Clock=0;return;
  }
  if(Stage==0)
  {
   if(Tower.Rules.GetStatus(Target.EntityId,n"chill")>0)
   {
    Tower.MonsterHealthHUD.RefreshBars();FString Key=""+Target.EntityId+":chill";
    if(!Check(Tower.MonsterHealthHUD.StatusBadges.Contains(Key),"real frost hit creates chill badge"))return;
    UOpusNative::WriteLog("ENTITY_UI_PASS FrostHitChill");
    FString Capture=UOpusNative::StringOption("DWOEntityChillCapture");
    if(!Capture.IsEmpty()){Tower.SetPaused(true);System::ExecuteConsoleCommand("Shot ShowUI filename="+Capture+" -nosuffix");Stage=6;Clock=0;}else Populate();
   }
   else if(Clock-LastCast>1){Tower.Hero.CastAt(Target.ActorLocation-FVector(180,0,0),Target.ActorLocation,20);LastCast=Clock;}
  }
  else if(Stage==6&&Clock>.35)Populate();
  else if(Stage==1&&Clock>.35&&!CapturedStatuses)
  {
   // Let the ordinary HUD refresh too, so it agrees with the head bar.
   CapturedStatuses=true;FString Capture=UOpusNative::StringOption("DWOEntityCapture");if(!Capture.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Capture+" -nosuffix");
  }
  else if(Stage==1&&Clock>.8)
  {
   auto HUD=Tower.MonsterHealthHUD;HUD.RefreshBars();
   if(!Check(Math::Abs(Tower.Rules.GetStatusRemaining(1,n"slow")-FrozenRemaining)<.00001&&Math::Abs(HUD.PlayerHealth.LagPercent-FrozenLag)<.00001,"paused rule timer and player trail stay frozen"))return;
   UOpusNative::WriteLog("ENTITY_UI_PASS Pause");Tower.SetPaused(false);Stage=2;Clock=0;
  }
  else if(Stage==2&&Clock>4)
  {
   auto HUD=Tower.MonsterHealthHUD;HUD.RefreshBars();
   if(!Check(HUD.StatusBadges.Num()==0&&HUD.StatusSeconds.Num()==0&&HUD.StatusStacks.Num()==0,"expired statuses remove all widgets and cached values"))return;
   UOpusNative::WriteLog("ENTITY_UI_PASS ExpiryCleanup");Tower.SetPaused(true);
   Tower.Rules.RespawnPlayer();HUD.RefreshBars();
   if(!Check(HUD.PlayerHealth.Percent==1&&HUD.PlayerHealth.LagPercent==1,"healing restores both strips"))return;
   if(!Check(HUD.PlayerHealth.Visible&&!HUD.PlayerBar.Interactive,"respawn restores player bar"))return;
   Tower.Rules.ApplyStatus(Target.EntityId,n"chill",10,3);HUD.RefreshBars();int Id=Target.EntityId;Target.DestroyActor();HUD.RefreshBars();
   if(!Check(!HUD.Bars.Contains(Id)&&!HUD.StatusBadges.Contains(""+Id+":chill"),"despawn removes bar and badge"))return;
   UOpusNative::WriteLog("ENTITY_UI_PASS HealRespawnDespawn");UOpusNative::WriteLog("ENTITY_UI_PASS RuntimeIntegration");Stage=3;UOpusNative::Quit(true);
  }
 }
}
