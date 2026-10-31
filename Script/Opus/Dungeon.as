// Zero's geometry-only ClaudePCG scene, hosted by Opus's existing tower game.
// No source GameMode, UI, encounter, account or combat code is imported.
class UDWODungeon : UObject
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADungeonGenerator Generator;
 UPROPERTY() UDWOMonsterSettings MonsterSettings;
 FVector PlayerSpawnLocation;
 TArray<FVector> Points;
 TArray<FVector> Reserved;
 bool Ready=false,StartWhenReady=false,NavigationRequested=false;
 double Waiting=0,NextWaitLog=5;
 bool InitializeSettings(ADWOTowerGameMode Game)
 {
  Tower=Game;
  MonsterSettings=Cast<UDWOMonsterSettings>(LoadObject(nullptr,"/Game/Monsters/Config/DA_GlobalMonsterSettings.DA_GlobalMonsterSettings"));
  if(MonsterSettings==nullptr){Tower.Fail("Missing /Game/Monsters/Config/DA_GlobalMonsterSettings");return false;}
  return true;
 }
 void Initialize(ADWOTowerGameMode Game)
 {
  Tower=Game;Generator=UOpusDungeon::Generator(Tower);
  if(Generator==nullptr){Tower.Fail("Missing Zero dungeon generator in L_OpusDungeon");return;}
  Tower.Hero.SetActorLocation(Generator.GetPlayerStartLocation()+FVector(0,0,101));
  PlayerSpawnLocation=Tower.Hero.ActorLocation;
 }
 void Update(double Dt)
 {
  if(Ready||Generator==nullptr)return;
  Waiting+=Dt;
  // Baked PCG instances register during world startup. Request navigation once
  // those components exist, including on fresh load and floor travel.
  if(!NavigationRequested&&Waiting>=1&&!Generator.IsGenerationInProgress()){NavigationRequested=true;System::ExecuteConsoleCommand("RebuildNavigation");}
  if(Waiting>=NextWaitLog){NextWaitLog=Waiting+5;FVector Probe;bool Projected=UOpusDungeon::Project(Tower,Generator.GetPlayerStartLocation(),Probe);UOpusNative::WriteLog("DUNGEON_WAIT time="+Waiting+" generating="+Generator.IsGenerationInProgress()+" projected="+Projected);}
  if(!UOpusDungeon::Ready(Tower)){if(Waiting>180)Tower.Fail("Dungeon geometry/navigation did not become ready");return;}
  Points=UOpusDungeon::WalkablePoints(Tower,95);
  if(Points.Num()<20){Tower.Fail("Dungeon has too few connected spawn points: "+Points.Num());return;}
  // Cell centers alone cover only the old <=100-monster waves. Add half-cell
  // positions reached by a full-radius capsule sweep from an accepted center.
  // Keep the same 95 cm wall clearance and 190 cm spacing for a complete map.
  TArray<FVector> Centers=Points,Offsets;
  Offsets.Add(FVector(200,0,0));Offsets.Add(FVector(-200,0,0));Offsets.Add(FVector(0,200,0));Offsets.Add(FVector(0,-200,0));
  for(auto Center:Centers)for(auto Offset:Offsets)
  {
   FVector Projected;if(!UOpusDungeon::Project(Tower,Center+Offset,Projected))continue;
   FVector Candidate=Projected+FVector(0,0,60);if((Candidate-Center-Offset).Size2D()>20)continue;
   bool Separate=true;for(auto Existing:Points)if((Existing-Candidate).Size2D()<190){Separate=false;break;}
   if(!Separate||!UOpusDungeon::Move(Tower,Center,Candidate-Center,95).Equals(Candidate,1))continue;
   Points.Add(Candidate);
  }
  FVector Ground;
  if(!UOpusDungeon::Project(Tower,Generator.GetPlayerStartLocation(),Ground)){Tower.Fail("Dungeon start is not navigable");return;}
  Tower.Hero.SetActorLocation(Ground+FVector(0,0,101));
  PlayerSpawnLocation=Tower.Hero.ActorLocation;
  if(Tower.Hero.TopDown.CameraFollow!=nullptr)Tower.Hero.TopDown.CameraFollow.ResetAfterTeleport();
  Ready=true;
  UOpusNative::WriteLog("DUNGEON_READY source=Zero rooms="+Generator.GetRooms().Num()+" points="+Points.Num()+" instances="+UOpusDungeon::InstanceCount(Tower)+" start="+PlayerSpawnLocation+" spawnExclusionMeters="+MonsterSettings.PlayerSpawnExclusionRadiusMeters+" aggroMeters="+MonsterSettings.AggroRadiusMeters);
  if(StartWhenReady){StartWhenReady=false;Tower.StartChallenge();}
 }
 bool SpawnPoint(int Index,bool Boss,FVector& Position)
 {
  if(!Ready)return false;
  if(Boss)
  {
   // Studio's boss belongs to the deepest reachable room. Compare actual
   // navigation routes so a nearby room across a wall is not mistaken for an end.
   double Longest=-1;FVector End;
   for(auto Room:UOpusDungeon::MapRooms(Tower))
   {
    FVector Center=FVector((Room.X+Room.Z/2)*100,(Room.Y+Room.W/2)*100,0),Point;double Nearest=1e20;
    for(auto P:Points)if(P.X>=Room.X*100&&P.X<(Room.X+Room.Z)*100&&P.Y>=Room.Y*100&&P.Y<(Room.Y+Room.W)*100&&OutsidePlayerSpawnExclusion(P)&&Available(P))
    {double D=(P-Center).SizeSquared2D();if(D<Nearest){Nearest=D;Point=P;}}
    if(Nearest==1e20)continue;auto Route=UOpusDungeon::Path(Tower,PlayerSpawnLocation,Point);if(Route.Num()<2)continue;double Length=0;for(int I=1;I<Route.Num();I++)Length+=(Route[I]-Route[I-1]).Size2D();
    if(Length>Longest){Longest=Length;End=Point;}
   }
   if(Longest>=0){Position=End;Reserved.Add(End);return true;}
  }
  TArray<FVector> Candidates;
  // The spawn exclusion stays at the floor's player start, even after moving.
  // Keep the existing 4 m clearance from the current player as well.
  for(auto P:Points){double D=(P-Tower.Hero.ActorLocation).Size2D();if(D>=400&&D<=2600&&OutsidePlayerSpawnExclusion(P)&&Available(P))Candidates.Add(P);}
  if(Candidates.Num()==0)for(auto P:Points)if((P-Tower.Hero.ActorLocation).Size2D()>=400&&OutsidePlayerSpawnExclusion(P)&&Available(P))Candidates.Add(P);
  if(Candidates.Num()==0)return false;
  int At=Tower.Random.RandRange(0,Candidates.Num()-1);Position=Candidates[At];Reserved.Add(Position);
  return true;
 }
 bool OutsidePlayerSpawnExclusion(FVector Point) const
 {
  if(MonsterSettings==nullptr)return false;
  double RadiusCm=Math::Max(0.0,MonsterSettings.PlayerSpawnExclusionRadiusMeters)*100.0;
  return (Point-PlayerSpawnLocation).SizeSquared2D()>=RadiusCm*RadiusCm;
 }
 bool WithinMonsterAggroRange(FVector Monster,FVector Player,int EntityId=0) const
 {
  if(MonsterSettings==nullptr)return false;
  double Meters=EntityId==0?MonsterSettings.AggroRadiusMeters:Tower.Rules.GetMonsterSeekRange(EntityId,float32(MonsterSettings.AggroRadiusMeters));
  double RadiusCm=Math::Max(0.0,Meters)*100.0;
  return RadiusCm>0&&(Player-Monster).SizeSquared2D()<=RadiusCm*RadiusCm;
 }
 bool Available(FVector Point) const {for(auto P:Reserved)if((P-Point).Size2D()<190)return false;return true;}
}
