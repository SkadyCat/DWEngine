// Exercise the real input gate, original animation notifies and native carriers.
// Never imports or writes a normal profile, and never starts an encounter.
class UDWOFreeCastTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0,BaseCasts=0,BaseReleases=0,BaseRequests=0;
 double Clock=0,Total=0,ManaBefore=0,PaidMana=0;
 bool Done=false,SawProjectile=false;
 void Initialize(ADWOTowerGameMode T){Tower=T;T.Hero.ControlProbe=true;T.Hero.AutoCast=false;T.Rules.SetPlayerGod(false);SetTickGroup(ETickingGroup::TG_PostUpdateWork);}
 bool Check(bool Value,FString Message){if(Value)return true;Done=true;UOpusNative::WriteLog("FREE_CAST_FAIL stage="+Stage+" "+Message);UOpusNative::Quit(false);return false;}
 void Next(int Value){Stage=Value;Clock=0;}
 bool Start()
 {
  auto H=Tower.Hero;auto V=H.Visual;BaseCasts=V.CastSerial;BaseReleases=V.AuthoredReleases;BaseRequests=V.CastRequests;ManaBefore=Tower.Rules.GetPlayerPools().Mana;SawProjectile=false;
  H.AimPoint=H.CombatLocation()+FVector(650,350,0);H.Aim=(H.AimPoint-H.CombatLocation()).GetSafeNormal2D();
  H.AdvanceCastInput(true,.05,true);
  UOpusNative::WriteLog("FREE_CAST_BEGIN updates="+V.Anim.UpdateCount+" progress="+V.Anim.GetCleanActionProgress()+" pending="+V.PendingCast+" paused="+Tower.Paused);
  return Check(V.PendingCast&&V.CastRequests==BaseRequests+1&&V.CastSerial==BaseCasts&&Tower.Rules.GetPlayerPools().Mana==ManaBefore,"empty ground starts an unpaid original wind-up");
 }
 void Capture(FString Name){if(!UOpusNative::Flag("DWOFreeCastCapture"))return;System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/FreeCast/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Total+=Dt;Clock+=Dt;
  if(!Check(Total<80,"timeout"))return;
  auto H=Tower.Hero;auto V=H.Visual;auto R=Tower.Rules;
  if(!V.Ready||(!Tower.CanUseSkills()&&Stage==0))return;
  if(H.CharacterMovement.MovementMode==EMovementMode::MOVE_None)H.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
  if(!H.CharacterMovement.IsMovingOnGround())return;
  if(!Check(Tower.Enemies.Num()==0&&Tower.Run.Wave==0&&Tower.Run.Elapsed==0,"skill use must not spawn enemies or start the floor"))return;
  SawProjectile=SawProjectile||R.GetFrameReport().Projectiles>0;
  if(Stage==0)
  {
   // Do not inject input in the same frame that creates the source animation
   // instance; let that graph evaluate before testing player actions.
   if(V.Anim.UpdateCount<2)return;
   FString Error;int Slot=-1;for(int I=0;I<4;I++)if(H.DeliveryPolicy.SkillAt(I)==n"frost_lance")Slot=I;
   if(!Check(Slot>=0&&R.SelectPlayerSkill(Slot,Error),"starter frost lance "+Error))return;
   if(!Check(Tower.Run.Phase==EDWOTowerPhase::Ready&&!Tower.CanFight()&&Tower.CanUseSkills(),"ready arena can use skills without starting a wave"))return;
   H.AutoCast=true; // A manual click must override an enabled auto-aim toggle.
   if(!Start())return;Next(1);return;
  }
  if(Stage==1)
  {
   if(!Check(Clock<5,"source release marker did not fire"))return;
   if(V.CastSerial==BaseCasts)return;
   if(!Check(V.CastSerial==BaseCasts+1&&V.AuthoredReleases==BaseReleases+1&&R.GetPlayerPools().Mana<ManaBefore,"real animation marker casts and pays once without an enemy"))return;
   PaidMana=R.GetPlayerPools().Mana;V.HandleAttackRelease(V.Clips[n"cast"]);
   if(!Check(V.CastSerial==BaseCasts+1&&R.GetPlayerPools().Mana==PaidMana,"duplicate notify cannot charge or release twice"))return;
   UOpusNative::WriteLog("FREE_CAST_PASS ReadyManualReleaseAndCost");Next(2);return;
  }
  if(Stage==2)
  {
   if(!Check(Clock<12,"held manual cast stopped without enemies"))return;
   if(V.CastSerial-BaseCasts<3){H.AdvanceCastInput(true,Dt,true);return;}
   if(!Check(SawProjectile&&V.CastSerial-BaseCasts==V.AuthoredReleases-BaseReleases,"held input creates native projectiles once per animation"))return;
   H.AdvanceCastInput(false,Dt);V.MovementPressed();Capture("ready-empty-casting");UOpusNative::WriteLog("FREE_CAST_PASS HeldEmptyGroundNativeProjectiles casts="+(V.CastSerial-BaseCasts));Next(3);return;
  }
  if(Stage==3)
  {
   if(Clock<1)return;BaseRequests=V.CastRequests;BaseCasts=V.CastSerial;ManaBefore=R.GetPlayerPools().Mana;Tower.SetPaused(true);
   H.AdvanceCastInput(true,Dt);
   if(!Check(!H.CastAt(H.CombatLocation(),H.AimPoint)&&V.CastRequests==BaseRequests,"pause blocks manual cast"))return;
   Next(4);return;
  }
  if(Stage==4)
  {
   if(Clock<.4)return;
   if(!Check(V.CastSerial==BaseCasts&&R.GetPlayerPools().Mana==ManaBefore,"paused empty casts do not emit or pay"))return;
   Tower.SetPaused(false);Tower.Run.Phase=EDWOTowerPhase::Waves;H.AutoCast=true;H.AdvanceCastInput(true,Dt);
   bool Rejected=!H.CastAt(H.CombatLocation(),H.AimPoint,30,true)&&V.CastRequests==BaseRequests&&!V.PendingCast;
   H.AutoCast=false;Tower.Run.Phase=EDWOTowerPhase::Ready;
   if(!Check(Rejected,"automatic targeting still requires a living enemy"))return;
   UOpusNative::WriteLog("FREE_CAST_PASS PauseAndAutomaticTargetGuards");Tower.Run.Phase=EDWOTowerPhase::Cleared;
   Next(40);return;
  }
  if(Stage==40)
  {
   if(Clock<.3||Tower.Hud.Screen.CurrentState!="completion"){Check(Clock<3,"completion screen must appear");return;}
   if(!Check(Tower.Paused&&!Tower.CanUseSkills(),"completion modal still pauses combat input"))return;
   auto ReturnButton=Tower.Hud.Screen.FindSourceNode("battle_live_modal_page::battle-completion-pickup");
   if(!Check(ReturnButton!=nullptr,"real return-to-loot button exists"))return;
   Tower.Hud.Screen.HandleInvoke(ReturnButton);Next(41);return;
  }
  if(Stage==41)
  {
   if(Clock<.2)return;
   if(!Check(Tower.CanUseSkills()&&!Tower.CanFight()&&!UOpusUI::BlocksCombat(Tower.Hud.Screen),"return to cleared floor permits skills without an encounter"))return;
   UOpusNative::WriteLog("FREE_CAST_PASS CompletionReturnResumesSkillUse");if(!Start())return;Next(5);return;
  }
  if(Stage==5)
  {
   if(!Check(Clock<5,"cleared-floor cast did not release"))return;
   if(V.CastSerial==BaseCasts)return;
   if(!Check(V.CastSerial==BaseCasts+1&&V.AuthoredReleases==BaseReleases+1&&R.GetPlayerPools().Mana<ManaBefore,"cleared floor releases and pays normally"))return;
   Next(6);return;
  }
  if(Stage==6)
  {
   if(Clock<.06)return;
   if(!Check(SawProjectile,"cleared-floor projectile must advance in the native world"))return;
   Capture("cleared-empty-casting");Tower.SetPaused(true);UOpusNative::WriteLog("FREE_CAST_PASS ClearedManualNativeProjectile");Next(7);return;
  }
  if(Stage==7&&Clock>.6)
  {
   Tower.SetPaused(false);Tower.Run.Phase=EDWOTowerPhase::Failed;
   if(!Check(!Tower.CanUseSkills()&&!H.CastAt(H.CombatLocation(),H.AimPoint),"failed floor still blocks casts"))return;
   if(!Check(DWOTowerTests::Run(),"floor state regressions"))return;
   Done=true;UOpusNative::WriteLog("FREE_CAST_PASS NoWaveProgressNoEnemyRequired");UOpusNative::WriteLog("FREE_CAST_PASS Complete");UOpusNative::Quit(true);
  }
 }
}
