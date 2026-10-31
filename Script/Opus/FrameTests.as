// Exercise the production scheduler, including a paused preparation and retry.
class UDWOFrameTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int LastCount=0,Frames=0,MaxCreated=0;
 bool CheckedPause=false,Done=false,Started=false;
 FVector Origin;
 FDWRulePlayerPools InitialPools;
 void Initialize(ADWOTowerGameMode T)
 {
  Tower=T;
 }
 bool Check(bool Good,FString Message)
 {
  if(!Good){Done=true;UOpusNative::WriteLog("FRAME_TEST_FAIL "+Message);UOpusNative::Quit(false);}return Good;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;
  if(!Started)
  {
   if(Tower.Dungeon==nullptr||!Tower.Dungeon.Ready||!Tower.DropService.Ready||!Tower.Hero.Visual.Ready)return;
   Origin=Tower.Hero.ActorLocation;InitialPools=Tower.Rules.GetPlayerPools();Tower.StartChallenge();Started=true;return;
  }
  Frames++;
  int Count=Tower.Enemies.Num(),Created=Count-LastCount;LastCount=Count;MaxCreated=Math::Max(MaxCreated,Created);
  if(!Check(Created<=1,"at most one actor is constructed per frame"))return;
  if(!Check(Tower.Run.Wave==1&&Tower.Run.Elapsed==0,"pending wave does not advance wave or challenge clock"))return;
  auto P=Tower.Rules.GetPlayerPools();if(!Check(P.Life==InitialPools.Life&&P.Mana==InitialPools.Mana&&Tower.Hero.ActorLocation.Equals(Origin),"native combat and hero remain frozen during staging"))return;
  if(Tower.Preparing)
  {
   if(!Check(!Tower.CanFight(),"staged encounter cannot fight"))return;
   for(auto E:Tower.Enemies)if(!Check(E.Staged&&E.IsHidden(),"partial encounter stays hidden"))return;
   if(Count>0&&!CheckedPause){Tower.SetPaused(true);CheckedPause=true;}
   return;
  }
  if(!Check(CheckedPause&&Tower.Paused&&!Tower.CanFight(),"publication preserves an explicit pause"))return;
  if(!Check(Count==Tower.Run.EncounterCount()&&Frames>Count&&MaxCreated==1,"entire dungeon count is prepared over frames"))return;
  int Bosses=0;
  for(auto E:Tower.Enemies){if(!Check(!E.Staged&&!E.IsHidden(),"all monsters published together"))return;auto Pool=Tower.Rules.GetTargetPools(E.EntityId);if(!Check(Pool.bActive&&!Pool.bDead&&Pool.MaxLife>0,"every published monster has its live native rule pool"))return;if(E.Boss)Bosses++;}
  if(!Check(Bosses==1,"the single boss is published with all ordinary monsters"))return;
  UOpusNative::WriteLog("FRAME_TEST_PASS AllMonstersWithBoss count="+Count+" bosses="+Bosses);
  Tower.SetPaused(false);if(!Check(Tower.CanFight(),"resume enables the complete encounter"))return;
  UOpusNative::WriteLog("FRAME_TEST_PASS StagedEncounter count="+Count+" frames="+Frames+" maxCreated="+MaxCreated+" elapsed="+Tower.Run.Elapsed);
  if(UDWOTowerJourney::Get().Visit==1){Tower.Retry(true);Done=true;return;}
  UOpusNative::WriteLog("FRAME_TEST_PASS RetryAndAssetCache");Done=true;UOpusNative::Quit(true);
 }
}
