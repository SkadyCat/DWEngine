// Real CharacterMovement, capsule collisions and RVO in a blocked corridor.
// The obstruction is removed without changing the fighter's movement command.
class AThreeCProbeBarrier : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) UStaticMeshComponent Body;
 default Body.SetCollisionProfileName(n"BlockAll");
 void Place(FVector At,FVector Scale)
 {
  Body.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cube.Cube")));
  SetActorLocation(At);Body.SetRelativeScale3D(Scale);
 }
}
class AThreeCBlockProbe : AActor
{
 UPROPERTY() AThreeCGameMode Arena;
 UPROPERTY() AThreeCFighter Selected;
 UPROPERTY() AThreeCFighter Enemy;
 UPROPERTY() AThreeCFighter Blocker;
 UPROPERTY() AThreeCProbeBarrier LeftWall;
 UPROPERTY() AThreeCProbeBarrier RightWall;
 UPROPERTY() AThreeCProbeBarrier Gate;
 TArray<FString> Models;
 int CaseIndex=0;
 double Clock=0;
 bool Released=false;
 FString CSV="model,obstacle,guarded,time,state,blocked,block_entries,x,y,yaw,speed,play_rate,anim_starts,anim_time,release_x\n";
 double ReleaseX=0;
 void Setup(AThreeCGameMode InArena)
 {
  Arena=InArena;Arena.Paused=false;Arena.Clock=3;Arena.PoseModel="";Arena.AutoCamera=false;Arena.CameraMode=1;
  SetTickGroup(ETickingGroup::TG_PostUpdateWork);
  Models.Add("cloth_grunt");Models.Add("goblin_archer");Models.Add("swordsman");
  for(auto F:Arena.Fighters)
  {
   F.StopTravel();F.Probing=true;F.SetActorTickEnabled(false);F.Mesh.SetComponentTickEnabled(false);F.SetActorHiddenInGame(true);F.SetActorEnableCollision(false);
   F.CharacterMovement.SetAvoidanceEnabled(false);F.CharacterMovement.SetMovementMode(EMovementMode::MOVE_None);
  }
  LeftWall=Cast<AThreeCProbeBarrier>(SpawnActor(AThreeCProbeBarrier));LeftWall.Place(FVector(250,-95,100),FVector(12,0.3,2));
  RightWall=Cast<AThreeCProbeBarrier>(SpawnActor(AThreeCProbeBarrier));RightWall.Place(FVector(250,95,100),FVector(12,0.3,2));
  Gate=Cast<AThreeCProbeBarrier>(SpawnActor(AThreeCProbeBarrier));Gate.Place(FVector(245,0,100),FVector(0.3,1.6,2));
  BeginCase();
 }
 void BeginCase()
 {
  Arena.Clock=3;
  if(Selected!=nullptr)
  {
   Selected.StopTravel();Selected.Probing=true;Selected.SetActorTickEnabled(false);Selected.Mesh.SetComponentTickEnabled(false);Selected.SetActorHiddenInGame(true);Selected.SetActorEnableCollision(false);Selected.CharacterMovement.SetAvoidanceEnabled(false);
  }
  if(Enemy!=nullptr){Enemy.SetActorEnableCollision(false);Enemy.SetActorHiddenInGame(true);}
  if(Blocker!=nullptr){Blocker.SetActorEnableCollision(false);Blocker.SetActorHiddenInGame(true);}
  FString Model=Models[CaseIndex/4];for(auto F:Arena.Fighters)if(F.ModelId==Model){Selected=F;break;}
  Enemy=nullptr;Blocker=nullptr;
  for(auto F:Arena.Fighters)if(F!=Selected){if(Enemy==nullptr&&F.Team!=Selected.Team)Enemy=F;else if(Blocker==nullptr)Blocker=F;if(Enemy!=nullptr&&Blocker!=nullptr)break;}
  Selected.SetActorEnableCollision(true);Selected.SetActorHiddenInGame(false);Selected.SetActorTickEnabled(true);Selected.Mesh.SetComponentTickEnabled(true);Selected.Probing=false;
  Selected.Reset();Selected.SetActorLocation(FVector(0,0,85));Selected.PreviousPosition=Selected.ActorLocation;Selected.SetActorRotation(FRotator::ZeroRotator);Selected.PreviousYaw=0;
  Selected.BlockGuard=CaseIndex%2==1;Selected.BlockedEntries=0;Selected.ClearTravelGuard();Selected.CharacterMovement.SetAvoidanceEnabled(true);
  Selected.GaitSync=true;Selected.Range=155;Selected.Speed=260;Selected.CharacterMovement.MaxWalkSpeed=260;Selected.AttackCooldown=1000;Selected.TargetClock=1000;Selected.Health=1000000;
  Selected.Target=Enemy;Selected.TargetHeldSince=Arena.Clock;Selected.AnimationStarts=0;
  Enemy.StopTravel();Enemy.CharacterMovement.SetMovementMode(EMovementMode::MOVE_None);Enemy.SetActorLocation(FVector(1050,0,85));Enemy.SetActorHiddenInGame(false);Enemy.SetActorEnableCollision(true);Enemy.Dead=false;Enemy.Health=1000000;
  Blocker.StopTravel();Blocker.CharacterMovement.SetMovementMode(EMovementMode::MOVE_None);Blocker.SetActorLocation(FVector(245,0,85));Blocker.SetActorHiddenInGame((CaseIndex/2)%2==0);Blocker.SetActorEnableCollision((CaseIndex/2)%2==1);
  Gate.SetActorEnableCollision((CaseIndex/2)%2==0);Gate.SetActorHiddenInGame((CaseIndex/2)%2==1);
  Clock=0;Released=false;ReleaseX=0;Arena.Focus=Selected.Index;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Arena==nullptr)return;Clock+=DeltaSeconds;
  if((CaseIndex/2)%2==1&&!Released)Blocker.SetActorLocation(FVector(245,Math::Sin(Clock*6)*12,85));
  if(!Released&&Clock>=6)
  {
   Released=true;ReleaseX=Selected.ActorLocation.X;Gate.SetActorEnableCollision(false);Gate.SetActorHiddenInGame(true);Blocker.SetActorEnableCollision(false);Blocker.SetActorHiddenInGame(true);
  }
  CSV+=Selected.ModelId+","+((CaseIndex/2)%2==0?"wall":"crowd")+","+(Selected.BlockGuard?1:0)+","+Clock+","+Selected.State.ToString()+","+(Selected.Blocked?1:0)+","+Selected.BlockedEntries+","+Selected.ActorLocation.X+","+Selected.ActorLocation.Y+","+Selected.ActorRotation.Yaw+","+Selected.ActualSpeed+","+Selected.ActionRate+","+Selected.AnimationStarts+","+Selected.Mesh.GetPosition()+","+ReleaseX+"\n";
  if(Clock>=9.5)
  {
   CaseIndex++;
   if(CaseIndex<Models.Num()*4)BeginCase();else
   {
    FString Output=FPaths::ProjectSavedDir()+"Diagnostics/"+Arena.Tag+"-block-probe.csv";
    FFileHelper::SaveStringToFile(CSV,Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Log("C3_BLOCK_PROBE_PASS models="+Models.Num()+" cases="+CaseIndex+" output="+Output);
    System::QuitGame(Gameplay::GetPlayerController(0),EQuitPreference::Quit,false);SetActorTickEnabled(false);
   }
  }
 }
}
