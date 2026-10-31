// Scene-only cold-load probe separates PCG/navigation from character/UI startup.
class ADWODungeonProbeMode : AGameModeBase
{
 default DefaultPawnClass=ADefaultPawn;
 double Clock=0,NextLog=0;
 bool Rebuilt=false;
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;
  if(!Rebuilt&&Clock>1){Rebuilt=true;System::ExecuteConsoleCommand("RebuildNavigation");}
  if(Clock>=NextLog){NextLog=Clock+2;auto Gen=UOpusDungeon::Generator(this);FVector Point;bool Projected=UOpusDungeon::Project(this,Gen.GetPlayerStartLocation(),Point);UOpusNative::WriteLog("DUNGEON_PROBE tick="+Clock+" instances="+UOpusDungeon::InstanceCount(this)+" generating="+Gen.IsGenerationInProgress()+" projected="+Projected);}
  if(UOpusDungeon::Ready(this))
  {
   auto Points=UOpusDungeon::WalkablePoints(this,95);
   UOpusNative::WriteLog("DUNGEON_PROBE_READY points="+Points.Num());UOpusNative::Quit(Points.Num()>=20);
  }
  if(Clock>90)UOpusNative::Quit(false);
 }
}
// Transient integration fixture: real scene, navigation, AS waves and native combat.
class UDWODungeonTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 TArray<FVector> Before;
 FVector Home,WallStart,WallDirection;
 double WallDistance=0,CastIn=0;
 double Clock=0;
 int Stage=0;
 bool Check(bool Good,FString What)
 {
  UOpusNative::WriteLog((Good?"DUNGEON_TEST_PASS ":"DUNGEON_TEST_FAIL ")+What);
  if(!Good)UOpusNative::Quit(false);return Good;
 }
 void Initialize(ADWOTowerGameMode Game){Tower=Game;}
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Tower==nullptr||Tower.Dungeon==nullptr||!Tower.Dungeon.Ready)return;
  if(!Tower.Hero.Visual.Ready)return;
  Clock+=DeltaSeconds;if(Clock>90){Check(false,"timeout stage="+Stage);return;}
  if(Stage==0)
  {
   if(UDWOTowerJourney::Get().Visit>1)
   {
    if(!Check(Tower.Run.Floor==2&&UOpusDungeon::InstanceCount(Tower)==7448,"FreshLevelTravel floor=2"))return;
    Check(true,"Integration");UOpusNative::Quit(true);return;
   }
   if(!Tower.Hero.Visual.Ready)return;
   if(!Check(Tower.Dungeon.Generator.GetRooms().Num()==11&&UOpusDungeon::InstanceCount(Tower)==7448,"ZeroGeometry rooms=11 instances=7448"))return;
   if(!Check(Tower.Run.Phase==EDWOTowerPhase::Ready&&!Tower.CanFight(),"ManualReady"))return;
   int Good=0,Bends=0,Blocked=0;
   FVector Start=Tower.Hero.ActorLocation;
   for(auto P:Tower.Dungeon.Points)
   {
    auto Route=UOpusDungeon::Path(this,Start,P);if(Route.Num()>1)Good++;
    if(Route.Num()>2)Bends++;
    if(UOpusDungeon::Sweep(this,Start-FVector(0,0,40),P,10)>=0)Blocked++;
   }
   if(!Check(Good==Tower.Dungeon.Points.Num()&&Good>=20&&Bends>0&&Blocked>0,"ConnectedNavigation points="+Good+" bends="+Bends+" blocked="+Blocked))return;
   bool WallStopped=false;
   for(auto P:Tower.Dungeon.Points)
   {
    FVector Origin=Start-FVector(0,0,40),Delta=P-Origin;
    if(UOpusDungeon::Sweep(this,Origin,P,10)<0)continue;
    FVector End=UOpusDungeon::Move(this,Origin,Delta,45);
    if((End-P).Size2D()>50){WallStopped=true;break;}
   }
   if(!Check(WallStopped,"CollisionAndProjectileWalls"))return;
   auto Dungeon=Tower.Dungeon;FVector Spawn=Dungeon.PlayerSpawnLocation;
   if(!Check(Dungeon.MonsterSettings!=nullptr&&Math::Abs(Dungeon.MonsterSettings.PlayerSpawnExclusionRadiusMeters-10.0)<0.001,"SpawnExclusionConfig meters=10"))return;
   if(!Check(!Dungeon.OutsidePlayerSpawnExclusion(Spawn+FVector(999,0,0))&&!Dungeon.OutsidePlayerSpawnExclusion(Spawn+FVector(0,0,5000))&&Dungeon.OutsidePlayerSpawnExclusion(Spawn+FVector(1000,0,0)),"SpawnExclusionBoundary horizontal=1000cm"))return;
   Tower.Dungeon.Reserved.Empty();bool Density=true,Safe=true;double MinSpawnDistance=1e30;
   for(int I=0;I<300;I++)
   {
    FVector P;if(!Dungeon.SpawnPoint(I,I==299,P)){Density=false;break;}
    double Distance=(P-Spawn).Size2D();MinSpawnDistance=Math::Min(MinSpawnDistance,Distance);if(Distance<1000)Safe=false;
   }
   Tower.Dungeon.Reserved.Empty();
   if(!Check(Density,"HighDensitySpawns count=300"))return;
   if(!Check(Safe,"SpawnExclusionAllMonsters count=300 minCm="+MinSpawnDistance))return;
   // Force the remote-candidate fallback after moving far from the original start.
   FVector PlayerBefore=Tower.Hero.ActorLocation;Tower.Hero.SetActorLocation(Spawn+FVector(100000,100000,0));
   FVector Remote;bool RemoteFound=Dungeon.SpawnPoint(0,true,Remote);Dungeon.Reserved.Empty();
   auto RealPoints=Dungeon.Points;Dungeon.Points.Empty();Dungeon.Points.Add(Spawn+FVector(500,0,0));
   FVector Forbidden;bool ForbiddenFound=Dungeon.SpawnPoint(0,false,Forbidden);Dungeon.Reserved.Empty();Dungeon.Points=RealPoints;
   Tower.Hero.SetActorLocation(PlayerBefore);
   if(!Check(RemoteFound&&(Remote-Spawn).Size2D()>=1000&&!ForbiddenFound,"SpawnExclusionFallbackKeepsOriginalStart"))return;
   // Event guards must use the same fixed exclusion as ordinary monsters/bosses.
   auto PreviousPhase=Tower.Run.Phase;Tower.Run.Phase=EDWOTowerPhase::Cleared;
   Dungeon.Points.Empty();Dungeon.Points.Add(Spawn+FVector(500,0,0));
   bool GuardInExclusion=Tower.SpawnEventGuards("spawn-safety-test",1,997,Spawn);
   Dungeon.Points=RealPoints;Tower.Run.Phase=PreviousPhase;Tower.LastError="";
   if(!Check(!GuardInExclusion,"SpawnExclusionEventGuards"))return;
   if(UOpusNative::Flag("DWOSpawnSafetyOnly")){Check(true,"SpawnSafety");UOpusNative::Quit(true);return;}
   Home=Tower.Hero.ActorLocation;
   TArray<FVector> Directions;Directions.Add(FVector(1,0,0));Directions.Add(FVector(-1,0,0));Directions.Add(FVector(0,1,0));Directions.Add(FVector(0,-1,0));
   bool Found=false;
   for(auto P:Tower.Dungeon.Points)
   {
    for(auto D:Directions)
    {
     double T=UOpusDungeon::Sweep(this,P,P+D*400,34);
     if(T>0.15&&T<0.85){WallStart=P+FVector(0,0,41);WallDirection=D;WallDistance=T*400;Found=true;break;}
    }
    if(Found)break;
   }
   if(!Check(Found,"PlayerWallFixture"))return;
   Tower.Hero.ControlProbe=true;Tower.Hero.SetActorLocation(WallStart);Stage=10;Clock=0;return;
  }
  if(Stage==10)
  {
   Tower.Hero.TopDown.Move(WallDirection,1,DeltaSeconds);
   if(Clock<1.4)return;
   double Distance=(Tower.Hero.ActorLocation-WallStart).DotProduct(WallDirection);
   if(!Check(Distance>10&&Distance<WallDistance+90&&Home.Y>1500,"PlayerWallsBeyondOldBounds distance="+Distance))return;
   Tower.Hero.TopDown.Stop();Tower.Hero.ControlProbe=false;Tower.Hero.SetActorLocation(Home);Tower.Hero.TopDown.CameraFollow.ResetAfterTeleport();
   Tower.StartChallenge();Stage=1;Clock=0;return;
  }
  if(Stage==1)
  {
   if(Tower.Preparing||Tower.Enemies.Num()==0)return;
   int Good=0;
   for(auto E:Tower.Enemies){if(UOpusDungeon::Path(this,E.ActorLocation,Tower.Hero.ActorLocation).Num()>1)Good++;Before.Add(E.ActorLocation);}
   int Bosses=0;for(auto E:Tower.Enemies)if(E.Boss)Bosses++;
   if(!Check(Good==Tower.Run.EncounterCount()&&Bosses==1,"NativeDungeonSpawns count="+Good+" bosses="+Bosses))return;
   Stage=2;Clock=0;return;
  }
  if(Stage==2&&Clock>3)
  {
   int Moved=0,InRange=0,Dormant=0;FString Stationary;
   for(int I=0;I<Before.Num();I++)if(IsValid(Tower.Enemies[I]))
   {
    auto E=Tower.Enemies[I];double Distance=(E.ActorLocation-Tower.Hero.CombatLocation()).Size2D();
    if(!Tower.Dungeon.WithinMonsterAggroRange(Before[I],Tower.Hero.CombatLocation(),E.EntityId)&&(Before[I]-E.ActorLocation).Size2D()<1&&!E.Chasing)Dormant++;
    else if((Before[I]-E.ActorLocation).Size2D()>40)Moved++;
    else if(Distance<=E.StopDistance+5)InRange++;
    else Stationary+=" id="+E.EntityId+" boss="+E.Boss+" distance="+Distance+" stop="+E.StopDistance+" before="+Before[I]+" after="+E.ActorLocation;
   }
   if(!Check(Moved+InRange+Dormant==Before.Num(),"MonstersRespectAggroRange moved="+Moved+" alreadyInRange="+InRange+" dormant="+Dormant+Stationary))return;
   auto Target=Tower.Enemies[0];FVector P;
   for(auto Candidate:Tower.Dungeon.Points)if((Candidate-Target.ActorLocation).Size2D()>200&&(Candidate-Target.ActorLocation).Size2D()<700&&UOpusDungeon::Sweep(this,Candidate,Target.ActorLocation,1)<0){P=Candidate;break;}
   if(!P.IsNearlyZero()){Tower.Hero.SetActorLocation(P+FVector(0,0,41));Tower.Hero.TopDown.CameraFollow.ResetAfterTeleport();}
   Tower.Hero.AutoCast=false;Tower.Hero.ControlProbe=true;Stage=3;Clock=0;return;
  }
  if(Stage==3)
  {
   if(Tower.Preparing)return;
   ADWOEnemy Target;double Best=1e30;
   for(auto E:Tower.Enemies)if(IsValid(E)&&!E.Dead){double D=(E.ActorLocation-Tower.Hero.ActorLocation).SizeSquared2D();if(D<Best){Best=D;Target=E;}}
   if(Target!=nullptr)
   {
    auto Hero=Tower.Hero;bool Clear=UOpusDungeon::Sweep(this,Hero.CombatLocation(),Target.ActorLocation,1)<0;
    if(Math::Sqrt(Best)>400||!Clear)
    {
     auto Route=UOpusDungeon::Path(this,Hero.ActorLocation,Target.ActorLocation);int At=1;
     while(At<Route.Num()&&(Route[At]-Hero.ActorLocation).Size2D()<60)At++;
     if(At<Route.Num())Hero.TopDown.Move((Route[At]-Hero.ActorLocation).GetSafeNormal2D(),Tower.Rules.GetPlayerPools().MoveScale,DeltaSeconds);else Hero.TopDown.Stop();
    }
    else Hero.TopDown.Stop();
    Hero.TopDown.AimAt(Target.ActorLocation);CastIn-=DeltaSeconds;
    if(Clear&&CastIn<=0){CastIn=1.0/3;Hero.CastAt(Hero.CombatLocation()+Hero.Aim*60,Target.ActorLocation);}
   }
   if(Tower.Kills>0&&UOpusNative::Loot(Tower.Rules).Num()>0)
   {
    Tower.Hero.TopDown.Stop();Tower.Hero.ControlProbe=false;
    if(!Check(true,"NativeCombatAndLoot kills="+Tower.Kills+" drops="+UOpusNative::Loot(Tower.Rules).Num()))return;
    Stage=4;Clock=0;
    FString Capture=UOpusNative::StringOption("DWODungeonCapture");
    if(!Capture.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Capture+" -nosuffix");
   }
  }
  if(Stage==4&&Clock>1){Stage=5;Tower.ChallengeFloor(2);}
 }
}
