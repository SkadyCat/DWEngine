// Explicit transient fixtures: real player damage, original action notifies and saved HUD.
class UDWOPlayerStunTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Enemy;
 int Stage=0,BeforeCasts=0,BeforeHits=0;double Clock=0,Total=0,Mana=0;bool Done=false;
 double RecoverySnapshot=0,LastNativeClock=0;
 FString ShieldSavedProfile;
 bool Check(bool Ok,FString Message)
 {
  if(Ok)return true;Done=true;UOpusNative::WriteLog("STUN_TEST_FAIL "+Message);UOpusNative::Quit(false);return false;
 }
 void Next(int Value){Stage=Value;Clock=0;}
 bool Crust(double Expected)
 {
  auto HUD=Tower.MonsterHealthHUD;HUD.RefreshBars();
  if(!Check(HUD.PlayerBar!=nullptr&&HUD.PlayerBar.FindSourceNode("hero-stun-break")==nullptr,"stale combined fracture artwork is removed"))return false;
  if(!Check(Math::Abs(Tower.Hero.Stun.Integrity()-Expected)<.025,"stun mechanics preserve their existing integrity state"))return false;
  for(int I=0;I<8;I++)
  {
   if(!Check(HUD.PlayerBar.FindSourceNode("hero-stun-slab-"+I)==nullptr&&HUD.PlayerBar.FindSourceNode("hero-stun-chip-"+I)==nullptr,"obsolete earth slabs and chips are removed"))return false;
  }
  auto Fill=HUD.PlayerBar.FindSourceNode("hero-stun-fill");
  if(!Check(Fill!=nullptr&&Fill.Normal.Texture!=nullptr,"authored yellow overlay texture is loaded"))return false;
  double Life=Tower.Rules.GetPlayerPools().MaxLife,Amount=Tower.Hero.Stun.Meter.Amount;
  if(!Check(Math::Abs(HUD.PlayerBar.FillValues["world-stun"]-(Life>0?Math::Clamp(Amount/Life,0.0,1.0):0))<.001,"head overlay uses accumulated stun divided by maximum life"))return false;
  return true;
 }
 bool ShieldOnly()
 {
  auto H=Tower.Hero;auto S=H.Stun;FString Error;ShieldSavedProfile=Tower.Rules.ExportPlayerState();
  // A transient equipment instance uses existing ordinary shield traits. No live
  // profile, damage resolver or player base parameters are edited for this test.
  FString Profile="{\"initialized\":true,\"instances\":{\"stun_shield\":{\"uid\":\"stun_shield\",\"baseId\":\"cinder_guard_chest\",\"name\":\"Shield fixture\",\"slot\":\"chest\",\"rarity\":\"rare\",\"tier\":1,\"level\":1,\"identified\":false,\"affixes\":[{\"id\":\"shield_base\",\"name\":\"Shield\",\"traits\":[{\"id\":\"shield_base\",\"params\":{\"n\":1000}}]},{\"id\":\"shield_bypass\",\"name\":\"Bypass\",\"traits\":[{\"id\":\"shield_bypass\",\"params\":{\"n\":10}}]}]}},\"loadout\":{\"skills\":[],\"equipment\":{\"chest\":\"stun_shield\"}}}";
  Profile=Profile.Replace("\"identified\":false","\"identified\":true");
  bool Imported=Tower.Rules.ImportPlayerState(Profile,Error);
  if(!Check(Imported,"shield equipment fixture imports: "+Error))return false;
  Tower.Rules.RespawnPlayer();S.Reset();S.ReadRules();auto Before=Tower.Rules.GetPlayerPools();int Triggers=S.TriggerCount;
  double Limit=S.Threshold();H.CharacterEvents.TestHit(Limit/2);auto Half=Tower.Rules.GetPlayerPools();
  if(!Check(Before.Shield>Limit&&Half.Life==Before.Life&&Half.Shield<Before.Shield&&Math::Abs(S.Meter.Amount-Limit/2)<.001,"shield-only hit wears half the crust at full life"))return false;
  H.CharacterEvents.TestHit(Limit/2);auto After=Tower.Rules.GetPlayerPools();
  if(!Check(After.Life==Before.Life&&After.Shield>0&&S.Broken&&S.TriggerCount==Triggers+1&&S.Remaining()>.49,"shield cannot stop accumulated half-second stun at full life"))return false;
  UOpusNative::WriteLog("STUN_TEST_PASS ShieldOnlyHitsAccumulateAtFullLife");return true;
 }
 bool PureRules()
 {
  FDWOStunBuildup M;M.Hit(40,1);M.Tick(1,1,4);
  if(!Check(Math::Abs(M.Amount-40)<0.0001,"one second delay keeps full stock"))return false;
  M.Tick(2,1,4);if(!Check(Math::Abs(M.Amount-20)<0.0001,"linear half stock after two decay seconds"))return false;
  M.Hit(10,1);M.Tick(1,1,4);if(!Check(Math::Abs(M.Amount-30)<0.0001,"new hit restarts delay using remaining stock"))return false;
  M.Tick(4,1,4);if(!Check(M.Amount<0.0001,"four seconds empties stock"))return false;
  M.Hit(20,1);for(int I=0;I<300;I++)M.Tick(1.0/60.0,1,4);
  if(!Check(M.Amount<0.0001,"sixty fps decay reaches same endpoint"))return false;
  M.Reset();M.Hit(20,1);for(int I=0;I<75;I++)M.Tick(1.0/15.0,1,4);
  if(!Check(M.Amount<0.0001,"fifteen fps decay reaches same endpoint"))return false;
  UOpusNative::WriteLog("STUN_TEST_PASS CompositeMeterBoundaryAndDecay");return true;
 }
 void Initialize(ADWOTowerGameMode T)
 {
  Tower=T;if(!PureRules())return;T.Hero.ControlProbe=true;T.StartChallenge();T.Rules.SetPlayerGod(true);
 }
 void Shot(FString Name){if(!UOpusNative::Flag("DWOStunVisual"))return;System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/PlayerStun/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;
  double NativeClock=Tower.Hero.CharacterEvents.SimulationClock();Clock+=Stage==17?double(Dt):Math::Max(0.0,NativeClock-LastNativeClock);LastNativeClock=NativeClock;Total+=Dt;
  if(Total>75){Check(false,"runtime timeout stage="+Stage);return;}
  auto H=Tower.Hero;auto V=H.Visual;auto S=H.Stun;
  if(Stage==0)
  {
   for(auto E:Tower.Enemies)if(IsValid(E)){E.SetActorTickEnabled(false);if(Enemy==nullptr)Enemy=E;}
   if(!Tower.Preparing&&V.Ready&&H.CharacterMovement.MovementMode==EMovementMode::MOVE_None)H.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
   if(Tower.Preparing||!V.Ready||Enemy==nullptr||!H.CharacterMovement.IsMovingOnGround()){Clock=0;return;}
   if(Clock<4)return;
   Enemy.SetActorLocation(H.CombatLocation()+FVector(450,0,0));
   if(!Check(S.Initialized&&Math::Abs(S.Threshold()-H.Rules.GetPlayerPools().MaxLife*.2)<.001&&S.Duration==.5&&S.Delay==1&&S.DecaySeconds==4&&S.RestoreSeconds==1,"fixed character equipment supplies all rule parameters"))return;
   if(!Crust(1))return;
   for(auto A:UOpusData::AffixPool(Tower.Rules))if(!Check(A.Id!="player_stun_buildup"&&A.Id!="player_stun_threshold"&&A.Id!="player_stun_recovery","fixed character parameters stay outside random equipment pool"))return;
   if(!Check(Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-resources-instance::rs-stun")!=nullptr,"saved WBP has stun control"))return;
   auto Resource=Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-resources-instance::rs-stun");
   if(!Check(Resource.Normal.Texture!=nullptr&&Resource.SourceType=="Svg"&&Resource.SourcePropertiesJson.Contains("rs-svg-stun-yellow-fill")&&Resource.Normal.SourceSvg.Contains("#f7d046")&&Resource.Normal.SourceSvg.Contains("0.55"),"cold-loaded saved WBP uses authored translucent yellow SVG"))return;
   Tower.Rules.SetPlayerGod(false);H.Aim=FVector(1,0,0);H.AimPoint=Enemy.ActorLocation;
   if(!Check(V.RequestCast(H.CombatLocation()+H.Aim*60,Enemy.ActorLocation,30),"real pending attack starts"))return;
   BeforeCasts=V.CastSerial;BeforeHits=V.HitSerial;Mana=H.Rules.GetPlayerPools().Mana;
   H.CharacterEvents.TestHit(20);H.CharacterEvents.TestHit(20);
   if(!Check(V.PendingCast&&V.HitSerial==BeforeHits&&Math::Abs(S.Meter.Amount-40)<.001,"subthreshold hits accumulate without interrupting attack"))return;
   Next(1);return;
  }
  if(Stage==1)
  {
   if(Clock<.1)return;if(!Crust(.5))return;Shot("stun-half");
   // Retire this fixture's windup before its original notify; start a fresh one
   // so the threshold crossing exercises cancellation before release.
   V.MovementPressed();V.Anim.CancelCleanActionForMovement();Next(2);return;
  }
  if(Stage==2)
  {
   if(V.RotationLocked())return;
   if(!Check(V.RequestCast(H.CombatLocation()+H.Aim*60,Enemy.ActorLocation,30),"second real pending attack starts"))return;
   BeforeCasts=V.CastSerial;Mana=H.Rules.GetPlayerPools().Mana;S.Meter.Reset();
   H.CharacterEvents.TestHit(79);
   if(!Check(S.Remaining()<=0&&V.PendingCast&&Math::Abs(S.Meter.Amount-79)<.001,"one point of intact crust does not stun"))return;
   H.CharacterEvents.TestHit(1);
   if(!Check(S.TriggerCount==1&&S.Meter.Amount==0&&S.Remaining()>.49&&!V.PendingCast,"threshold clears meter, applies half-second stun and cancels intent"))return;
   if(!Check(S.Broken&&S.Integrity()==0,"threshold shatters the entire earth layer"))return;
   if(!Crust(0))return;
   auto P=H.Rules.GetPlayerPools();
   if(!Check(P.ActionScale==0&&P.MoveScale==0&&!V.CanDodge(),"native control stops action, movement and dodge"))return;
   V.HandleAttackRelease(V.Clips[n"cast"]);
   if(!Check(V.CastSerial==BeforeCasts&&H.Rules.GetPlayerPools().Mana==Mana,"stale release cannot cast or spend mana"))return;
   Next(3);return;
  }
  if(Stage==3)
  {
   if(Clock>=.1&&Clock<.2)Shot("stun-active");
   if(S.Remaining()>0)
   {
    if(!Check(!V.RequestCast(H.CombatLocation()+H.Aim*60,Enemy.ActorLocation,30),"attack rejected during stun"))return;return;
   }
   if(!Check(Clock>=.45&&Clock<.75&&H.Rules.GetPlayerPools().ActionScale>0,"control expires at half a second"))return;
   UOpusNative::WriteLog("STUN_TEST_PASS NativeThresholdInterruptAndHalfSecondControl");
   if(!Check(S.Recovering()&&S.Restored<.08,"restoration starts only after native stun ends"))return;
   Next(7);return;
  }
  if(Stage==7)
  {
   if(Clock<.48)return;
   if(!Check(S.Restored>.43&&S.Restored<.58&&S.TriggerCount==1,"one second recovery is half complete after half a second"))return;
   if(!Crust(S.Integrity()))return;Shot("stun-restore");
   double Life=H.Rules.GetPlayerPools().Life;H.CharacterEvents.TestHit(90);
   if(!Check(H.Rules.GetPlayerPools().Life<Life&&S.Meter.Amount==0&&S.TriggerCount==1,"recovering layer permits normal health damage without repeated stun"))return;
   RecoverySnapshot=S.Restored;Tower.SetPaused(true);Next(17);return;
  }
  if(Stage==17)
  {
   if(Clock<.18)return;
   if(!Check(Math::Abs(S.Restored-RecoverySnapshot)<.001,"pause freezes crust restoration"))return;
   Tower.SetPaused(false);Next(8);return;
  }
  if(Stage==8)
  {
   if(S.Broken)return;
   if(!Check(Clock>.35&&Clock<.65&&S.Integrity()==1&&S.Meter.Amount==0,"earth layer is fully restored after one second clock="+Clock+" snapshot="+RecoverySnapshot+" integrity="+S.Integrity()+" amount="+S.Meter.Amount+" breakAge="+S.BreakAge))return;
   if(!Crust(1))return;Shot("stun-restored");
   UOpusNative::WriteLog("STUN_TEST_PASS CrustBreakAndOneSecondRestore");Next(9);return;
  }
  if(Stage==9)
  {
   if(Clock<.1)return;H.CharacterEvents.TestHit(20);H.CharacterEvents.TestHit(1,true);
   if(!Check(Math::Abs(S.Meter.Amount-20)<.001,"damage over time does not count as a hit after crust restoration"))return;
   Next(4);return;
  }
  if(Stage==4)
  {
   if(Clock<.85)return;
   if(!Check(Math::Abs(S.Meter.Amount-20)<.001,"live delay keeps buildup"))return;
   Next(5);return;
  }
  if(Stage==5)
  {
   if(S.Meter.Quiet<3)return;
   if(!Check(S.Meter.Amount>9&&S.Meter.Amount<11,"live bar decays to half"))return;Shot("stun-decay");Next(6);return;
  }
  if(Stage==6)
  {
   if(S.Meter.Quiet<5.05)return;
   if(!Check(S.Meter.Amount<.001,"live bar completely empties after delay plus four seconds"))return;
   if(!ShieldOnly())return;
   Next(18);return;
  }
  if(Stage==18)
  {
   if(Clock<.1)return;Tower.MonsterHealthHUD.RefreshBars();Shot("stun-shield-only");Next(19);return;
  }
  if(Stage==19)
  {
   if(Clock<.1)return;FString Error;
   if(!Check(Tower.Rules.ImportPlayerState(ShieldSavedProfile,Error),"restore transient equipment after shield fixture: "+Error))return;
   Tower.Rules.RespawnPlayer();S.Reset();
   H.CharacterEvents.TestHit(15);H.CharacterEvents.TestHit(10000);
   if(!Check(H.Rules.GetPlayerPools().bDead&&S.Meter.Amount==0&&!S.Broken&&!V.PendingCast,"death clears buildup, broken layer and pending attack"))return;
   UOpusNative::WriteLog("STUN_TEST_PASS FixedEquipmentHUDAndRecovery");Done=true;UOpusNative::Quit(true);
  }
 }
}
