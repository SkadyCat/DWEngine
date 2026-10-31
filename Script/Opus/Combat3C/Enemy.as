// 3C lab stable motor and montage presentation. Native rules alone choose and
// release skills: visual action locks never suppress damage or reset cooldowns.
class ADWOEnemy : ACharacter
{
 default AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
 default AIControllerClass=AAIController;
 default bUseControllerRotationYaw=false;
 default bUseControllerRotationPitch=false;
 default bUseControllerRotationRoll=false;
 default Mesh.SetVisibility(false);
 default Mesh.SetComponentTickEnabled(false);
 default CharacterMovement.bOrientRotationToMovement=false;
 default CharacterMovement.bUseControllerDesiredRotation=false;
 default CharacterMovement.bUseRVOAvoidance=true;
 default CharacterMovement.AvoidanceConsiderationRadius=300;
 default CharacterMovement.MaxAcceleration=1400;
 default CharacterMovement.BrakingDecelerationWalking=1800;
 UPROPERTY(DefaultComponent,Attach=CollisionCylinder) UStaticMeshComponent Body;
 default Body.SetCollisionProfileName(n"NoCollision");
 UPROPERTY(DefaultComponent,Attach=CollisionCylinder) USkeletalMeshComponent Model;
 default Model.SetCollisionProfileName(n"NoCollision");
 default Model.RelativeRotation=FRotator(0,-90,0);
 default Model.VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 UPROPERTY(DefaultComponent,Attach=Model) UStaticMeshComponent Weapon;
 default Weapon.SetCollisionProfileName(n"NoCollision");
 default Weapon.SetGenerateOverlapEvents(false);
 UPROPERTY(DefaultComponent,Attach=Model) UStaticMeshComponent Offhand;
 default Offhand.SetCollisionProfileName(n"NoCollision");
 default Offhand.SetGenerateOverlapEvents(false);
 UPROPERTY(DefaultComponent,Attach=Model) USkeletalMeshComponent GearRig;
 default GearRig.SetCollisionProfileName(n"NoCollision");
 default GearRig.SetGenerateOverlapEvents(false);
 default GearRig.bAlwaysCreatePhysicsState=false;
 default GearRig.VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 UPROPERTY(DefaultComponent) UDWRuleTargetComponent RuleTarget;
 default RuleTarget.Team=1;
 UPROPERTY() UDWRuleWorldSubsystem Rules;
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TMap<FName,UAnimSequence> Clips;
 UPROPERTY() TMap<FName,UAnimMontage> PoseMontages;
 UPROPERTY() UAnimMontage ActivePoseMontage;
 UPROPERTY() int EntityId=0;
 UPROPERTY() bool Boss=false,Staged=false,Dead=false;
 UPROPERTY() double Speed=300,StopDistance=130;
 FString EventId;
 FDWOModel Appearance;
 TArray<FDWO3CGear> Gear;
 bool EquipmentReady=false,SlotReady=false,HasGaitCalibration=false;
 FName AnimationState;
 double ActionRemaining=0,ActionRate=1,CorpseIn=4,LastLife=0,LastShield=0;
 int AttackSerial=0,AnimationStarts=0,PoseTransitions=0,CoalescedHits=0;
 double Clock=0,NextHitReaction=0,LocomotionSince=0,ActionYaw=0;
 FVector ActionTarget,ActualTravelVelocity,PreviousPosition,FilteredTravel;
 double ActualSpeed=0;
 bool Chasing=false,Blocked=false,TravelRequested=false;
 FVector ProgressOrigin,ProgressDirection;
 double ProgressClock=0,BlockedSince=0,ClearFor=0;
 int BlockedEntries=0;
 TArray<FVector> DungeonPath;
 int PathIndex=0;
 double RepathIn=0;
 FDWOMonsterHealthBar HealthBar;
 double HealthBarHeight=110;

