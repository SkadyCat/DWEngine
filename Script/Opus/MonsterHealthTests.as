// Opt-in transient integration probe: real native casts, pools and actor death.
class UDWOMonsterHealthTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Target;
 int Stage=0;
 float Clock=0,LastCast=-10,FrozenLag=0;
 bool TrailVerified=false;
 bool Check(bool Good,FString What)
 {
  if(!Good){UOpusNative::WriteLog("HEALTH_TEST_FAIL "+What);UOpusNative::Quit(false);}
  return Good;
 }
 void Initialize(ADWOTowerGameMode InTower)
 {
  Tower=InTower;Tower.StartChallenge();Stage=-1;
 }
 void BeginProbe()
 {
  Stage=0;
  if(!Check(Tower.MonsterHealthHUD!=nullptr&&Tower.MonsterHealthHUD.IsInViewport(),"viewport overlay created"))return;
  if(!Check(Tower.MonsterHealthHUD.GetVisibility()==ESlateVisibility::HitTestInvisible,"overlay passes mouse input"))return;
  int Shielded=0;
  for(auto E:Tower.Enemies)
  {
   auto P=Tower.Rules.GetTargetPools(E.EntityId);
   if(!Check(E.HealthBar.Visible&&Math::Abs(E.HealthBar.Percent-P.Life/P.MaxLife)<0.00001&&E.HealthBarHeight>=70,"spawn snapshot and model anchor"))return;
   if(P.MaxShield>0){Shielded++;if(!Check(Math::Abs(E.HealthBar.ShieldPercent-P.Shield/P.MaxShield)<0.00001,"native shield fill"))return;}
   E.Speed=0; // Keep the transient cast fixture stationary, outside the hero.
  }
  Target=Tower.Enemies[0];
  Tower.MonsterHealthHUD.RefreshBars();
  if(!Check(Tower.MonsterHealthHUD.Bars.Num()==Tower.Enemies.Num(),"one UICreator instance per live monster"))return;
  auto Skin=Tower.MonsterHealthHUD.Bars[Target.EntityId];
  if(!Check(Skin.FindSourceNode("monster-fill")!=nullptr&&Skin.FindSourceNode("monster-fill").Normal.Texture!=nullptr&&UOpusUI::SourceHash(Skin)=="5e16349bc923b90841d311296916957f4fc59986ea4c7004e58b884da747152f","frozen UICreator health-strip resource"))return;
  UOpusNative::WriteLog("HEALTH_TEST_PASS SpawnAndInput shielded="+Shielded);
  Fire(20);
 }
 void Fire(float Power)
 {
  if(Tower.Hero.CastAt(Target.ActorLocation-FVector(180,0,0),Target.ActorLocation,Power))LastCast=Clock;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Tower==nullptr)return;
  if(Stage==-1){if(Tower.Preparing)return;BeginProbe();return;}
  if(Stage==5){if(Tower.Preparing)return;CheckBoss();return;}
  if(Target==nullptr)return;Clock+=DeltaSeconds;
  if(Clock>15){Check(false,"native damage/death probe timed out");return;}
  auto P=Tower.Rules.GetTargetPools(Target.EntityId);
  if(Stage==0)
  {
   if(P.bActive&&!P.bDead&&P.Life<P.MaxLife)
   {
    Target.RefreshHealthBar(0);
    Tower.MonsterHealthHUD.RefreshBars();
    if(!Check(Math::Abs(Target.HealthBar.Percent-P.Life/P.MaxLife)<0.00001&&Target.HealthBar.LagPercent>Target.HealthBar.Percent,"actual native hit drives red fill and gold trail"))return;
    auto Skin=Tower.MonsterHealthHUD.Bars[Target.EntityId];
    if(!Check(Math::Abs(Skin.FillValues["world-health"]-Target.HealthBar.Percent)<0.00001&&Math::Abs(Skin.FillValues["world-lag"]-Target.HealthBar.LagPercent)<0.00001,"UICreator fills receive actual native ratios"))return;
    FrozenLag=Target.HealthBar.LagPercent;Tower.SetPaused(true);Stage=1;Clock=0;
    UOpusNative::WriteLog("HEALTH_TEST_PASS NativeDamage life="+P.Life+" max="+P.MaxLife+" fill="+Target.HealthBar.Percent+" lag="+FrozenLag);
    FString Capture=UOpusNative::StringOption("DWOCaptureDamage");if(!Capture.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Capture+" -nosuffix");
   }
   else if(Clock-LastCast>1)Fire(20);
  }
  else if(Stage==1&&Clock>0.25)
  {
   if(!Check(Math::Abs(Target.HealthBar.LagPercent-FrozenLag)<0.00001,"pause freezes the damage trail"))return;
   Tower.SetPaused(false);Stage=2;Clock=0;
   UOpusNative::WriteLog("HEALTH_TEST_PASS Pause");
  }
  else if(Stage==2)
  {
   if(Clock>0.1&&!TrailVerified)
   {
    if(!Check(Target.HealthBar.LagPercent<FrozenLag&&Target.HealthBar.LagPercent>=Target.HealthBar.Percent,"trail resumes and catches up"))return;
    TrailVerified=true;UOpusNative::WriteLog("HEALTH_TEST_PASS TrailCatchUp");
   }
   if(Target.Dead)
   {
    if(!Check(!Target.HealthBar.Visible,"native death removes bar before corpse expires"))return;
    Tower.MonsterHealthHUD.RefreshBars();if(!Check(!Tower.MonsterHealthHUD.Bars.Contains(Target.EntityId),"death removes the UICreator viewport instance"))return;
    Tower.SpawnEncounter(EDWOTowerCommand::Boss);Stage=5;
   }
   else if(Clock>0.35&&Clock-LastCast>1)Fire(Math::Max(10000.0,P.MaxLife*100));
  }
  else if(Stage==3&&Tower.Captured&&Tower.CaptureClock>=7)Complete();
 }
 void CheckBoss()
 {
  auto Boss=Tower.Enemies.Last();auto BP=Tower.Rules.GetTargetPools(Boss.EntityId);
  if(!Check(Boss.Boss&&Boss.HealthBar.Visible&&BP.MaxLife>0&&Math::Abs(Boss.HealthBar.Percent-BP.Life/BP.MaxLife)<0.00001,"boss uses its own native pool"))return;
  Tower.MonsterHealthHUD.RefreshBars();if(!Check(Tower.MonsterHealthHUD.Bars.Contains(Boss.EntityId),"boss receives the same UICreator template"))return;
  UOpusNative::WriteLog("HEALTH_TEST_PASS DeathAndBoss");Stage=3;
  if(UOpusNative::StringOption("DWOCapture").IsEmpty())Complete();
 }
 void Complete(){UOpusNative::WriteLog("HEALTH_TEST_PASS RuntimeIntegration");UOpusNative::Quit(true);Stage=4;}
}
