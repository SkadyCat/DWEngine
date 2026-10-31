// A real rendered character/CMC/MM integration probe, isolated from saves.
class UDWOHeroTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0,Frames=0,DirectionIndex=0;
 double Clock=0,Total=0,Travel=0,NextDiagnostic=5;
 FVector Start,Hand,Home,ProbeHome;
 UPROPERTY() AActor ProbeRoom;
 UPROPERTY() ADWOEnemy InvalidatedTarget;
 UPROPERTY() ADWOEnemy LastHeldTarget;
 FString Profile;
 bool Done=false,HitSeen=false;
 int BeforeCasts=0,BeforeReleases=0,BeforeRequests=0;
 double BeforeMana=0;
 void Initialize(ADWOTowerGameMode T)
 {
  Tower=T;SetTickGroup(ETickingGroup::TG_PostUpdateWork);Tower.Hero.ControlProbe=true;Tower.Hero.AutoCast=false;Tower.Rules.SetPlayerGod(true);
 }
 bool Check(bool Good,FString Message)
 {
  if(!Good){Done=true;UOpusNative::WriteLog("HERO_TEST_FAIL stage="+Stage+" "+Message);UOpusNative::Quit(false);}return Good;
 }
 void Next(int Value){Stage=Value;Clock=0;Frames=0;}
 void Capture(FString Name){FString Folder=UOpusNative::StringOption("DWOHeroCaptures");if(!Folder.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Folder+"/"+Name+".png -nosuffix");}
 bool CheckCamera()
 {
  auto H=Tower.Hero;auto C=H.TopDown;auto PC=Gameplay::GetPlayerController(0);FVector At=H.ActorLocation;FVector First;
  for(int Pass=0;Pass<3;Pass++)
  {
   int Rate=Pass==0?30:Pass==1?60:120;H.SetActorLocation(At);C.CameraFollow.ResetAfterTeleport();
   for(int I=1;I<=Rate;I++){H.SetActorLocation(At+FVector(300.0*I/Rate,0,0));C.CameraFollow.AdvanceFollow(1.0/Rate);}
   FVector Followed=H.CameraBoom.GetWorldLocation()+H.CameraBoom.TargetOffset;
   if(Pass==0)First=Followed;else if(!Check((Followed-First).Size()<0.001,"original follow is consistent at 30/60/120 FPS"))return false;
  }
  H.SetActorLocation(At);C.CameraFollow.ResetAfterTeleport();
  if(!UOpusNative::StringOption("DWOHeroCaptures").IsEmpty())
  {
   for(int I=0;I<4;I++)
   {
    C.Face(I*90);FVector2D Center,Up,Right;
    bool Projected=PC.ProjectWorldLocationToScreen(At,Center)&&PC.ProjectWorldLocationToScreen(At+C.MoveDirection(1,0)*200,Up)&&PC.ProjectWorldLocationToScreen(At+C.MoveDirection(0,1)*200,Right);
    if(!Check(Projected&&Up.Y<Center.Y&&Right.X>Center.X&&Math::Abs(H.Camera.GetWorldRotation().Yaw)<0.01,"WASD stays screen-relative through four actor facings"))return false;
   }
  }
  C.Face(0);UOpusNative::WriteLog("HERO_TEST_PASS ZeroCameraFollow");return true;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Done||Tower==nullptr)return;Total+=DeltaSeconds;Clock+=DeltaSeconds;
  if(!Check(Total<115,"timeout"))return;
  auto H=Tower.Hero;auto V=H.Visual;auto C=H.TopDown;
  if(Total>=NextDiagnostic){NextDiagnostic=Total+10;UOpusNative::WriteLog("HERO_TEST_STATE stage="+Stage+" visualReady="+V.Ready+" loading="+V.LoadingTime+" heroTick="+H.IsActorTickEnabled()+" visualTick="+V.IsComponentTickEnabled()+" towerTick="+Tower.IsActorTickEnabled()+" travel="+Tower.Traveling+" dungeonWait="+Tower.Dungeon.Waiting);}
  if(!Check(V.Failure.IsEmpty(),V.Failure)||!V.Ready)return;
  if(UOpusNative::Flag("DWOHeroCloseup"))C.Distance=1400;
  if(Stage==0)
  {
   if(Tower.Dungeon==nullptr||!Tower.Dungeon.Ready)return;
   if(ProbeRoom==nullptr)
   {
    Home=H.ActorLocation;ProbeHome=FVector(100000,0,100);ProbeRoom=SpawnActor(ADWOHeroProbeRoom,FVector(100000,0,0));
    auto Room=Cast<ADWOHeroProbeRoom>(ProbeRoom);Room.Sun.SetVisibility(false);Room.Fill.SetVisibility(false);
    H.SetActorLocation(ProbeHome);C.CameraFollow.ResetAfterTeleport();Frames=0;return;
   }
   if(++Frames<15)return;
   if(!Check(Cast<UCleanNetworkMovement>(H.CharacterMovement)!=nullptr&&V.Anim!=nullptr&&V.Anim.UpdateCount>0,"actual Zero CMC and motion matching graph"))return;
   if(!Check(H.Wizard.GetNumBones()>40&&H.Wizard.GetAttachParent()==H.Mesh&&H.Mesh.GetAnimInstance()==V.Anim&&!H.Body.IsVisible(),"original graph drives attached-parent wizard"))return;
   if(!Check(IsValid(H.CombatTarget)&&(H.CombatTarget.ActorLocation-H.CombatLocation()).Size()<0.01&&H.CombatTarget.RuleTarget.RadiusCm==45,"physical capsule keeps the original rule sphere and height"))return;
   if(!Check(DWO3CGeometry::Sweep(H.CombatLocation()+FVector(180,0,0),H.CombatLocation(),1)<0,"geometry visibility excludes the physical Pawn capsule"))return;
   if(!Check(H.CapsuleComponent.GetScaledCapsuleHalfHeight()==98&&Math::Abs(H.ActorLocation.Z-100)<3,"capsule is grounded"))return;
   if(!Check(H.CameraBoom.TargetArmLength==(UOpusNative::Flag("DWOHeroCloseup")?1400:2200)&&Math::Abs(H.Camera.GetWorldRotation().Pitch+60)<0.01&&C.CameraFollow!=nullptr,"original Zero fixed camera and follow"))return;
   if(!CheckCamera())return;
   if(UDWOTowerJourney::Get().Visit>1)
   {
    if(!Check(!Tower.Rules.GetPlayerPools().bDead&&Tower.Run.Phase==EDWOTowerPhase::Ready,"retry restores living Ready character"))return;
    UOpusNative::WriteLog("HERO_TEST_PASS RetryAndCache");UOpusNative::WriteLog("HERO_TEST_PASS RuntimeIntegration");Done=true;UOpusNative::Quit(true);return;
   }
   UOpusNative::WriteLog("HERO_TEST_PASS RigAndIdle graph="+V.Anim.GetClass().GetName());Profile=Tower.Rules.ExportPlayerState();Capture("zero-ready");Start=H.ActorLocation;
   if(UOpusNative::Flag("DWOHeroCombatOnly")){H.SetActorLocation(Home);C.Stop();C.CameraFollow.ResetAfterTeleport();ProbeRoom.DestroyActor();Tower.StartChallenge();Next(8);return;}
   C.Move(FVector(1,0,0),1,DeltaSeconds);Next(1);return;
  }
  if(Stage==1)
  {
   C.Move(FVector(1,0,0),1,DeltaSeconds);
   if(Clock<1)return;
   double Distance=(H.ActorLocation-Start).Size2D();
   if(!Check(Distance>350&&Distance<600&&Math::Abs(H.GetVelocity().Size2D()-500)<1,"CMC acceleration and original run speed distance="+Distance+" speed="+H.GetVelocity().Size2D()))return;
   if(!Check(V.Anim.TrajectorySamples>20&&!V.Anim.SelectedDatabase.IsEmpty()&&V.Anim.GroundSpeed>400,"real MM trajectory and moving pose selection"))return;
   if(!Check((H.CombatTarget.GetVelocity()-H.GetVelocity()).Size()<0.1,"native moving-player query receives actual CMC velocity"))return;
   UOpusNative::WriteLog("HERO_TEST_PASS LocomotionAndReadOnly distance="+Distance+" db="+V.Anim.SelectedDatabase);Capture("zero-run");C.Move(FVector::ZeroVector,1,DeltaSeconds);Next(2);return;
  }
  if(Stage==2)
  {
   C.Move(FVector::ZeroVector,1,DeltaSeconds);if(Clock<0.7)return;
   if(!Check(H.GetVelocity().Size2D()<0.1&&V.Anim.SelectedDatabase=="PSD_Idle","release brakes into original idle database db="+V.Anim.SelectedDatabase))return;
   if(!Check(Tower.Rules.ExportPlayerState()==Profile,"3C does not alter native rule state in Ready"))return;
   Hand=H.Wizard.GetSocketTransform(n"LeftHand",ERelativeTransformSpace::RTS_Component).GetLocation();V.StartAction(n"cast",4);
   if(!Check(V.ActionState==4&&V.RotationLocked(),"cast starts original saved montage and action lock"))return;
   Next(3);return;
  }
  if(Stage==3)
  {
   if(Clock<0.2)return;
   double Motion=(H.Wizard.GetSocketTransform(n"LeftHand",ERelativeTransformSpace::RTS_Component).GetLocation()-Hand).Size();
   if(!Check(Motion>1&&V.Anim.GetCleanActionProgress()>0&&V.Anim.GetCleanActionProgress()<0.6,"authored cast speed and actual retargeted pose"))return;
   Capture("zero-cast");UOpusNative::WriteLog("HERO_TEST_PASS CastPose handMotion="+Motion);
   if(!Check(V.MovementPressed(),"movement cancels cast using native inertialization"))return;
   Start=H.ActorLocation;C.Move(FVector(0,1,0),1,DeltaSeconds);Next(4);return;
  }
  if(Stage==4)
  {
   C.Move(FVector(0,1,0),1,DeltaSeconds);if(Clock<0.5)return;
   if(!Check((H.ActorLocation-Start).Size2D()>100&&V.ActionState==0,"cancelled cast resumes CMC movement"))return;
   C.Stop(true);Tower.SetPaused(true);Start=H.ActorLocation;Next(5);return;
  }
  if(Stage==5)
  {
   C.Stop(true);if(Clock<0.3)return;
   if(!Check((H.ActorLocation-Start).Size()<0.1&&H.GetVelocity().IsNearlyZero()&&H.Mesh.bPauseAnims&&C.WaitForRelease,"pause stops character and freezes original graph"))return;
   Tower.SetPaused(false);UOpusNative::WriteLog("HERO_TEST_PASS Pause");H.SetActorLocation(ProbeHome);C.Face(0);C.CameraFollow.ResetAfterTeleport();C.Move(FVector::ZeroVector,1,DeltaSeconds);Next(6);return;
  }
  if(Stage==6)
  {
   C.Move(FVector::ZeroVector,1,DeltaSeconds);
   if(Clock<0.3||!V.CanDodge())return;
   Start=H.ActorLocation;FVector D=FRotator(0,DirectionIndex*45,0).ForwardVector;V.Dodged(D);
   if(!Check(V.ActionState==1&&(H.ActorLocation-Start).IsNearlyZero(),"roll starts a montage without teleport direction="+DirectionIndex))return;
   Next(7);return;
  }
  if(Stage==7)
  {
   C.Move(FVector::ZeroVector,1,DeltaSeconds);
   if(Clock<0.15)return;
   if(V.ActionState==1&&Clock<3)return;
   FVector Delta=H.ActorLocation-Start;FVector D=FRotator(0,DirectionIndex*45,0).ForwardVector;
   if(!Check(Delta.Size2D()>20&&Delta.Size2D()<1400&&Delta.GetSafeNormal2D().DotProduct(D)>0.85,"original root-motion travel direction="+DirectionIndex+" delta="+Delta))return;
   UOpusNative::WriteLog("HERO_TEST_ROLL direction="+DirectionIndex+" travel="+Delta.Size2D());
   if(++DirectionIndex<8){H.SetActorLocation(ProbeHome);C.Face(0);C.CameraFollow.ResetAfterTeleport();Next(6);return;}
   if(!Check(Tower.Rules.ExportPlayerState()==Profile,"presentation-only roll cannot spend native resources"))return;
   UOpusNative::WriteLog("HERO_TEST_PASS RootMotionEightDirections");H.SetActorLocation(Home);C.Stop();C.CameraFollow.ResetAfterTeleport();ProbeRoom.DestroyActor();Tower.StartChallenge();Next(8);return;
  }
  if(Stage==8)
  {
   C.Move(FVector::ZeroVector,1,DeltaSeconds);if(Tower.Preparing||Clock<0.3)return;
   for(auto Enemy:Tower.Enemies)Enemy.SetActorTickEnabled(false);
   Tower.Rules.SetPlayerGod(false);BeforeCasts=V.CastSerial;BeforeReleases=V.AuthoredReleases;BeforeMana=Tower.Rules.GetPlayerPools().Mana;
   auto Enemy=Tower.Enemies[0];
   if(!Check(H.CastAt(Enemy.ActorLocation-FVector(180,0,0),Enemy.ActorLocation,30)&&V.PendingCast&&V.ActionState==4,"cast request starts the original wind-up"))return;
   if(!Check(V.CastSerial==BeforeCasts&&V.AuthoredReleases==BeforeReleases&&Tower.Rules.GetPlayerPools().Mana==BeforeMana,"request does not release or spend before the authored marker"))return;
   V.HandleActionEnd(V.Clips[n"cast"]);
   if(!Check(!V.CanRestartCast()&&!H.CastAt(H.CombatLocation(),Enemy.ActorLocation),"end-before-release and held requests cannot skip/restart the wind-up"))return;
   Next(20);return;
  }
  if(Stage==20)
  {
   if(Clock<0.1)return;
   if(!Check(V.PendingCast&&V.CastSerial==BeforeCasts&&Tower.Rules.GetPlayerPools().Mana==BeforeMana,"wind-up remains unpaid"))return;
   if(!Check(V.MovementPressed()&&!V.PendingCast,"movement retires the pending cast"))return;
   V.HandleAttackRelease(V.Clips[n"cast"]);Next(21);return;
  }
  if(Stage==21)
  {
   if(Clock<2)return;
   if(!Check(V.CastSerial==BeforeCasts&&V.AuthoredReleases==BeforeReleases&&Tower.Rules.GetPlayerPools().Mana==BeforeMana,"cancelled and stale notifies never release or spend"))return;
   UOpusNative::WriteLog("HERO_TEST_PASS CancelledWindupNoCast");
   InvalidatedTarget=Tower.Enemies[0];
   if(!Check(H.CastAt(InvalidatedTarget.ActorLocation-FVector(180,0,0),InvalidatedTarget.ActorLocation,30),"targeted cast begins before target death"))return;
   Next(24);return;
  }
  if(Stage==24)
  {
   if(Clock<0.1)return;if(!InvalidatedTarget.Dead)InvalidatedTarget.Die();if(V.PendingCast)return;
   if(!Check(!H.IsCastTargetValid(InvalidatedTarget)&&V.CastSerial==BeforeCasts+1&&V.AuthoredReleases==BeforeReleases+1&&Tower.Rules.GetPlayerPools().Mana<BeforeMana,"manual cast still releases at the chosen point after the aimed enemy dies"))return;
   Next(25);return;
  }
  if(Stage==25)
  {
   if(Clock<2)return;
   if(!Check(V.CastSerial==BeforeCasts+1&&V.AuthoredReleases==BeforeReleases+1,"target death does not cause a duplicate release"))return;
   UOpusNative::WriteLog("HERO_TEST_PASS ManualTargetDeathStillReleases");
   BeforeCasts=V.CastSerial;BeforeReleases=V.AuthoredReleases;BeforeMana=Tower.Rules.GetPlayerPools().Mana;
   ADWOEnemy Enemy;for(auto E:Tower.Enemies)if(H.IsCastTargetValid(E)){Enemy=E;break;}
   if(!Check(Enemy!=nullptr&&H.CastAt(Enemy.ActorLocation-FVector(180,0,0),Enemy.ActorLocation,30),"living target starts the next cast"))return;
   V.HandleAttackRelease(V.Clips[n"hit"]);
   if(!Check(V.PendingCast&&V.CastSerial==BeforeCasts,"a different sequence cannot release this cast"))return;
   Next(22);return;
  }
  if(Stage==22)
  {
   if(!Check(Clock<4,"source Attack Release notify must actually fire"))return;
   if(V.AuthoredReleases==BeforeReleases)return;
   if(!Check(Clock>0.1&&V.CastSerial==BeforeCasts+1&&V.AuthoredReleases==BeforeReleases+1&&!V.PendingCast&&Tower.Rules.GetPlayerPools().Mana<BeforeMana,"actual source notify commits one native cast and its cost after wind-up"))return;
   double PaidMana=Tower.Rules.GetPlayerPools().Mana;V.HandleAttackRelease(V.Clips[n"cast"]);
   if(!Check(V.CastSerial==BeforeCasts+1&&Tower.Rules.GetPlayerPools().Mana==PaidMana,"duplicate release cannot cast or charge twice"))return;
   UOpusNative::WriteLog("HERO_TEST_PASS AuthoredCastRelease delay="+Clock+" mana="+BeforeMana+"->"+PaidMana);
   Tower.Rules.SetPlayerGod(true);BeforeCasts=V.CastSerial;BeforeReleases=V.AuthoredReleases;BeforeRequests=V.CastRequests;
   Next(23);return;
  }
  if(Stage==23)
  {
   if(!Check(Clock<12,"held cast chains through original Action End markers"))return;
   if(!V.PendingCast)for(auto Enemy:Tower.Enemies)if(Enemy!=LastHeldTarget&&H.IsCastTargetValid(Enemy)){C.AimAt(Enemy.ActorLocation);break;}
   int Requests=V.CastRequests;
   H.AdvanceCastInput(true,DeltaSeconds);
   if(V.CastRequests>Requests)LastHeldTarget=V.CastEnemy;
   if(!Check(V.CastSerial-BeforeCasts==V.AuthoredReleases-BeforeReleases,"held input never casts between release markers"))return;
   if(V.CastSerial-BeforeCasts<3)return;
   H.AdvanceCastInput(false,DeltaSeconds);V.MovementPressed();
   if(!Check(V.CastRequests-BeforeRequests==3,"held input starts one motion for each release"))return;
   UOpusNative::WriteLog("HERO_TEST_PASS HeldCastOnePerMotion casts=3 duration="+Clock);
   if(!Check(Tower.Rules.DodgePlayer(),"native dodge validation still accepts and owns costs"))return;
   V.Dodged(H.ActorRightVector);UOpusNative::WriteLog("HERO_TEST_PASS NativeCastAndDodge");Next(9);return;
  }
  if(Stage==9)
  {
   if(UOpusNative::Flag("DWOHeroArena"))
   {
    // A single real attacker makes nonlethal hurt observable before death;
    // this is a damage-delivery fixture, not a crowd or AI performance test.
    bool Attacker=false;
    for(auto Enemy:Tower.Enemies)if(IsValid(Enemy)&&!Enemy.Dead)
    {
     if(!Attacker){Attacker=true;Enemy.SetActorTickEnabled(true);Enemy.SetActorLocation(H.CombatLocation()+FVector(120,0,0));}
     else Enemy.SetActorTickEnabled(false);
    }
   }
   if(V.ActionState!=1&&Clock>2)Tower.Rules.SetPlayerGod(false);
   C.Move(FVector::ZeroVector,1,DeltaSeconds);if(V.HitSerial>0)HitSeen=true;
   if(!Tower.Rules.GetPlayerPools().bDead||V.ActionState!=8)return;
   if(!Check(HitSeen,"native monster damage drives hurt and death on the real character"))return;
   UOpusNative::WriteLog("HERO_TEST_PASS NativeHitAndDeath");Next(10);return;
  }
  if(Stage==10)
  {
   if(Clock<3.5)return;Capture("zero-death");Next(11);return;
  }
  if(Stage==11&&Clock>0.3){Done=true;Tower.Retry(false);}
 }
}
// Transient flat physics fixture; the production scene and assets stay intact.
class ADWOHeroProbeRoom : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Floor;
 default Floor.SetCollisionProfileName(n"BlockAll");
 default Floor.RelativeScale3D=FVector(40,40,0.5);
 default Floor.RelativeLocation=FVector(0,0,-25);
 UPROPERTY(DefaultComponent) UDirectionalLightComponent Sun;
 default Sun.RelativeRotation=FRotator(-60,-25,0);
 default Sun.Intensity=5;
 UPROPERTY(DefaultComponent) USkyLightComponent Fill;
 default Fill.Intensity=1;
 UFUNCTION(BlueprintOverride) void BeginPlay(){Floor.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cube.Cube")));}
}
