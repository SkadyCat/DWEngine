// Transient geometry checks use the shipping generated dungeon and physical collision.
class ADWOGroundProbeFloor : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) UBoxComponent Floor;
 default Floor.SetBoxExtent(FVector(200,200,10));
 default Floor.SetCollisionProfileName(n"BlockAll");
}
class ADWODungeonGroundProbeMode : AGameModeBase
{
 default DefaultPawnClass=ASpectatorPawn;
 double Clock=0;
 bool Built=false;
 bool Check(bool Good,FString Detail){UOpusNative::WriteLog((Good?"DUNGEON_GROUND_PASS ":"DUNGEON_GROUND_FAIL ")+Detail);if(!Good)UOpusNative::Quit(false);return Good;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;if(Clock>90){Check(false,"timeout");return;}
  if(!Built&&Clock>1){Built=true;System::ExecuteConsoleCommand("RebuildNavigation");}
  if(!UOpusDungeon::Ready(this))return;
  auto Points=UOpusDungeon::WalkablePoints(this,95);int Good=0;double MinZ=100000,MaxZ=-100000;
  for(auto P:Points){FVector Floor,Normal;if(UOpusDungeon::Surface(this,P,Floor,Normal)&&Normal.Z>.5&&Floor.Z<P.Z&&P.Z-Floor.Z<200){Good++;MinZ=Math::Min(MinZ,Floor.Z);MaxZ=Math::Max(MaxZ,Floor.Z);}}
  if(!Check(Good==Points.Num()&&Good>=20,"actual dungeon surfaces="+Good+" floorZ="+MinZ+".."+MaxZ))return;
  auto Fixture=SpawnActor(ADWOGroundProbeFloor,FVector(100000,100000,300));FVector Raised,Normal;
  bool HasRaised=UOpusDungeon::Surface(this,FVector(100000,100000,370),Raised,Normal);
  if(!Check(HasRaised&&Math::Abs(Raised.Z-310)<.1&&Normal.Z>.99,"raised floor="+Raised))return;
  if(!Check(!UOpusDungeon::Surface(this,FVector(101000,101000,370),Raised,Normal),"void has no physical surface"))return;
  UOpusNative::WriteLog("DUNGEON_GROUND_DONE");UOpusNative::Quit(true);
 }
}
