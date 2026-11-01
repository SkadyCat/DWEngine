// Original clips are unchanged. C compares stable control with the original
// direct-translation policy; training damage is local to this lab.
class AThreeCFighter : ACharacter
{
 default AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
 default AIControllerClass=AAIController;
 default Mesh.SetCollisionProfileName(n"NoCollision");
 default Mesh.RelativeLocation=FVector(0,0,-85);
 default Mesh.RelativeRotation=FRotator(0,-90,0);
 default Mesh.VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 default CharacterMovement.bUseRVOAvoidance=true;
 default CharacterMovement.AvoidanceConsiderationRadius=300;
 default bUseControllerRotationYaw=false;
 default bUseControllerRotationPitch=false;
 default bUseControllerRotationRoll=false;
 UPROPERTY(DefaultComponent,Attach=CollisionCylinder) UStaticMeshComponent Marker;
 default Marker.SetCollisionProfileName(n"NoCollision");
 default Marker.RelativeLocation=FVector(0,0,-83);
 default Marker.RelativeScale3D=FVector(1.05,1.05,0.025);
 UPROPERTY(DefaultComponent,Attach=CharacterMesh0) UStaticMeshComponent MainGear;
 default MainGear.SetCollisionProfileName(n"NoCollision");
 default MainGear.SetGenerateOverlapEvents(false);
 UPROPERTY(DefaultComponent,Attach=CharacterMesh0) UStaticMeshComponent OffGear;
 default OffGear.SetCollisionProfileName(n"NoCollision");
 default OffGear.SetGenerateOverlapEvents(false);
 UPROPERTY(DefaultComponent,Attach=CharacterMesh0) USkeletalMeshComponent GearRig;
 default GearRig.SetCollisionProfileName(n"NoCollision");
 default GearRig.SetGenerateOverlapEvents(false);
 default GearRig.bAlwaysCreatePhysicsState=false;
 default GearRig.VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 TArray<FC3Gear> Gear;
 bool EquipmentReady=false;
 UPROPERTY() AThreeCGameMode Arena;
 UPROPERTY() AThreeCFighter Target;
 UPROPERTY() AThreeCFighter AttackTarget;
 UPROPERTY() TMap<FName,UAnimSequence> Clips;
 UPROPERTY() TMap<FName,UAnimMontage> PoseMontages;
 UPROPERTY() UAnimMontage ActivePoseMontage;
 bool PoseBlending=true,SlotReady=false,Probing=false,GaitSync=true;
 FVector ActualTravelVelocity;
 // A blocked motor waits for clearance; collision noise is not a new stride.
 bool BlockGuard=true,Blocked=false,TravelRequested=false;
 FVector ProgressOrigin,ProgressDirection,FilteredTravel;
 double ProgressClock=0,BlockedSince=0,ClearFor=0;
 int BlockedEntries=0;
 double PoseBlendSeconds=0.22;
 int PoseTransitions=0;
 UPROPERTY() int Index=0;
 UPROPERTY() int Team=0;
 UPROPERTY() FString ModelId;
 FC3Model Data;
 FName State;
 double Health=160,ActionLeft=0,AttackCooldown=0,HitAt=-1,RespawnLeft=0;
 double Range=160,Speed=300,ActionRate=1,ActualSpeed=0,TurnRate=0;
 double TargetClock=0;
 double TargetHeldSince=-10,NextHitReaction=0,LocomotionSince=0,AttackYaw=0;
 double SignedTurnRate=0,LastLargeTurn=0,LastLargeTurnAt=-10;
 bool Chasing=false;
 int TargetChanges=0,ShortTargetChanges=0,RetargetsWhileActing=0;
 int AnimationStarts=0,HitRestarts=0,ShortClipCuts=0,ActionTurnFrames=0,AbruptTurnFrames=0,TurnReversals=0;
 bool Dead=false;
 int AttackCount=0,HitCount=0,DeathCount=0,WalkCount=0,TurnCount=0;
 FVector SpawnPoint,PreviousPosition,PreviousLeft,PreviousRight;
 double PreviousYaw=0;
 bool PreviousFeet=false;
 void Configure(AThreeCGameMode InArena,FC3Model InData,int InIndex,int InTeam,FVector At)
 {
  Arena=InArena;Data=InData;ModelId=Data.Id;Index=InIndex;Team=InTeam;SpawnPoint=At;
  PoseBlending=C3::Option("C3Blend","on")!="off";
  GaitSync=C3::Option("C3Gait","sync")!="legacy";
  BlockGuard=C3::Option("C3Block","stable")!="legacy";
  // Set playback rate after CharacterMovement has moved, before pose evaluation.
  Mesh.AddTickPrerequisiteActor(this);
  CapsuleComponent.SetCapsuleSize(34,85);CapsuleComponent.SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility,ECollisionResponse::ECR_Block);
  Mesh.SetSkinnedAssetAndUpdate(Cast<USkeletalMesh>(LoadObject(nullptr,Data.Path)));Mesh.SetRelativeScale3D(FVector(Data.Scale));
  for(auto P:Data.Clips){auto Clip=Cast<UAnimSequence>(LoadObject(nullptr,P.Value));if(Clip!=nullptr)Clips.Add(P.Key,Clip);}
  EquipVisuals();
  Marker.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cylinder.Cylinder")));
  Marker.SetMaterial(0,Cast<UMaterialInterface>(LoadObject(nullptr,"/Game/ThreeCLab/Materials/M_Team"+Team)));
  Range=ModelId.Contains("archer")||ModelId=="crossbowman"?850:ModelId.Contains("mage")||ModelId.Contains("shaman")?700:ModelId=="spearman"?210:155;
  Speed=Math::Clamp(Data.Gait,180.0,355.0);TargetClock=(Index%13)*0.023;
  CharacterMovement.bOrientRotationToMovement=false;CharacterMovement.bUseControllerDesiredRotation=false;
  CharacterMovement.MaxWalkSpeed=Speed;CharacterMovement.MaxAcceleration=1400;CharacterMovement.BrakingDecelerationWalking=1800;
  Reset();
 }
 USceneComponent GearComponent(int Slot) const
 {
  if(Gear[Slot].Skeletal)return GearRig;
  return Slot==0?MainGear:OffGear;
 }
 void EquipVisuals()
 {
  MainGear.SetVisibility(false);OffGear.SetVisibility(false);GearRig.SetVisibility(false);GearRig.SetComponentTickEnabled(false);
  Gear=C3Equipment::ForModel(ModelId);EquipmentReady=false;
  for(int I=0;I<Gear.Num();I++)
  {
   auto G=Gear[I];
   if(!Mesh.DoesSocketExist(G.Socket)||!Mesh.DoesSocketExist(G.Hand)){Error("C3_EQUIPMENT_SOCKET_FAILED "+ModelId+" "+G.Socket);return;}
   if(G.Skeletal)
   {
    auto Asset=Cast<USkeletalMesh>(LoadObject(nullptr,G.Asset));auto Clip=Cast<UAnimSequence>(LoadObject(nullptr,G.Animation));
    if(Asset==nullptr||Clip==nullptr){Error("C3_EQUIPMENT_ASSET_FAILED "+ModelId+" "+G.Asset);return;}
    GearRig.SetSkinnedAssetAndUpdate(Asset);GearRig.PlayAnimation(Clip,false);GearRig.SetPosition(float32(G.FreezeTime),false);GearRig.SetPlayRate(0);
    GearRig.GetAnimInstance().SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);GearRig.AddTickPrerequisiteComponent(Mesh);GearRig.SetComponentTickEnabled(true);
   }
   else
   {
    auto Asset=Cast<UStaticMesh>(LoadObject(nullptr,G.Asset));if(Asset==nullptr){Error("C3_EQUIPMENT_ASSET_FAILED "+ModelId+" "+G.Asset);return;}
    auto Component=I==0?MainGear:OffGear;Component.SetStaticMesh(Asset);
   }
   auto Component=GearComponent(I);Component.AttachToComponent(Mesh,G.Socket,EAttachmentRule::SnapToTarget,EAttachmentRule::SnapToTarget,EAttachmentRule::SnapToTarget,false);
   Component.SetRelativeTransform(FTransform(G.Rotation,G.Offset,G.Scale));Component.SetVisibility(true);
  }
  EquipmentReady=Gear.Num()>0||C3Equipment::BuiltInCount(ModelId)>0;
 }
 double EquipmentGripError(int Slot) const
 {
  auto G=Gear[Slot];FVector Grip=GearComponent(Slot).WorldTransform.TransformPosition(G.AssetGrip);
  FVector HandGrip=Mesh.GetSocketTransform(G.Hand,ERelativeTransformSpace::RTS_World).TransformPosition(G.HandGrip);
  return (Grip-HandGrip).Size();
 }
 double EquipmentFollowError(int Slot) const
 {
  auto G=Gear[Slot];FTransform Expected=FTransform(G.Rotation,G.Offset,G.Scale)*Mesh.GetSocketTransform(G.Socket,ERelativeTransformSpace::RTS_World);
  return (Expected.Location-GearComponent(Slot).WorldLocation).Size();
 }
 void Reset()
 {
  SetActorLocation(SpawnPoint);Health=160+(Index%3)*35;Dead=false;Target=nullptr;AttackTarget=nullptr;HitAt=-1;ActionLeft=0;AttackCooldown=0.2+(Index%7)*0.1;
  TargetHeldSince=-10;NextHitReaction=0;Chasing=false;LocomotionSince=Arena.Clock;LastLargeTurnAt=-10;
  PreviousPosition=ActorLocation;PreviousYaw=ActorRotation.Yaw;PreviousFeet=false;SlotReady=false;ActivePoseMontage=nullptr;Mesh.GlobalAnimRateScale=1;State=NAME_None;Play(n"idle",true);SetControlled(Arena.Controlled);
 }
 void StopTravel(){CharacterMovement.StopMovementImmediately();ConsumeMovementInputVector();}
 void ClearTravelGuard()
 {
  Blocked=false;TravelRequested=false;ProgressClock=0;ClearFor=0;ProgressOrigin=ActorLocation;FilteredTravel=FVector::ZeroVector;
 }
 void SetControlled(bool Value){StopTravel();Chasing=false;ClearTravelGuard();CharacterMovement.SetMovementMode(Value?EMovementMode::MOVE_Walking:EMovementMode::MOVE_None);}
 bool HoldForClearance(double Dt,FVector To,bool WantsTravel)
 {
  if(!BlockGuard)return false;
  if(!WantsTravel){ClearTravelGuard();return false;}
  if(!Blocked)
  {
   if(!TravelRequested){ProgressOrigin=ActorLocation;ProgressDirection=To.GetSafeNormal2D();ProgressClock=0;}
   else
   {
    ProgressClock+=Dt;
    if(ProgressClock>=0.28)
    {
     FVector Progress=ActorLocation-ProgressOrigin;
     // Net progress rejects back-and-forth RVO motion that has nonzero speed.
     if(Progress.DotProduct(ProgressDirection)<Math::Max(4.0,Speed*ProgressClock*0.1)&&Progress.Size2D()<Math::Max(12.0,Speed*ProgressClock*0.25))
     {Blocked=true;BlockedSince=Arena.Clock;ClearFor=0;BlockedEntries++;FilteredTravel=FVector::ZeroVector;}
     ProgressOrigin=ActorLocation;ProgressDirection=To.GetSafeNormal2D();ProgressClock=0;
    }
   }
   TravelRequested=false;
  }
  if(!Blocked)return false;
  TArray<AActor> Ignore;Ignore.Add(this);FHitResult Hit;
  FVector Start=ActorLocation+FVector(0,0,2);
  bool Obstructed=System::CapsuleTraceSingleByProfile(Start,Start+To.GetSafeNormal2D()*Math::Max(65.0,Speed*0.28),34,82,n"Pawn",false,Ignore,EDrawDebugTrace::None,Hit,true);
  ClearFor=Obstructed?0:ClearFor+Dt;
  // There is no timed shove/retry. Resume only after an actual body-sized gap
  // stays open, otherwise a stationary blocker would produce periodic jerks.
  if(ClearFor>=0.18&&Arena.Clock-BlockedSince>=0.3){ClearTravelGuard();return false;}
  StopTravel();Play(n"idle",true);return true;
 }
 bool IsActionState() const{return State==n"attack1"||State==n"attack2"||State==n"hit";}
 bool UsesPoseBlends() const{return PoseBlending&&Arena.Controlled&&Arena.PoseModel.IsEmpty();}
 double LocomotionRate(FName Key,double Travel,double Dt) const
 {
  if(GaitSync)
  {
   if(!C3Gaits::Striding(ModelId))return 1;
   return Math::Clamp(Travel/Math::Max(1.0,C3Gaits::Speed(ModelId,Key)*Data.Scale),0.0,3.0);
  }
  double Wanted=Math::Clamp(Travel/Math::Max(1.0,Data.Gait*Data.Scale),0.35,1.35);
  return ActionRate+(Wanted-ActionRate)*Math::Min(1.0,Dt*6);
 }
 void StartPose(FName Key,bool Loop,double Rate,FName From)
 {
  if(!UsesPoseBlends()){SlotReady=false;ActivePoseMontage=nullptr;Mesh.PlayAnimation(Clips[Key],Loop);Mesh.SetPlayRate(float32(Rate));return;}
  if(!PoseMontages.Contains(Key))
  {
   auto Montage=Cast<UAnimMontage>(LoadObject(nullptr,"/Game/ThreeCLab/Animation/"+ModelId+"/AM_"+Key.ToString()));
   if(Montage==nullptr){Error("C3_POSE_MONTAGE_FAILED "+ModelId+" "+Key);return;}
   // Keep the last evaluated pose at the end. The next command, rather than
   // automatic blend-out to reference pose, owns the handoff.
   PoseMontages.Add(Key,Montage);
  }
  auto Next=PoseMontages[Key];
  if(!SlotReady)
  {
   // A native single-node slot renderer is installed once. Subsequent commands
   // only play montages on the same slot, preserving outgoing pose evaluation.
   Mesh.PlayAnimation(Next,Loop);SlotReady=true;
  }
  else
  {
   auto Anim=Mesh.GetAnimInstance();
   if(GaitSync&&ActivePoseMontage!=nullptr&&(From==n"run"||From==n"walk"))
   {
    // Freeze the outgoing gait when committing to a stationary action. For
    // gait-to-gait transitions retain its own calibrated travel rate.
    bool NextMoves=Key==n"run"||Key==n"walk";
    double OutRate=NextMoves?LocomotionRate(From,ActualSpeed,1):0;
    Anim.Montage_SetPlayRate(ActivePoseMontage,float32(OutRate));
   }
   FAlphaBlendArgs Blend;Blend.BlendTime=float32(Key==n"death"?0.12:PoseBlendSeconds);Blend.BlendOption=EAlphaBlendOption::Cubic;
   if(Anim.Montage_PlayWithBlendIn(Next,Blend,float32(Rate))<=0)Error("C3_POSE_PLAY_FAILED "+ModelId+" "+Key);
   PoseTransitions++;
  }
  ActivePoseMontage=Next;
  auto Anim=Mesh.GetAnimInstance();Anim.SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
  Anim.Montage_SetNextSection(n"Default",Loop?n"Default":NAME_None,Next);Anim.Montage_SetPlayRate(Next,float32(Rate));
 }
 void Play(FName Key,bool Loop,double Rate=1)
 {
  FName Chosen=Clips.Contains(Key)?Key:n"idle";if(!Clips.Contains(Chosen))return;
  if(State==Chosen&&(Loop||(Arena.Controlled&&ActionLeft>0))){ActionRate=Rate;Mesh.SetPlayRate(float32(Rate));return;}
  if(State==n"hit"&&Chosen==n"hit"&&ActionLeft>0)HitRestarts++;
  if(State!=Chosen&&IsActionState()&&ActionLeft>0.08&&Chosen!=n"death")ShortClipCuts++;
  AnimationStarts++;if(State!=Chosen)LocomotionSince=Arena.Clock;
  FName From=State;State=Chosen;ActionRate=Rate;StartPose(Chosen,Loop,Rate,From);ActionLeft=Loop?0:Clips[Chosen].GetPlayLength()/Math::Max(0.05,Rate);
 }
 void TakeTrainingHit(double Damage)
 {
  if(Dead)return;Health-=Damage;HitCount++;
  if(Health<=0){Dead=true;DeathCount++;HitAt=-1;if(Arena.Controlled)StopTravel();Play(n"death",false);RespawnLeft=ActionLeft+1.2;return;}
  // Damage always applies. Stable presentation finishes its current committed
  // action and coalesces repeated hit reactions instead of restarting frame 0.
  if(Arena.Controlled)
  {
   if((IsActionState()&&ActionLeft>0)||Arena.Clock<NextHitReaction)return;
   HitAt=-1;AttackTarget=nullptr;StopTravel();
   Play(n"hit",false);AttackYaw=ActorRotation.Yaw;NextHitReaction=Arena.Clock+Math::Max(0.8,ActionLeft+0.25);return;
  }
  Play(n"hit",false);
 }
 void ChangeTarget(AThreeCFighter Next)
 {
  if(Next==Target)return;
  if(IsValid(Target)&&!Target.Dead)
  {
   if(Arena.Clock-TargetHeldSince<0.75)ShortTargetChanges++;
   if(IsActionState()&&ActionLeft>0)RetargetsWhileActing++;
  }
  Target=Next;TargetHeldSince=Arena.Clock;TargetChanges++;ClearTravelGuard();
 }
 void TickStable(double Dt)
 {
  AttackCooldown=Math::Max(0.0,AttackCooldown-Dt);ActionLeft=Math::Max(0.0,ActionLeft-Dt);
  if(HitAt>=0)
  {
   HitAt-=Dt;
   if(HitAt<=0)
   {
    HitAt=-1;
    if(IsValid(AttackTarget)&&!AttackTarget.Dead&&(AttackTarget.ActorLocation-ActorLocation).Size2D()<Range*1.35)
    {
     AttackTarget.TakeTrainingHit(9+(Index%5));
     if(Range>300)System::DrawDebugLine(Mesh.WorldLocation+FVector(0,0,100),AttackTarget.Mesh.WorldLocation+FVector(0,0,90),Arena.TeamColor(Team),0.12,2);
    }
   }
  }
  // The victim and facing belong to the committed action, including recovery.
  if(IsActionState()&&ActionLeft>0)
  {
   StopTravel();ClearTravelGuard();SetActorRotation(FRotator(0,AttackYaw,0));Chasing=false;return;
  }
  bool ValidTarget=IsValid(Target)&&!Target.Dead;
  TargetClock-=Dt;
  if(!ValidTarget||TargetClock<=0)
  {
   TargetClock=0.8+(Index%9)*0.035;AThreeCFighter Nearest;double Best=1e30;
   for(auto Other:Arena.Fighters)if(Other!=this&&Other.Team!=Team&&!Other.Dead)
   {
    double D=(Other.ActorLocation-ActorLocation).SizeSquared2D();if(D<Best){Best=D;Nearest=Other;}
   }
   // A modestly closer neighbour cannot steal an existing engagement.
   double Current=ValidTarget?(Target.ActorLocation-ActorLocation).SizeSquared2D():1e30;
   if(!ValidTarget||(Arena.Clock-TargetHeldSince>1.25&&Current>(Range*1.35)*(Range*1.35)&&Best<Current*0.36))ChangeTarget(Nearest);
  }
  if(!IsValid(Target)||Target.Dead){StopTravel();ClearTravelGuard();Play(n"idle",true);return;}
  FVector To=Target.ActorLocation-ActorLocation;double Distance=To.Size2D();
  // Do not insert a tiny idle between two attacks. Retain recovery pose until
  // the next attack is ready; a chase can still transition directly to motion.
  if(UsesPoseBlends()&&(State==n"attack1"||State==n"attack2")&&Distance<Range*1.05&&AttackCooldown>0&&AttackCooldown<=0.22){StopTravel();return;}
  // Separate entry/exit distances prevent run-idle-run chatter at attack range.
  if(Chasing){if(Distance<Range*0.78)Chasing=false;}else if(Distance>Range*1.08)Chasing=true;
  bool PreparingAttack=Distance<Range&&AttackCooldown<=0;
  if(GaitSync&&PreparingAttack){Chasing=false;StopTravel();}
  if(HoldForClearance(Dt,To,Chasing&&!PreparingAttack))return;
  FilteredTravel=Math::VInterpTo(FilteredTravel,ActualTravelVelocity,float32(Dt),8);
  bool StableHeading=ActualSpeed>40&&FilteredTravel.Size2D()>40&&FilteredTravel.DotProduct(To.GetSafeNormal2D())>0;
  FVector Heading=GaitSync&&Chasing&&(BlockGuard?StableHeading:ActualSpeed>15)?(BlockGuard?FilteredTravel:ActualTravelVelocity):To;
  double YawDelta=0;
  if(Distance>70||PreparingAttack)
  {
   double DesiredYaw=Heading.ToOrientationRotator().Yaw;YawDelta=DesiredYaw-ActorRotation.Yaw;
   while(YawDelta>180)YawDelta-=360;while(YawDelta<-180)YawDelta+=360;
   if(GaitSync&&Chasing&&Math::Abs(YawDelta)<6)YawDelta=0;
   double Step=Math::Clamp(YawDelta,-300.0*Dt,300.0*Dt);SetActorRotation(FRotator(0,ActorRotation.Yaw+Step,0));YawDelta-=Step;
  }
  double Drill=(Arena.Clock+Index*0.37)%28;
  double Pace=Clips.Contains(n"walk")&&Drill>17&&Drill<20?0.45:1.0;
  CharacterMovement.MaxWalkSpeed=Speed*Pace;
  double AttackDelta=To.ToOrientationRotator().Yaw-ActorRotation.Yaw;while(AttackDelta>180)AttackDelta-=360;while(AttackDelta<-180)AttackDelta+=360;
  if(Distance<Range&&Math::Abs(AttackDelta)<15&&AttackCooldown<=0)
  {
   AttackCount++;AttackTarget=Target;AttackYaw=ActorRotation.Yaw;Chasing=false;StopTravel();
   FName Attack=AttackCount%2==0&&Clips.Contains(n"attack2")?n"attack2":n"attack1";
   Play(Attack,false);AttackCooldown=Math::Max(0.9,ActionLeft+0.18)+(Index%5)*0.07;HitAt=Math::Min(0.42,ActionLeft*0.42);return;
  }
  if(Chasing&&Math::Abs(YawDelta)<65){AddMovementInput(To.GetSafeNormal2D(),1);TravelRequested=true;}
  // Use actual travel with a dead band; crowded actors do not flicker between
  // idle and run because a requested move was immediately blocked by a capsule.
  double Travel=GaitSync?ActualSpeed:GetVelocity().Size2D();bool WasMoving=State==n"run"||State==n"walk";
  bool Moving=WasMoving?Travel>(GaitSync?2:18):Travel>(GaitSync?6:38);
  if(UsesPoseBlends()&&IsActionState()&&Chasing&&Math::Abs(YawDelta)<65)Moving=true;
  bool UseWalk=Clips.Contains(n"walk")&&(GaitSync?Travel<C3Gaits::Speed(ModelId,n"walk")*Data.Scale*(State==n"walk"?1.3:0.95):Pace<0.6);
  FName Next=Moving?(UseWalk?n"walk":n"run"):n"idle";
  double Rate=Moving?LocomotionRate(Next,Travel,Dt):1;
  if(Next==State||IsActionState()||Arena.Clock-LocomotionSince>=0.25)Play(Next,true,Rate);
  if(Moving)WalkCount++;
 }
 void MeasureMotion(double Dt,bool WasActing)
 {
  ActualSpeed=(ActorLocation-PreviousPosition).Size2D()/Math::Max(0.001,Dt);
  double DeltaYaw=ActorRotation.Yaw-PreviousYaw;while(DeltaYaw>180)DeltaYaw-=360;while(DeltaYaw<-180)DeltaYaw+=360;
  SignedTurnRate=DeltaYaw/Math::Max(0.001,Dt);TurnRate=Math::Abs(SignedTurnRate);
  if(Math::Abs(DeltaYaw)>40)TurnCount++;
  if(Math::Abs(DeltaYaw)>35)AbruptTurnFrames++;
  if(WasActing&&Math::Abs(DeltaYaw)>3)ActionTurnFrames++;
  if(Math::Abs(DeltaYaw)>30)
  {
   if(DeltaYaw*LastLargeTurn<0&&Arena.Clock-LastLargeTurnAt<0.25)TurnReversals++;
   LastLargeTurn=DeltaYaw;LastLargeTurnAt=Arena.Clock;
  }
  PreviousPosition=ActorLocation;PreviousYaw=ActorRotation.Yaw;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Arena==nullptr||Probing)return;double Dt=Arena.Controlled?double(DeltaSeconds):Math::Min(double(DeltaSeconds),0.05);
  ActualTravelVelocity=(ActorLocation-PreviousPosition)/Math::Max(0.001,double(DeltaSeconds));ActualTravelVelocity.Z=0;ActualSpeed=ActualTravelVelocity.Size2D();
  Mesh.GlobalAnimRateScale=Arena.Paused?0:1;
  if(Arena.Paused){StopTravel();PreviousPosition=ActorLocation;PreviousYaw=ActorRotation.Yaw;ActualTravelVelocity=FVector::ZeroVector;ActualSpeed=0;TurnRate=0;SignedTurnRate=0;return;}
  // Even while a state transition waits for its hysteresis, the currently
  // visible gait must track travel. A blocked actor freezes its current step.
  if(Arena.Controlled&&GaitSync&&(State==n"run"||State==n"walk"))ActionRate=LocomotionRate(State,ActualSpeed,Dt);
  Mesh.SetPlayRate(float32(ActionRate));
  if(Dead){RespawnLeft-=Dt;if(RespawnLeft<=0)Reset();return;}
  if(Arena.Clock<2.5)return;
  bool WasActing=IsActionState()&&ActionLeft>Dt+0.001;
  if(Arena.Controlled){TickStable(Dt);MeasureMotion(double(DeltaSeconds),WasActing);return;}
  TargetClock-=Dt;if(TargetClock<=0||!IsValid(Target)||Target.Dead)
  {
   TargetClock=0.4+(Index%9)*0.015;double Best=1e30;AThreeCFighter Nearest;
   for(auto Other:Arena.Fighters)if(Other!=this&&Other.Team!=Team&&!Other.Dead){double D=(Other.ActorLocation-ActorLocation).SizeSquared2D();if(D<Best){Best=D;Nearest=Other;}}ChangeTarget(Nearest);
  }
  if(Target==nullptr){Play(n"idle",true);return;}
  AttackCooldown-=Dt;ActionLeft=Math::Max(0.0,ActionLeft-Dt);
  if(HitAt>=0)
  {
   HitAt-=Dt;if(HitAt<=0){HitAt=-1;if(!Target.Dead&&(Target.ActorLocation-ActorLocation).Size2D()<Range*1.35){Target.TakeTrainingHit(9+(Index%5));if(Range>300)System::DrawDebugLine(Mesh.WorldLocation+FVector(0,0,100),Target.Mesh.WorldLocation+FVector(0,0,90),Arena.TeamColor(Team),0.12,2);}}
  }
  FVector To=Target.ActorLocation-ActorLocation;double Distance=To.Size2D();
  FRotator Facing=To.Rotation();Facing.Pitch=0;Facing.Roll=0;
  SetActorRotation(Arena.Controlled?Math::RInterpTo(ActorRotation,Facing,float32(Dt),8):Facing);
  FVector Move=FVector::ZeroVector;bool Acting=ActionLeft>0&&(State==n"attack1"||State==n"attack2"||State==n"hit");
  double Drill=(Arena.Clock+Index*0.37)%28;
  bool Retreat=Drill>22&&Drill<24.5;
  double Pace=Drill>17&&Drill<20?0.45:Drill>24.5&&Drill<26?1.45:1.0;
  if((Distance>Range*0.92||Retreat)&&(!Arena.Controlled||!Acting))Move=To.GetSafeNormal2D()*(Retreat?-1:1);
  FVector Push=FVector::ZeroVector;
  for(auto Other:Arena.Fighters)if(Other!=this&&!Other.Dead){FVector Away=ActorLocation-Other.ActorLocation;double D=Away.Size2D();if(D>1&&D<95)Push+=Away.GetSafeNormal2D()*(1-D/95);}
  if(Arena.Controlled){CharacterMovement.MaxWalkSpeed=Speed*Pace;AddMovementInput((Move+Push*0.6).GetSafeNormal2D(),float32(Move.IsNearlyZero()?0:1));}
  else
  {
   FVector P=ActorLocation+Move*Speed*Pace*Dt+Push*120*Dt;P.X=Math::Clamp(P.X,-3600.0,3600.0);P.Y=Math::Clamp(P.Y,-3600.0,3600.0);SetActorLocation(P);
  }
  if(Distance<Range&&!Retreat&&AttackCooldown<=0&&(!Arena.Controlled||!Acting))
  {
   AttackCount++;FName Attack=AttackCount%2==0&&Clips.Contains(n"attack2")?n"attack2":n"attack1";
   Play(Attack,false);AttackCooldown=Math::Max(0.9,ActionLeft+0.12)+(Index%5)*0.07;HitAt=Math::Min(0.42,ActionLeft*0.42);
  }
  else if(ActionLeft<=0)
  {
   bool Moving=!Move.IsNearlyZero();FName Next=Moving?(Pace<0.6&&Clips.Contains(n"walk")?n"walk":n"run"):n"idle";
   double Rate=Moving?Speed*Pace/Math::Max(1.0,Data.Gait*Data.Scale):1;Play(Next,true,Rate);if(Moving)WalkCount++;
  }
  MeasureMotion(double(DeltaSeconds),WasActing);
 }
}
