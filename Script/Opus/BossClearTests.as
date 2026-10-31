// Real native skill damage, tower completion and saved UICreator return action.
class UDWOBossClearTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Boss;
 UPROPERTY() ADWOEnemy Minion;
 int Stage=0,InitialClears=0,InitialCasts=0,KillsAfterBoss=0;
 double Clock=0,NextShot=0;
 bool Done=false;
 void Initialize(ADWOTowerGameMode Value){Tower=Value;InitialClears=UDWOTowerJourney::Get().Cleared;}
 bool Check(bool Good,FString Message){if(!Good){Done=true;UOpusNative::WriteLog("BOSS_CLEAR_FAIL "+Message);UOpusNative::Quit(false);}return Good;}
 int MinionsAlive() const {int Count=0;for(auto E:Tower.Enemies)if(IsValid(E)&&!E.Dead&&!E.Boss&&E.EventId.IsEmpty())Count++;return Count;}
 void Next(int Value){Stage=Value;Clock=0;NextShot=0;}
 void Fire(ADWOEnemy Target)
 {
  if(Clock<NextShot)return;NextShot=Clock+1;
  TArray<FVector> Directions;Directions.Add(FVector(1,0,0));Directions.Add(FVector(-1,0,0));Directions.Add(FVector(0,1,0));Directions.Add(FVector(0,-1,0));
  for(auto Direction:Directions){FVector Origin=Target.ActorLocation-Direction*180;if(DWO3CGeometry::Sweep(Origin,Target.ActorLocation,15)<0){Tower.Hero.CastAt(Origin,Target.ActorLocation,1000000);return;}}
  Check(false,"no clear native projectile origin");
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;
  if(Stage==0)
  {
   if(!Tower.Dungeon.Ready||!Tower.DropService.Ready||!Tower.Hero.Visual.Ready)return;
   Tower.Rules.SetPlayerGod(true);Tower.Hero.ControlProbe=true;Tower.Hero.CharacterMovement.StopMovementImmediately();Tower.Hero.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
   Tower.StartChallenge();Next(1);return;
  }
  Clock+=Dt;if(!Check(Clock<60,"timeout stage="+Stage))return;
  if(Stage==1)
  {
   if(Tower.Preparing)return;
   int Bosses=0;for(auto E:Tower.Enemies){E.Speed=0;if(E.Boss){Boss=E;Bosses++;}}
   if(!Check(Bosses==1&&MinionsAlive()==Tower.Run.EncounterCount()-1,"complete initial batch includes exactly one boss"))return;
   if(!Check(!Tower.Recent.UI.CanTravelHideout(),"living boss still blocks leaving for the hideout"))return;
   InitialCasts=Tower.Hero.Visual.CastSerial;Next(2);return;
  }
  if(Stage==2)
  {
   if(!Boss.Dead){Fire(Boss);return;}
   if(Tower.Run.Phase!=EDWOTowerPhase::Cleared)return;
   if(!Check(Tower.Hero.Visual.CastSerial>InitialCasts&&MinionsAlive()>0&&UDWOTowerJourney::Get().Cleared==InitialClears+1&&Tower.Recent.State.FirstFloorComplete,"native boss death completes floor and quest with ordinary monsters alive"))return;
   if(!Check(UOpusNative::Loot(Tower.Rules).Num()>0,"native boss drop survives completion"))return;
   UOpusNative::WriteLog("BOSS_CLEAR_PASS BossKillCompletesWithMinions alive="+MinionsAlive());KillsAfterBoss=Tower.Kills;Next(3);return;
  }
  if(Stage==3)
  {
   if(Clock<.3||Tower.Hud.Screen.CurrentState!="completion")return;
   if(!Check(Tower.Paused,"completion popup pauses combat"))return;
   auto Button=Tower.Hud.Screen.FindSourceNode("battle_live_modal_page::battle-completion-pickup");
   if(!Check(Button!=nullptr,"saved return-to-loot action exists"))return;
   Tower.Hud.Screen.HandleInvoke(Button);Next(4);return;
  }
  if(Stage==4)
  {
   if(Clock<.3||Tower.Paused)return;
   if(!Check(Tower.CanFight()&&Tower.CanUseSkills(),"remaining monsters can still fight after dismissing completion"))return;
   if(!Check(Tower.Recent.UI.CanTravelHideout(),"boss completion permits leaving without clearing minions"))return;
   for(auto E:Tower.Enemies)if(IsValid(E)&&!E.Dead&&!E.Boss){Minion=E;break;}
   if(!Check(Minion!=nullptr,"ordinary monster remains available"))return;
   UOpusNative::WriteLog("BOSS_CLEAR_PASS CompletionPopupAndReturn");Next(5);return;
  }
  if(Stage==5)
  {
   if(!Minion.Dead){Fire(Minion);return;}
   if(!Check(Tower.Run.Phase==EDWOTowerPhase::Cleared&&Tower.Kills>KillsAfterBoss&&UDWOTowerJourney::Get().Cleared==InitialClears+1,"post-clear native kill cannot repeat completion"))return;
   UOpusNative::WriteLog("BOSS_CLEAR_PASS PostClearCombatAndSingleSettlement");
   if(!Check(DWOTowerTests::Run(),"boss flow, timeout and death regressions"))return;
   Done=true;UOpusNative::WriteLog("BOSS_CLEAR_PASS Complete");UOpusNative::Quit(true);
  }
 }
}
