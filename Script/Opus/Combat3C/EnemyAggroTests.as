// Real actor/native-skill coverage. Opt-in, transient, no formal save writes.
class UDWOEnemyAggroTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Subject;
 TArray<FString> Assets;
 bool Done=false;
 double Age=0;
 void Initialize(ADWOTowerGameMode Game)
 {
  Tower=Game;Tower.Hero.ControlProbe=true;Tower.Hero.AutoCast=false;Tower.Rules.SetPlayerGod(true);
  DWO3CAssets::Prepare("goblin_archer",Assets);UOpusAssets::Request(this,Assets);
 }
 bool Check(bool Good,FString Name)
 {
  UOpusNative::WriteLog((Good?"AGGRO_TEST_PASS ":"AGGRO_TEST_FAIL ")+Name);
  if(!Good){Done=true;UOpusNative::Quit(false);}return Good;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;
  Age+=Dt;if(Age>90){Check(false,"Timeout");return;}
  if(!Tower.Hero.Visual.Ready||!Tower.DropService.Ready||!UOpusAssets::Ready(this,Assets))return;
  auto Settings=Tower.Dungeon.MonsterSettings;
  if(!Check(Settings!=nullptr&&Math::Abs(Settings.AggroRadiusMeters-20.0)<0.001&&Math::Abs(Settings.PlayerSpawnExclusionRadiusMeters-10.0)<0.001,"SavedGlobalDataAsset aggro=20 spawn=10"))return;
  FVector Origin=Tower.Hero.CombatLocation();
  if(!Check(Tower.Dungeon.WithinMonsterAggroRange(Origin+FVector(2000,0,5000),Origin)&&!Tower.Dungeon.WithinMonsterAggroRange(Origin+FVector(2000.1,0,0),Origin),"HorizontalBoundary 2000cm"))return;
  FOpusMonsterData Data;for(auto D:UOpusNative::Monsters(Tower.Rules))if(D.Id==n"claw_beast"){Data=D;break;}Data.Health=1000000;
  Subject=Cast<ADWOEnemy>(SpawnActor(ADWOEnemy,Origin+FVector(-2100,0,25)));
  Subject.Rules=Tower.Rules;Subject.Tower=Tower;Subject.EntityId=++Tower.NextId;Subject.Boss=true;Subject.Speed=260;Subject.StopDistance=130;Subject.RuleTarget.RadiusCm=34;
  // A 30 m native skill would fire outside 20 m if the AI gate were missing.
  FOpusSkillData Nova;Nova.Id=n"monster_nova";Nova.Range=3000;Nova.Cooldown=0.2;Nova.Power=1;TArray<FOpusSkillData> Skills;Skills.Add(Nova);FString Error;
  if(!Check(UOpusNative::RegisterEnemy(Tower.Rules,Subject,Subject.EntityId,Data,n"monster_rarity_boss",1,34,"",Skills,Error),"RegisteredNativeEnemy "+Error))return;
  double BaseRange=Tower.Rules.GetMonsterSeekRange(Subject.EntityId,float32(Settings.AggroRadiusMeters));
  if(!Check(Math::Abs(BaseRange-20.0)<0.001,"AuthoredBase20m actual="+BaseRange))return;
  Subject.ConfigureAppearance("goblin_archer");Subject.SetActorTickEnabled(false);Subject.SetActorEnableCollision(false);Tower.Enemies.Add(Subject);Tower.Run.Start();Tower.SetPaused(false);
  int Before=Subject.AttackSerial;Subject.Chasing=true;Subject.DungeonPath.Add(Origin);Subject.RepathIn=42;Subject.AddMovementInput(FVector(1,0,0),1);
  for(int I=0;I<40;I++)Subject.AdvanceEnemy(0.05);
  if(!Check(Subject.AttackSerial==Before&&!Subject.Chasing&&Subject.DungeonPath.Num()==0&&Subject.GetPendingMovementInputVector().IsNearlyZero(),"OutsideRangeNoCastOrChase"))return;
  Subject.SetActorLocation(Origin+FVector(-1999,0,25));Subject.PreviousPosition=Subject.ActorLocation;
  Subject.AdvanceEnemy(0.05);
  if(!Check(Subject.AttackSerial>Before,"EnteringRangeCastsNativeSkill"))return;
  Subject.ActionRemaining=0;Subject.Play(n"idle",true);Subject.AdvanceEnemy(0.001);
  if(!Check(Subject.Chasing&&!Subject.GetPendingMovementInputVector().IsNearlyZero(),"EnteringRangeMovesTowardPlayer"))return;
  Before=Subject.AttackSerial;Subject.SetActorLocation(Origin+FVector(-2001,0,25));
  for(int I=0;I<40;I++)Subject.AdvanceEnemy(0.05);
  if(!Check(Subject.AttackSerial==Before&&!Subject.Chasing&&Subject.DungeonPath.Num()==0&&Subject.GetPendingMovementInputVector().IsNearlyZero(),"LeavingRangeClearsStaleTargetAndPath"))return;
  // All three roles use the same per-monster compiled trait pipeline.
  Subject.SetActorLocation(Origin+FVector(-600,0,25));
  if(!Check(Tower.Rules.SetMonsterTraits(Subject.EntityId,"[{\"id\":\"monster_seek_range\",\"params\":{\"n\":4}}]",Error),"SetRangeTrait4m "+Error))return;
  bool Shared=true;
  for(int Role=0;Role<3;Role++){Subject.Boss=Role==1;Subject.EventId=Role==2?"guard-probe":"";Subject.Chasing=true;Subject.AdvanceEnemy(0.05);Shared=Shared&&!Subject.Chasing&&Subject.AttackSerial==Before;}
  Subject.Boss=true;Subject.EventId="";
  if(!Check(Tower.Rules.SetMonsterTraits(Subject.EntityId,"[{\"id\":\"monster_seek_range\",\"params\":{\"n\":8}}]",Error),"SetRangeTrait8m "+Error))return;
  if(!Check(Shared,"RangeTraitControlsOrdinaryBossAndGuard"))return;
  for(int I=0;I<6;I++)Subject.AdvanceEnemy(0.05);
  if(!Check(Subject.AttackSerial>Before,"ReentryReacquiresWithoutResettingCooldown"))return;
  if(!Check(Tower.Rules.SetMonsterTraits(Subject.EntityId,"[{\"id\":\"monster_seek_range\",\"params\":{\"n\":0}}]",Error),"SetRangeTraitZero "+Error))return;
  if(!Check(!Tower.Dungeon.WithinMonsterAggroRange(Origin,Origin,Subject.EntityId),"ZeroRangeDisablesAtSamePosition"))return;
  Done=true;UOpusNative::WriteLog("AGGRO_TEST_PASS Complete");UOpusNative::Quit(true);
 }
}
