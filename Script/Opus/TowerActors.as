class ADWOHero : ACleanNetworkCharacter
{
 default CapsuleComponent.SetCapsuleSize(34,98);
 default bUseControllerRotationYaw=false;
 default CharacterMovement.bOrientRotationToMovement=false;
 default CharacterMovement.bUseControllerDesiredRotation=false;
 default CharacterMovement.RotationRate=FRotator(0,540,0);
 default CharacterMovement.MaxWalkSpeed=500;
 default CharacterMovement.MaxWalkSpeedCrouched=200;
 default CharacterMovement.MaxAcceleration=1700;
 default CharacterMovement.BrakingDecelerationWalking=2000;
 default CharacterMovement.BrakingFrictionFactor=1;
 default CharacterMovement.bUseSeparateBrakingFriction=true;
 default CharacterMovement.GravityScale=1.5;
 default CharacterMovement.JumpZVelocity=600;
 default CharacterMovement.AirControl=0.2;
 default CharacterMovement.bRunPhysicsWithNoController=true;
 default Mesh.SetCollisionProfileName(n"NoCollision");
 default Mesh.RelativeLocation=FVector(0,0,-98);
 default Mesh.RelativeRotation=FRotator(0,-90,0);
 default Mesh.VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 UPROPERTY(DefaultComponent) UStaticMeshComponent Body;
 default Body.SetCollisionProfileName(n"NoCollision");
 UPROPERTY() USkeletalMeshComponent PoseSource;
 UPROPERTY(DefaultComponent) USkeletalMeshComponent Wizard;
 default Wizard.SetCollisionProfileName(n"NoCollision");
 default Wizard.RelativeScale3D=FVector(1.1875);
 default Wizard.VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 UPROPERTY(DefaultComponent) UDWOHeroVisual Visual;
 UPROPERTY(DefaultComponent) UOpusCharacterEvents CharacterEvents;
 UPROPERTY(DefaultComponent) UDWOPlayerStun Stun;
 UPROPERTY(DefaultComponent) UOpusReactiveCombat ReactiveCombat;
 UPROPERTY(DefaultComponent) UDWOReactiveAuras ReactiveAuras;
 UPROPERTY(DefaultComponent) UOpusDeliveryPolicy DeliveryPolicy;
 UPROPERTY(DefaultComponent) UDWOSelfSkills SelfSkills;
 UPROPERTY(DefaultComponent) UDWOTopDownControl TopDown;
 UPROPERTY() ADWOHeroCombatTarget CombatTarget;

 UPROPERTY(DefaultComponent) USpringArmComponent CameraBoom;
 default CameraBoom.TargetArmLength=2200;
 default CameraBoom.SetAbsolute(false,true,false);
 default CameraBoom.bEnableCameraLag=false;
 default CameraBoom.CameraLagSpeed=10;
 default CameraBoom.RelativeRotation=FRotator(-60,0,0);
 default CameraBoom.RelativeLocation=FVector(0,0,75);
 default CameraBoom.bDoCollisionTest=false;
 UPROPERTY(DefaultComponent,Attach=CameraBoom,AttachSocket=SpringEndpoint) UCameraComponent Camera;
 default Camera.FieldOfView=60;
 UPROPERTY() UDWRuleWorldSubsystem Rules;
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() int OwnerSlot=-1;
 UPROPERTY() bool AutoCast=false;
 FVector Aim=FVector(1,0,0);
 FVector AimPoint=FVector(1000,0,60);
 bool ControlProbe=false;
 UPROPERTY() FString PickupTarget;
 TArray<FVector> PickupRoute;
 FString PickupRouteKey;
 int PickupWaypoint=0;
 FVector PickupDirection(FVector Goal)
 {
  if(PickupRouteKey!=PickupTarget)
  {
   PickupRouteKey=PickupTarget;PickupWaypoint=0;PickupRoute=UOpusDungeon::Path(Tower,ActorLocation,Goal);
   if(PickupRoute.Num()==0){PickupTarget="";Tower.LastError="无法抵达掉落附近";return FVector::ZeroVector;}
  }
  while(PickupWaypoint<PickupRoute.Num()&&(PickupRoute[PickupWaypoint]-ActorLocation).Size2D()<45)PickupWaypoint++;
  return ((PickupWaypoint<PickupRoute.Num()?PickupRoute[PickupWaypoint]:Goal)-ActorLocation).GetSafeNormal2D();
 }
 void PickLoot(FString Key)
 {
  if(Tower==nullptr||Rules==nullptr||Rules.GetPlayerPools().bDead)return;
  Visual.MovementPressed();
  if(PickupTarget!=Key)PickupRouteKey="";PickupTarget=Key;
  if(Tower.Hud!=nullptr&&Tower.Hud.Screen!=nullptr)Tower.Hud.Screen.SetFocus();
 }
 FVector CombatLocation() const {return ActorLocation-FVector(0,0,CapsuleComponent.GetScaledCapsuleHalfHeight()-60);}
 void EnsureCombatTarget()
 {
  if(IsValid(CombatTarget))return;
  CombatTarget=Cast<ADWOHeroCombatTarget>(SpawnActor(ADWOHeroCombatTarget,CombatLocation()));
  CombatTarget.Hero=this;CombatTarget.SetOwner(this);
  CombatTarget.Root.AttachToComponent(CapsuleComponent,NAME_None,EAttachmentRule::SnapToTarget,EAttachmentRule::SnapToTarget,EAttachmentRule::SnapToTarget,false);
  CombatTarget.Root.RelativeLocation=FVector(0,0,60-CapsuleComponent.GetScaledCapsuleHalfHeight());
  CombatTarget.AddTickPrerequisiteComponent(CharacterMovement);
 }
 UFUNCTION(BlueprintOverride) void InitializeCleanCharacter()
 {
  PoseSource=Mesh;EnsureCombatTarget();
  Wizard.AttachToComponent(Mesh,NAME_None,EAttachmentRule::SnapToTarget,EAttachmentRule::SnapToTarget,EAttachmentRule::SnapToTarget,false);
  Wizard.RelativeScale3D=FVector(1.1875);
  Body.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cylinder.Cylinder")));
  Body.SetRelativeScale3D(FVector(0.75,0.75,1.2));UOpusNative::Tint(Body,FLinearColor(0.95,0.75,0.2));
  Visual.Initialize(this);TopDown.Initialize(this);CharacterMovement.DisableMovement();
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){if(IsValid(CombatTarget))CombatTarget.DestroyActor();}
 bool IsCastTargetValid(ADWOEnemy Enemy) const
 {
  if(Rules==nullptr||Tower==nullptr||!IsValid(Enemy)||Enemy.Tower!=Tower||Enemy.Rules!=Rules||Enemy.Staged||Enemy.Dead)return false;
  auto P=Rules.GetTargetPools(Enemy.EntityId);return P.bActive&&!P.bDead&&P.Life>0;
 }
 ADWOEnemy ResolveCastTarget(FVector Point) const
 {
  if(Tower==nullptr||Rules==nullptr)return nullptr;
  ADWOEnemy Target;double Best=1e30;
  for(auto Enemy:Tower.Enemies)
  {
   if(!IsValid(Enemy))continue;double Distance=(Enemy.ActorLocation-Point).SizeSquared2D();double Radius=Enemy.RuleTarget.RadiusCm;
   // Use the existing rule target footprint for cursor/point selection.
   // An arbitrary floor point is not an enemy target.
   if(Distance<=Radius*Radius&&Distance<Best&&IsCastTargetValid(Enemy)){Best=Distance;Target=Enemy;}
  }
  return Target;
 }
 UFUNCTION(BlueprintCallable) bool CastAt(FVector Origin,FVector Target,float Power=30,bool AutomaticTarget=false)
 {
  if(Rules==nullptr||Tower==nullptr||!Tower.CanUseSkills())return false;
  return Visual.RequestCast(Origin,Target,Power,AutomaticTarget);
 }
 void AdvanceCastInput(bool Casting,double DeltaSeconds,bool ManualInput=false)
 {
  bool Held=Casting&&Tower.CanUseSkills();
  bool AutomaticTarget=AutoCast&&!ManualInput;
  // Manual input aims at a world point. Only automatic targeting needs an enemy.
  if(Held&&AutomaticTarget)Held=Tower.CanFight()&&(SelfSkills.FreeTarget()||(Visual.PendingCast&&IsCastTargetValid(Visual.CastEnemy))||ResolveCastTarget(AimPoint)!=nullptr);
  Rules.SetPlayerCasting(Held);
  auto Pools=Rules.GetPlayerPools();int Plan=UOpusNative::ActivePlan(Rules);
  // Authored action End owns animation readiness. RequestCast also keeps all
  // native cooldown/control checks; input polling adds no separate attack timer.
  if(Held&&Pools.ActionScale>0&&Plan>=0)CastAt(CombatLocation()+Aim*60,AimPoint,30,AutomaticTarget);
 }
 UFUNCTION(BlueprintOverride) void TickCleanCharacter(float DeltaSeconds)
 {
  if(Rules==nullptr||Tower==nullptr||OwnerSlot<0)return;
  auto PC=Cast<APlayerController>(GetController());if(PC==nullptr)return;
  Tower.Recent.SyncAuras();
  if(ControlProbe&&UOpusNative::Flag("DWOTransient")){Rules.UpdateOwner(OwnerSlot,CombatLocation()+Aim*60,Aim);UOpusNative::TargetPoint(Rules,OwnerSlot,AimPoint);return;}
  if(Tower.Studio.Ready&&Tower.Studio.UI.HandleKeys(PC)){TopDown.Stop(true);Rules.SetPlayerCasting(false);return;}
  if(Tower.Recent.UI!=nullptr&&Tower.Recent.UI.HandleKeys(PC)){TopDown.Stop(true);Rules.SetPlayerCasting(false);return;}
  if(Tower.Events.HandleKeys(PC)){TopDown.Stop(true);Rules.SetPlayerCasting(false);return;}
  if(Tower.WorldUI!=nullptr&&Tower.WorldUI.TextFocused()){if(PC.WasInputKeyJustPressed(EKeys::Escape))Tower.WorldUI.FilterKey(EKeys::Escape);Visual.MovementPressed();TopDown.Stop(true);Rules.SetPlayerCasting(false);return;}
  if(UOpusUI::TextFocused(Tower.Hud.Screen)){if(PC.WasInputKeyJustPressed(EKeys::Escape))Tower.Hud.Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Rules)+1));Visual.MovementPressed();TopDown.Stop(true);Rules.SetPlayerCasting(false);return;}
  if(Tower.Hud!=nullptr)Tower.Hud.HandleKeys(PC);
  if(PC.WasInputKeyJustPressed(EKeys::Escape)&&!Tower.HudKeyConsumed)Tower.SetPaused(!Tower.Paused);
  if(PC.WasInputKeyJustPressed(EKeys::R)&&Tower.Run.Phase==EDWOTowerPhase::Failed)Tower.Retry();
  if(PC.WasInputKeyJustPressed(EKeys::N)&&Tower.Run.Phase==EDWOTowerPhase::Cleared)Tower.Hud.MapDevice.Open();
  if(PC.WasInputKeyJustPressed(EKeys::Enter))Tower.StartChallenge();
  if(Tower.Paused||UOpusUI::TextFocused(Tower.Hud.Screen)||UOpusUI::BlocksCombat(Tower.Hud.Screen)){if(!Tower.Paused)Visual.MovementPressed();TopDown.Stop(true);Rules.SetPlayerCasting(false);return;}
  if(!TopDown.InputReady(PC)){TopDown.Stop();Rules.SetPlayerCasting(false);return;}
  if(PC.WasInputKeyJustPressed(EKeys::CapsLock))TopDown.bWalk=!TopDown.bWalk;
  TopDown.UpdateRollAimRecovery(DeltaSeconds);
  FString Error;
  TArray<FKey> Keys;Keys.Add(EKeys::One);Keys.Add(EKeys::Two);Keys.Add(EKeys::Three);Keys.Add(EKeys::Four);
  if(!Tower.HudKeyConsumed)for(int I=0;I<4;I++)if(PC.WasInputKeyJustPressed(Keys[I])){if(I!=UOpusNative::Selected(Rules))Visual.MovementPressed();Rules.SelectPlayerSkill(I,Error);if(Tower.Hud.Screen!=nullptr)Tower.Hud.Screen.HandleInputKey(Keys[I]);}
  FVector Move=TopDown.MoveDirection((PC.IsInputKeyDown(EKeys::W)?1.0:0.0)-(PC.IsInputKeyDown(EKeys::S)?1.0:0.0),(PC.IsInputKeyDown(EKeys::D)?1.0:0.0)-(PC.IsInputKeyDown(EKeys::A)?1.0:0.0));
  if(!Move.IsNearlyZero()&&(PC.WasInputKeyJustPressed(EKeys::W)||PC.WasInputKeyJustPressed(EKeys::A)||PC.WasInputKeyJustPressed(EKeys::S)||PC.WasInputKeyJustPressed(EKeys::D)))Visual.MovementPressed();
  if(!Move.IsNearlyZero())PickupTarget="";
  if(Tower.Studio.Ready)Move=Tower.Studio.Approach(Move);
  if(!PickupTarget.IsEmpty())
  {
   bool Found=false;
   for(auto Loot:UOpusNative::Loot(Rules))if(Loot.Key==PickupTarget&&!Loot.Claimed)
   {
    Found=true;
    if((Loot.Position-ActorLocation).Size2D()<=UOpusNative::PickupRadius())
    {
     if(UOpusNative::Pickup(Rules,Loot.Key,ActorLocation,Error)){Tower.SaveProfile();Tower.Hud.RefreshIn=0;UOpusNative::WriteLog("TowerPickup key="+Loot.Key);}
     Tower.LastError=Error;PickupTarget="";TopDown.Stop();
    }
    else Move=PickupDirection(Loot.Position);
    break;
   }
   if(!Found)PickupTarget="";
  }
  auto Pools=Rules.GetPlayerPools();double Dt=Math::Clamp(double(DeltaSeconds),0.0,0.05);
  if(Visual.Ready&&Tower.Dungeon!=nullptr&&Tower.Dungeon.Ready&&!Tower.Preparing&&Tower.Run.Phase!=EDWOTowerPhase::Failed&&!Pools.bDead)
  {
   TopDown.Move(Move,Pools.MoveScale,Dt);
  }
  else TopDown.Suspend();
  bool PointerBlocked=UOpusUI::BlocksPointer(Tower.Hud.Screen)||UOpusWorldUI::PointerBlocked(PC)||Tower.WorldUI.BlocksPointer(PC)||(Tower.Studio.Ready&&Tower.Studio.UI.BlocksPointer());
  bool ManualCasting=!PointerBlocked&&(PC.IsInputKeyDown(EKeys::LeftMouseButton)||PC.IsInputKeyDown(EKeys::SpaceBar));
  bool AutomaticTarget=AutoCast&&!ManualCasting;
  if(!AutomaticTarget&&!PointerBlocked)TopDown.MouseAim(PC);
  if(AutomaticTarget){auto Target=TopDown.SelectAutoTarget(DeltaSeconds);if(Target!=nullptr)TopDown.AimAt(Target.ActorLocation);}
  if(!Pools.bDead&&!Visual.RotationLocked()&&!TopDown.bRecoveringRollAim)TopDown.Face(Aim.Rotation().Yaw);
  if(Tower.CanFight()&&Visual.CanDodge()&&PC.WasInputKeyJustPressed(EKeys::LeftShift)&&Rules.DodgePlayer())
  {
   Visual.Dodged(Move.IsNearlyZero()?Aim:Move.GetSafeNormal());
  }
  Rules.UpdateOwner(OwnerSlot,CombatLocation()+Aim*60,Aim);UOpusNative::TargetPoint(Rules,OwnerSlot,AimPoint);
  bool Casting=AutoCast||ManualCasting;
  AdvanceCastInput(Casting,Dt,ManualCasting);
  TArray<FKey> AuraKeys;AuraKeys.Add(EKeys::Q);AuraKeys.Add(EKeys::E);AuraKeys.Add(EKeys::T);AuraKeys.Add(EKeys::G);
  if(!Tower.HudKeyConsumed)for(int I=0;I<4;I++)if(PC.WasInputKeyJustPressed(AuraKeys[I])&&!Tower.Recent.AuraSlots[I].IsNone())Tower.Recent.Toggle(I);
  TArray<FKey> BeltKeys;BeltKeys.Add(EKeys::Q);BeltKeys.Add(EKeys::E);BeltKeys.Add(EKeys::T);BeltKeys.Add(EKeys::G);
  if(!Tower.HudKeyConsumed)for(int I=0;I<4;I++)if(PC.WasInputKeyJustPressed(BeltKeys[I])&&Tower.Recent.AuraSlots[I].IsNone())Rules.UsePlayerItem(UOpusNative::Belt(Rules,I),Error);
  if(PC.WasInputKeyJustPressed(EKeys::P))for(auto Loot:UOpusNative::Loot(Rules))if(!Loot.Claimed&&(Loot.Position-ActorLocation).Size2D()<=UOpusNative::PickupRadius()){if(UOpusNative::Pickup(Rules,Loot.Key,ActorLocation,Error)){Tower.SaveProfile();Tower.Hud.RefreshIn=0;}Tower.LastError=Error;break;}
 }
}
