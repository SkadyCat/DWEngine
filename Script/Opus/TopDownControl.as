// Zero Clean3C's fixed camera, independent cursor aim, CMC movement intent and
// bounded post-roll facing recovery. The original follow component owns lag.
class UDWOTopDownControl : UActorComponent
{
 UPROPERTY() ADWOHero Hero;
 UPROPERTY() UCleanCameraFollowComponent CameraFollow;
 UPROPERTY(EditAnywhere,Category="Top Down") double Pitch=-60,Yaw=0,Distance=2200,AimDeadZone=20;
 FVector DesiredMove,MoveVelocity;
 bool WaitForRelease=false,bWalk=false,bRecoveringRollAim=false;
 double RollAimAngularSpeed=0;
 UPROPERTY() ADWOEnemy AutoTarget;
 double AutoTargetHeld=0;
 // Lab engagement hysteresis, applied only to automatic aim. Manual pointer
 // aiming and a committed cast's authored release retain their own snapshots.
 ADWOEnemy SelectAutoTarget(double Dt)
 {
  AutoTargetHeld+=Math::Max(0.0,Dt);
  ADWOEnemy Nearest;double Best=1e30;
  for(auto Enemy:Hero.Tower.Enemies)if(Hero.IsCastTargetValid(Enemy))
  {
   double Distance=(Enemy.ActorLocation-Hero.ActorLocation).SizeSquared2D();if(Distance<Best){Best=Distance;Nearest=Enemy;}
  }
  bool Valid=Hero.IsCastTargetValid(AutoTarget);
  if(Valid&&Hero.Visual.RotationLocked())return AutoTarget;
  double Current=Valid?(AutoTarget.ActorLocation-Hero.ActorLocation).SizeSquared2D():1e30;
  if(!Valid||(AutoTargetHeld>=1.25&&Best<Current*0.36))
  {
   if(AutoTarget!=Nearest){AutoTarget=Nearest;AutoTargetHeld=0;}
  }
  return AutoTarget;
 }
 void Initialize(ADWOHero H)
 {
  Hero=H;SetTickGroup(ETickingGroup::TG_PostPhysics);AddTickPrerequisiteComponent(Hero.CharacterMovement);
  ConfigureCamera();CameraFollow=UCleanCameraFollowComponent::GetOrCreate(Hero);CameraFollow.Configure(Hero.CameraBoom,Hero.CharacterMovement);CameraFollow.ResetAfterTeleport();
  UOpusNative::WriteLog("TOPDOWN_CONTROL_READY source=ZeroClean3C characterMovement=1 pitch=-60 yaw=0 distance=2200 cameraRelativeWASD=1");
 }
 void ConfigureCamera()
 {
  Hero.CameraBoom.SetAbsolute(false,true,false);Hero.CameraBoom.bUsePawnControlRotation=false;
  Hero.CameraBoom.SetWorldRotation(FRotator(Pitch,Yaw,0));Hero.CameraBoom.TargetArmLength=Distance;
  Hero.CameraBoom.RelativeLocation=FVector(0,0,75);Hero.CameraBoom.bDoCollisionTest=false;Hero.Camera.bUsePawnControlRotation=false;Hero.Camera.FieldOfView=60;
 }
 FVector MoveDirection(double Forward,double Right) const {return FRotator(0,Yaw,0).RotateVector(FVector(Forward,Right,0).GetSafeNormal());}
 void Stop(bool RequireRelease=false)
 {
  DesiredMove=FVector::ZeroVector;MoveVelocity=FVector::ZeroVector;
  if(Hero!=nullptr){Hero.ConsumeMovementInputVector();Hero.CharacterMovement.StopMovementImmediately();Hero.GameplayMovementMultiplier=0;}
  if(RequireRelease)WaitForRelease=true;
 }
 bool InputReady(APlayerController PC)
 {
  if(!WaitForRelease)return true;
  if(PC.IsInputKeyDown(EKeys::W)||PC.IsInputKeyDown(EKeys::S)||PC.IsInputKeyDown(EKeys::A)||PC.IsInputKeyDown(EKeys::D)||PC.IsInputKeyDown(EKeys::LeftMouseButton)||PC.IsInputKeyDown(EKeys::SpaceBar)||PC.IsInputKeyDown(EKeys::LeftShift))return false;
  WaitForRelease=false;return true;
 }
 void Move(FVector Direction,double Scale,double Dt)
 {
  if(Hero.CharacterMovement.MovementMode==EMovementMode::MOVE_None&&!Hero.Rules.GetPlayerPools().bDead)Hero.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
  DesiredMove=Direction.GetSafeNormal2D();Hero.GameplayMovementMultiplier=float(Math::Max(0.0,Scale));
  Hero.CharacterMovement.MaxWalkSpeed=bWalk?300:500;Hero.CharacterMovement.MaxWalkSpeedCrouched=200;Hero.SetNetworkLocomotion(bWalk,false,300,500,700);
  bool Locked=Hero.Visual.MovementLocked();Hero.SetNetworkActionLock(Hero.Visual.RotationLocked(),Hero.ActorRotation.Yaw,Locked);
  if(Scale<=0){Stop();return;}if(!Locked)Hero.AddMovementInput(DesiredMove,1);else Hero.ConsumeMovementInputVector();
 }
 void Suspend(){Stop();Hero.CharacterMovement.DisableMovement();}
 bool AimAt(FVector Target)
 {
  FVector Delta=Target-Hero.ActorLocation;Delta.Z=0;if(Delta.SizeSquared()<AimDeadZone*AimDeadZone)return false;
  Hero.AimPoint=FVector(Target.X,Target.Y,Hero.CombatLocation().Z);Hero.Aim=Delta.GetSafeNormal();
  if(!Hero.Visual.RotationLocked()&&!bRecoveringRollAim)Face(Hero.Aim.Rotation().Yaw);return true;
 }
 void Face(double NewYaw){Hero.SetActorRotation(FRotator(0,NewYaw,0));auto PC=Hero.GetController();if(PC!=nullptr)PC.SetControlRotation(FRotator(0,NewYaw,0));}
 void UpdateRollAimRecovery(double Dt)
 {
  if(!bRecoveringRollAim||Hero.Visual.RotationLocked()||Dt<=0)return;
  double Current=Hero.ActorRotation.Yaw,Delta=Math::FindDeltaAngleDegrees(Current,Hero.Aim.Rotation().Yaw);
  double BrakeSpeed=Math::Sqrt(2.0*1800.0*Math::Abs(Delta));double Desired=Math::Sign(Delta)*Math::Min(540.0,Math::Min(BrakeSpeed,Math::Abs(Delta)*10.0));
  RollAimAngularSpeed=Math::FInterpConstantTo(RollAimAngularSpeed,Desired,Dt,1800.0);double Step=RollAimAngularSpeed*Dt;
  if(Math::Sign(Step)==Math::Sign(Delta)&&Math::Abs(Step)>Math::Abs(Delta))Step=Delta;
  double NewYaw=Current+Step;if(Math::Abs(Delta)<=0.25){NewYaw=Hero.Aim.Rotation().Yaw;bRecoveringRollAim=false;RollAimAngularSpeed=0;}Face(NewYaw);
 }
 bool AimFromScreen(APlayerController PC,double X,double Y)
 {
  int Width=0,Height=0;PC.GetViewportSize(Width,Height);if(X<0||Y<0||X>=Width||Y>=Height)return false;
  FVector Origin,Direction;if(!PC.DeprojectScreenPositionToWorld(float32(X),float32(Y),Origin,Direction))return false;
  TArray<AActor> Ignore;Ignore.Add(Hero);FHitResult Hit;
  if(System::LineTraceSingle(Origin,Origin+Direction*20000,ETraceTypeQuery::Visibility,false,Ignore,EDrawDebugTrace::None,Hit,true))return AimAt(Hit.ImpactPoint);
  if(Math::Abs(Direction.Z)<=0.0001)return false;
  double T=(Hero.ActorLocation.Z-Hero.CapsuleComponent.GetScaledCapsuleHalfHeight()-Origin.Z)/Direction.Z;return T>0&&T<20000&&AimAt(Origin+Direction*T);
 }
 void MouseAim(APlayerController PC){float32 X=0,Y=0;if(PC.GetMousePosition(X,Y))AimFromScreen(PC,X,Y);}
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds){if(Hero==nullptr)return;ConfigureCamera();MoveVelocity=Hero.GetVelocity();}
}