 void ConfigureAppearance(FString Id)
 {
  Appearance=DWOModels::Streamable(Id);if(Appearance.Mesh.IsEmpty()){Tower.Fail("Unknown model "+Id);return;}
  HasGaitCalibration=DWO3CAssets::Calibrated(Id);
  auto Asset=Cast<USkeletalMesh>(UOpusAssets::Find(this,Appearance.Mesh));if(Asset==nullptr){Tower.Fail("Model not prepared "+Appearance.Mesh);return;}
  double HalfHeight=Math::Max(85.0,RuleTarget.RadiusCm);
  CapsuleComponent.SetCapsuleSize(float32(RuleTarget.RadiusCm),float32(HalfHeight));
  CapsuleComponent.SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility,ECollisionResponse::ECR_Ignore);
  Model.RelativeLocation=FVector(0,0,-HalfHeight);Model.SetSkinnedAssetAndUpdate(Asset);Model.SetRelativeScale3D(FVector(Appearance.Scale));Body.SetVisibility(false);
  // Native movement completes before measuring displacement; the slot evaluates
  // afterwards, including outgoing poses and attached equipment.
  SetTickGroup(ETickingGroup::TG_PostPhysics);Model.AddTickPrerequisiteActor(this);Model.AddTickPrerequisiteComponent(CharacterMovement);
  for(auto C:Appearance.Clips)
  {
   auto Clip=Cast<UAnimSequence>(UOpusAssets::Find(this,C.Value));if(Clip!=nullptr)Clips.Add(C.Key,Clip);
   auto Montage=Cast<UAnimMontage>(UOpusAssets::Find(this,DWO3CAssets::Montage(Id,C.Key)));
   if(Montage==nullptr){Tower.Fail("3C montage not prepared "+Id+" "+C.Key);return;}PoseMontages.Add(C.Key,Montage);
  }
  EquipVisuals();auto Pools=Rules.GetTargetPools(EntityId);LastLife=Pools.Life;LastShield=Pools.Shield;
  PreviousPosition=ActorLocation;ClearTravelGuard();Play(n"idle",true);RefreshHealthBar(0);
  CharacterMovement.SetMovementMode(Staged?EMovementMode::MOVE_None:EMovementMode::MOVE_Walking);
  Model.GlobalAnimRateScale=Staged?0:1;
  UOpusNative::WriteLog("TowerAppearance id="+EntityId+" model="+Id+" clips="+Clips.Num()+" stable3C=1 gear="+Gear.Num());
 }
 USceneComponent GearComponent(int Slot) const {return Gear[Slot].Skeletal?Cast<USceneComponent>(GearRig):(Slot==0?Cast<USceneComponent>(Weapon):Cast<USceneComponent>(Offhand));}
 void EquipVisuals()
 {
  Weapon.SetVisibility(false);Offhand.SetVisibility(false);GearRig.SetVisibility(false);GearRig.SetComponentTickEnabled(false);
  Gear=DWO3CEquipment::ForModel(Appearance.Id);EquipmentReady=false;
  for(int I=0;I<Gear.Num();I++)
  {
   auto G=Gear[I];if(!Model.DoesSocketExist(G.Socket)||!Model.DoesSocketExist(G.Hand)){Tower.Fail("3C equipment socket "+Appearance.Id);return;}
   if(G.Skeletal)
   {
    auto Asset=Cast<USkeletalMesh>(UOpusAssets::Find(this,DWOModels::ObjectPath(G.Asset)));
    auto Clip=Cast<UAnimSequence>(UOpusAssets::Find(this,DWOModels::ObjectPath(G.Animation)));
    if(Asset==nullptr||Clip==nullptr){Tower.Fail("3C equipment asset "+G.Asset);return;}
    GearRig.SetSkinnedAssetAndUpdate(Asset);GearRig.PlayAnimation(Clip,false);GearRig.SetPosition(float32(G.FreezeTime),false);GearRig.SetPlayRate(0);
    GearRig.GetAnimInstance().SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);GearRig.AddTickPrerequisiteComponent(Model);GearRig.SetComponentTickEnabled(true);
   }
   else
   {
    auto Asset=Cast<UStaticMesh>(UOpusAssets::Find(this,DWOModels::ObjectPath(G.Asset)));if(Asset==nullptr){Tower.Fail("3C equipment asset "+G.Asset);return;}
    (I==0?Weapon:Offhand).SetStaticMesh(Asset);
   }
   auto Component=GearComponent(I);Component.AttachToComponent(Model,G.Socket,EAttachmentRule::SnapToTarget,EAttachmentRule::SnapToTarget,EAttachmentRule::SnapToTarget,false);
   Component.SetRelativeTransform(FTransform(G.Rotation,G.Offset,G.Scale));Component.SetVisibility(true);
  }
  EquipmentReady=Gear.Num()>0||DWO3CEquipment::BuiltInCount(Appearance.Id)>0;
 }
 bool IsActionState() const {return AnimationState==n"attack1"||AnimationState==n"attack2"||AnimationState==n"hit";}
 bool Committed() const {return IsActionState()&&ActionRemaining>0;}
 double GaitSpeed(FName Key) const
 {
  return Math::Max(1.0,(HasGaitCalibration?DWO3CGaits::Speed(Appearance.Id,Key):Appearance.Gait)*Appearance.Scale);
 }
 double LocomotionRate(FName Key) const {return DWO3CGaits::Striding(Appearance.Id)?Math::Clamp(ActualSpeed/GaitSpeed(Key),0.0,3.0):1;}
 void SetPoseRate(double Rate)
 {
  ActionRate=Rate;
  if(ActivePoseMontage!=nullptr&&Model.GetAnimInstance()!=nullptr)Model.GetAnimInstance().Montage_SetPlayRate(ActivePoseMontage,float32(Rate));
 }
 void Play(FName Key,bool Loop,double Rate=1)
 {
  if(!Clips.Contains(Key)||!PoseMontages.Contains(Key))return;
  if(AnimationState==Key&&(Loop||Committed())){SetPoseRate(Rate);return;}
  FName From=AnimationState;AnimationState=Key;AnimationStarts++;LocomotionSince=Clock;
  auto Next=PoseMontages[Key];
  if(!SlotReady){Model.PlayAnimation(Next,Loop);SlotReady=true;}
  else
  {
   auto Anim=Model.GetAnimInstance();
   if(ActivePoseMontage!=nullptr&&(From==n"run"||From==n"walk"))Anim.Montage_SetPlayRate(ActivePoseMontage,float32(Key==n"run"||Key==n"walk"?LocomotionRate(From):0));
   FAlphaBlendArgs Blend;Blend.BlendTime=Key==n"death"?0.12:0.22;Blend.BlendOption=EAlphaBlendOption::Cubic;
   if(Anim.Montage_PlayWithBlendIn(Next,Blend,float32(Rate))<=0){Tower.Fail("3C pose play "+Appearance.Id+" "+Key);return;}PoseTransitions++;
  }
  ActivePoseMontage=Next;auto Anim=Model.GetAnimInstance();Anim.SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
  Anim.Montage_SetNextSection(n"Default",Loop?n"Default":NAME_None,Next);SetPoseRate(Rate);
  ActionRemaining=Loop?0:Clips[Key].GetPlayLength();
 }
 void StopTravel(){CharacterMovement.StopMovementImmediately();ConsumeMovementInputVector();}
 void ClearTravelGuard()
 {
  Blocked=false;TravelRequested=false;ProgressClock=0;ClearFor=0;ProgressOrigin=ActorLocation;FilteredTravel=FVector::ZeroVector;
 }
 void CommitPose(FName Key,FVector Target,double Rate)
 {
  StopTravel();ClearTravelGuard();Chasing=false;ActionYaw=ActorRotation.Yaw;ActionTarget=Target;Play(Key,false,Rate);
 }
 void PresentHit(bool Damaged,FVector Target,double Rate)
 {
  if(!Damaged)return;
  if(Committed()||Clock<NextHitReaction){CoalescedHits++;return;}
  CommitPose(n"hit",Target,Rate);NextHitReaction=Clock+Math::Max(0.8,ActionRemaining/Math::Max(0.1,Rate)+0.25);
 }
 bool HoldForClearance(double Dt,FVector To,bool WantsTravel)
 {
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
     if(Progress.DotProduct(ProgressDirection)<Math::Max(4.0,Speed*ProgressClock*0.1)&&Progress.Size2D()<Math::Max(12.0,Speed*ProgressClock*0.25))
     {Blocked=true;BlockedSince=Clock;ClearFor=0;BlockedEntries++;FilteredTravel=FVector::ZeroVector;}
     ProgressOrigin=ActorLocation;ProgressDirection=To.GetSafeNormal2D();ProgressClock=0;
    }
   }
   TravelRequested=false;
  }
  if(!Blocked)return false;
  TArray<AActor> Ignore;Ignore.Add(this);FHitResult Hit;FVector Start=ActorLocation+FVector(0,0,2);
  bool Obstructed=System::CapsuleTraceSingleByProfile(Start,Start+To.GetSafeNormal2D()*Math::Max(65.0,Speed*0.28),
    CapsuleComponent.GetScaledCapsuleRadius(),CapsuleComponent.GetScaledCapsuleHalfHeight()-3,n"Pawn",false,Ignore,EDrawDebugTrace::None,Hit,true);
  ClearFor=Obstructed?0:ClearFor+Dt;
  if(ClearFor>=0.18&&Clock-BlockedSince>=0.3){ClearTravelGuard();return false;}
  StopTravel();Play(n"idle",true);return true;
 }
 void RefreshHealthBar(float Dt)
 {
  if(!HealthBar.Initialized){FVector Center,Extent;float32 Radius=0;System::GetComponentBounds(Model,Center,Extent,Radius);HealthBarHeight=Math::Max(70.0,Center.Z+Extent.Z-ActorLocation.Z+24);}
  if(Dead)HealthBar.Hide();else if(Rules!=nullptr)HealthBar.Read(Rules.GetTargetPools(EntityId),Dt);
 }
 void Die()
 {
  if(Dead)return;Dead=true;HealthBar.Hide();Tower.Kills++;StopTravel();CharacterMovement.DisableMovement();SetActorEnableCollision(false);
  if(Boss&&EventId.IsEmpty()&&Tower.Studio.Ready)Tower.Studio.BossDefeated(ActorLocation);
  Model.GlobalAnimRateScale=1;Play(n"death",false);CorpseIn=ActionRemaining+1;Rules.ForgetGameTarget(EntityId);
 }
 UFUNCTION(BlueprintOverride) void BeginPlay(){Body.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cube.Cube")));Body.SetRelativeScale3D(FVector(0.7,0.7,1.2));}
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds){AdvanceEnemy(DeltaSeconds);}
 void AdvanceEnemy(float DeltaSeconds)
 {
  if(Rules==nullptr||Tower==nullptr||Appearance.Mesh.IsEmpty())return;
  double Dt=Math::Max(0.0,double(DeltaSeconds));
  bool Frozen=Staged||Tower.Paused||Tower.Preparing;Model.GlobalAnimRateScale=Frozen?0:1;
  RefreshHealthBar(Frozen?0:DeltaSeconds);
  if(Frozen){StopTravel();PreviousPosition=ActorLocation;return;}
  if(Dead){CorpseIn-=Dt;if(CorpseIn<=0)DestroyActor();return;}
  auto P=Rules.GetTargetPools(EntityId);if(P.bActive&&P.bDead){Die();return;}
  if(!Tower.CanFight()||Tower.Hero==nullptr){StopTravel();PreviousPosition=ActorLocation;return;}
  if(CharacterMovement.MovementMode==EMovementMode::MOVE_None)CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
  Clock+=Dt;ActualTravelVelocity=(ActorLocation-PreviousPosition)/Math::Max(0.001,Dt);ActualTravelVelocity.Z=0;ActualSpeed=ActualTravelVelocity.Size2D();PreviousPosition=ActorLocation;
  float32 Remaining=0;bool Controlled=Rules.GetStatusEffect(EntityId,n"control",Remaining)>0;
  double PoseRate=Controlled?0:Math::Max(0.1,1+Rules.GetStatusEffect(EntityId,n"action_speed",Remaining)/100.0);
  if(IsActionState()){SetPoseRate(PoseRate);ActionRemaining=Math::Max(0.0,ActionRemaining-Dt*PoseRate);}
  else if(AnimationState==n"run"||AnimationState==n"walk")SetPoseRate(LocomotionRate(AnimationState));
  auto HeroPools=Rules.GetPlayerPools();FVector Target=Tower.Hero.CombatLocation(),ToHero=Target-ActorLocation;
  bool InAggroRange=Tower.Dungeon!=nullptr&&Tower.Dungeon.WithinMonsterAggroRange(ActorLocation,Target,EntityId);
  bool Clear=InAggroRange&&DWO3CGeometry::Sweep(ActorLocation,Target,1)<0;
  // Called exactly once every combat frame, even during a visual recovery.
  bool Attacked=Rules.TickMonsterSkill(EntityId,ActorLocation,Target,float32(Dt),!HeroPools.bDead&&Clear);
  if(Attacked){AttackSerial++;if(!Committed())CommitPose(AttackSerial%2==0&&Clips.Contains(n"attack2")?n"attack2":n"attack1",Target,PoseRate);}
  PresentHit(P.Life<LastLife||P.Shield<LastShield,Target,PoseRate);LastLife=P.Life;LastShield=P.Shield;
  if(!InAggroRange)
  {
   Chasing=false;DungeonPath.Empty();PathIndex=0;RepathIn=0;
   StopTravel();ClearTravelGuard();if(!Committed())Play(n"idle",true);return;
  }
  if(Committed()){StopTravel();ClearTravelGuard();SetActorRotation(FRotator(0,ActionYaw,0));return;}
  if(Controlled||HeroPools.bDead||Speed<=0){StopTravel();ClearTravelGuard();Play(n"idle",true);return;}
  double Distance=ToHero.Size2D();
  if(Chasing){if(Distance<StopDistance*0.78)Chasing=false;}else if(Distance>StopDistance*1.08)Chasing=true;
  RepathIn-=Dt;
  if(RepathIn<=0){DungeonPath=UOpusDungeon::Path(this,ActorLocation,Tower.Hero.ActorLocation);PathIndex=1;RepathIn=0.45+(EntityId%11)*0.025;}
  while(PathIndex<DungeonPath.Num()&&(DungeonPath[PathIndex]-ActorLocation).Size2D()<Math::Max(50.0,RuleTarget.RadiusCm+20)){PathIndex++;ClearTravelGuard();}
  FVector To=PathIndex<DungeonPath.Num()?DungeonPath[PathIndex]-ActorLocation:FVector::ZeroVector;
  if(UOpusNative::Flag("DWOHeroArena")&&UOpusNative::Flag("DWOTransient"))To=ToHero;
  bool WantsTravel=Chasing&&!To.IsNearlyZero();
  if(Blocked&&To.GetSafeNormal2D().DotProduct(ProgressDirection)<0.6)ClearTravelGuard();
  if(HoldForClearance(Dt,To,WantsTravel))return;
  FilteredTravel=Math::VInterpTo(FilteredTravel,ActualTravelVelocity,float32(Dt),8);
  bool StableHeading=ActualSpeed>40&&FilteredTravel.Size2D()>40&&FilteredTravel.DotProduct(To.GetSafeNormal2D())>0;
  FVector Heading=WantsTravel?(StableHeading?FilteredTravel:To):ToHero;double YawDelta=Math::FindDeltaAngleDegrees(ActorRotation.Yaw,Heading.Rotation().Yaw);
  if(WantsTravel&&Math::Abs(YawDelta)<6)YawDelta=0;
  double Step=Math::Clamp(YawDelta,-300.0*Dt,300.0*Dt);if(Distance>70)SetActorRotation(FRotator(0,ActorRotation.Yaw+Step,0));
  CharacterMovement.MaxWalkSpeed=float32(Speed*Rules.GetMonsterMoveScale(EntityId));
  if(WantsTravel&&Math::Abs(YawDelta-Step)<65){AddMovementInput(To.GetSafeNormal2D(),1);TravelRequested=true;}else StopTravel();
  bool WasMoving=AnimationState==n"run"||AnimationState==n"walk";bool Moving=WasMoving?ActualSpeed>2:ActualSpeed>6;
  bool UseWalk=Clips.Contains(n"walk")&&HasGaitCalibration&&ActualSpeed<GaitSpeed(n"walk")*(AnimationState==n"walk"?1.3:0.95);
  FName Next=Moving?(UseWalk?n"walk":n"run"):n"idle";
  if(Next==AnimationState||IsActionState()||Clock-LocomotionSince>=0.25)Play(Next,true,Moving?LocomotionRate(Next):1);
 }
}
