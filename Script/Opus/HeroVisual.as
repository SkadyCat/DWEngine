// Zero's original MM graph, montage player and root-motion roll catalog.
// DWRules alone owns casts, damage, resources and status timers.
class UDWOHeroVisual : UActorComponent
{
 UPROPERTY() ADWOHero Hero;
 UPROPERTY() bool Ready=false;
 UPROPERTY() FString Failure;
 UPROPERTY() FName AnimationState;
 UPROPERTY() TMap<FName,UAnimSequence> Clips;
 UPROPERTY() UCleanRollAnimationSet RollSet;
 UPROPERTY() UCleanMotionAnimInstance Anim;
 UPROPERTY() int ActionState=0;
 UPROPERTY() int CastSerial=0,DodgeSerial=0,HitSerial=0;
 UPROPERTY() int AuthoredReleases=0;
 UPROPERTY() int CastRequests=0,CancelledCasts=0,RejectedCasts=0;
 UPROPERTY() int ActionRevision=0,ReleaseRevision=-1,ActionEndRevision=-1;
 UPROPERTY() UAnimSequence ActiveActionSequence;
 UPROPERTY() bool PendingCast=false;
 bool CastRequiresEnemy=false;
 UPROPERTY() ADWOEnemy CastEnemy;
 int CastPlan=-1;
 FVector CastOriginOffset,CastTarget,CastDirection;
 float CastPower=30;
 // Base casting motion is twice as fast; equipment cast-speed still multiplies it.
 UPROPERTY(EditAnywhere, Category="Tuning") double BaseCastPlayRate=2.0;
 UPROPERTY() FVector VisualVelocity;
 TMap<FName,FString> ClipPaths;
 TArray<FString> Assets;
 FString MeshPath="/Game/Characters/WizardHero/FingerRig/SK_WizardHero.SK_WizardHero";
 FString SourcePath="/Game/G2_Mage/Meshes/Characters/Combines/SK_Judy_D3.SK_Judy_D3";
 FString GraphPath="/Game/Clean3C/Animation/Channel/ABP_Clean3C_ChannelMobile.ABP_Clean3C_ChannelMobile_C";
 FString RetargetPath="/Game/Characters/WizardHero/StanceFix20261018/ABP_WizardRetarget.ABP_WizardRetarget_C";
 FString MaterialPath="/Game/Characters/WizardHero/Materials/Shimmer20261018/MI_WizardHero_Shimmer.MI_WizardHero_Shimmer";
 FString RollPath="/Game/Clean3C/Animation/Mage/Rolls/DA_Roll_Mocap114.DA_Roll_Mocap114";
 FString WeaponPath="/Game/Clean3C/Animation/WeaponLayers/DA_WeaponLayer_0.DA_WeaponLayer_0";
 double LoadingTime=0,RollCooldown=0,ActionRate=1;
 void AddClip(FName Key,FString Package,FString Name)
 {
  ClipPaths.Add(Key,DWOModels::ObjectPath(Package));Assets.AddUnique(DWOModels::ObjectPath(Package));
  Assets.AddUnique(DWOModels::ObjectPath("/Game/Clean3C/Animation/Montages/AM_"+Name));
 }
 void Initialize(ADWOHero InHero)
 {
  Hero=InHero;AddTickPrerequisiteActor(Hero);Hero.CharacterMovement.AddTickPrerequisiteComponent(this);
  Hero.Wizard.AddTickPrerequisiteComponent(Hero.Mesh);
  Assets.Add(MeshPath);Assets.Add(SourcePath);Assets.Add(GraphPath);Assets.Add(RetargetPath);Assets.Add(MaterialPath);Assets.Add(RollPath);Assets.Add(WeaponPath);
  AddClip(n"cast","/Game/Clean3C/Animation/Mage/Set/Set_Mage_Fireball_L_RM","Set_Mage_Fireball_L_RM");
  AddClip(n"hit","/Game/Clean3C/Animation/Mage/Set/Set_Mage_Hurt_IP","Set_Mage_Hurt_IP");
  AddClip(n"death","/Game/Clean3C/Animation/Mage/Set/Set_Mage_Death_IP","Set_Mage_Death_IP");
  TArray<FString> Names;Names.Add("F_0");Names.Add("F_R_45");Names.Add("R_90");Names.Add("B_R_45");Names.Add("B_180");Names.Add("B_L_45");Names.Add("L_90");Names.Add("F_L_45");
  for(int I=0;I<Names.Num();I++){FString Name="Mocap114_Dive_"+Names[I];AddClip(FName("roll_"+I),"/Game/Clean3C/Animation/Mage/Rolls/Mocap114/Dive/"+Name,Name);}
  UOpusAssets::Request(this,Assets);
 }
 void Fail(FString Error){Failure=Error;UOpusNative::WriteLog("HERO_VISUAL_ERROR "+Error);if(Hero!=nullptr&&Hero.Tower!=nullptr)Hero.Tower.Fail(Error);}
 void FinishLoading()
 {
  FCpuProfilerTraceScoped Scope(n"Opus_PrepareZero3C");
  auto Source=Cast<USkeletalMesh>(UOpusAssets::Find(this,SourcePath));auto Mesh=Cast<USkeletalMesh>(UOpusAssets::Find(this,MeshPath));
  auto Graph=Cast<UClass>(UOpusAssets::Find(this,GraphPath));auto Retarget=Cast<UClass>(UOpusAssets::Find(this,RetargetPath));
  auto Material=Cast<UMaterialInterface>(UOpusAssets::Find(this,MaterialPath));RollSet=Cast<UCleanRollAnimationSet>(UOpusAssets::Find(this,RollPath));
  auto Weapon=Cast<UCleanWeaponLayer>(UOpusAssets::Find(this,WeaponPath));
  if(Source==nullptr||Mesh==nullptr||Graph==nullptr||Retarget==nullptr||RollSet==nullptr||Weapon==nullptr){Fail("Missing original Zero 3C dependency");return;}
  for(auto C:ClipPaths){auto Clip=Cast<UAnimSequence>(UOpusAssets::Find(this,C.Value));if(Clip==nullptr){Fail("Missing Zero action "+C.Value);return;}Clips.Add(C.Key,Clip);}
  Hero.Mesh.SetSkinnedAssetAndUpdate(Source);Hero.Mesh.SetHiddenInGame(true,false);Hero.Mesh.SuspendClothingSimulation();
  Hero.Mesh.SetAnimInstanceClass(Graph);Anim=Cast<UCleanMotionAnimInstance>(Hero.Mesh.GetAnimInstance());
  if(Anim==nullptr){Fail("Zero MM graph did not create CleanMotionAnimInstance");return;}
  Anim.SetWeaponLayer(Weapon.IdlePose,Weapon.UpperBodyWeight);
  Hero.Wizard.SetSkinnedAssetAndUpdate(Mesh);if(Material!=nullptr)Hero.Wizard.SetMaterial(0,Material);Hero.Wizard.SetAnimInstanceClass(Retarget);
  Hero.Wizard.SetHiddenInGame(false,false);Hero.Body.SetVisibility(false);
  Ready=true;AnimationState=n"idle";
  UOpusNative::WriteLog("HERO_VISUAL_READY source=Zero native=CleanNetworkCharacter graph=ChannelMobile motionMatching=1 rolls=Mocap114");
 }
 bool RotationLocked() const
 {
  // A completed cast's visual blend-out is not another gameplay action.
  if(ActionState==0&&CanRestartCast())return false;
  return ActionState!=0||(Anim!=nullptr&&Anim.IsAnyMontageActiveIncludingBlendOut());
 }
 bool MovementLocked() const {return RotationLocked();}
 bool StartAction(FName Key,int State,double Rate=1)
 {
  if(!Ready||!Clips.Contains(Key)||Anim==nullptr)return false;
  if(!Anim.PlayCleanAction(Clips[Key],float(Math::Clamp(Rate,0.1,4.0)),0,false))return false;
  CancelPendingCast();ActionRevision++;ActiveActionSequence=Clips[Key];
  Hero.StopJumping();Hero.CharacterMovement.StopMovementImmediately();Hero.TopDown.bRecoveringRollAim=false;Hero.TopDown.RollAimAngularSpeed=0;
  Hero.SetNetworkActionLock(true,Hero.ActorRotation.Yaw);ActionRate=Math::Clamp(Rate,0.1,4.0);ActionState=State;AnimationState=Key;return true;
 }
 bool CanRestartCast() const {return ActionState==0&&!PendingCast&&ReleaseRevision==ActionRevision&&ActionEndRevision==ActionRevision;}
 void CancelPendingCast()
 {
  if(PendingCast)CancelledCasts++;PendingCast=false;CastPlan=-1;CastEnemy=nullptr;CastRequiresEnemy=false;
 }
 // Accept an action intent only. Resources and rule effects wait for the
 // original source animation's Attack Release marker, once per action revision.
 bool RequestCast(FVector Origin,FVector Target,float Power,bool AutomaticTarget=false)
 {
  if(!Ready||!Hero.HasAuthority()||!Hero.Tower.CanUseSkills()||Anim==nullptr||!Hero.CharacterMovement.IsMovingOnGround())return false;
  if(AutomaticTarget&&!Hero.Tower.CanFight())return false;
  if(ActionState!=0||RotationLocked())return false;
  auto P=Hero.Rules.GetPlayerPools();int Plan=UOpusNative::ActivePlan(Hero.Rules);
  if(Plan<0||P.bDead||P.ActionScale<=0||P.Cooldown>0)return false;
  if(!Hero.SelfSkills.CanCast())return false;
  bool RequireEnemy=AutomaticTarget&&!Hero.SelfSkills.FreeTarget();
  auto Enemy=Hero.ResolveCastTarget(Target);if(RequireEnemy&&Enemy==nullptr)return false;
  FVector Direction=(Target-Origin).GetSafeNormal2D();if(Direction.IsNearlyZero())Direction=Hero.Aim;
  double Rate=BaseCastPlayRate*Math::Max(0.1,1.0+Hero.Rules.GetPlanStat(Plan,"cast.speed_percent")/100);
  Hero.SetRootMotionTranslationYaw(0);Hero.SetCleanRootMotionScale(1);
  if(!StartAction(n"cast",4,Rate))return false;
  CastPlan=Plan;CastEnemy=Enemy;CastRequiresEnemy=RequireEnemy;CastOriginOffset=Origin-Hero.CombatLocation();CastTarget=Target;CastDirection=Direction;CastPower=Power;
  Hero.Aim=Direction;Hero.AimPoint=Target;Hero.SetActorRotation(Direction.Rotation());Hero.SetNetworkActionLock(true,Direction.Rotation().Yaw);
  PendingCast=true;CastRequests++;return true;
 }
 void HandleAttackRelease(UAnimSequenceBase Animation)
 {
  if(!Hero.HasAuthority()||ActionState!=4||Animation!=ActiveActionSequence||!PendingCast||ReleaseRevision==ActionRevision||Anim==nullptr||!Anim.IsCleanActionActive())return;
  if(CastRequiresEnemy&&!Hero.IsCastTargetValid(CastEnemy)){MovementPressed();return;}
  auto P=Hero.Rules.GetPlayerPools();
  if(!Hero.Tower.CanUseSkills()||P.bDead||P.ActionScale<=0||CastPlan!=UOpusNative::ActivePlan(Hero.Rules)){CancelPendingCast();return;}
  // Retire the intent before calling native code: re-entrant/duplicate notifies
  // and later recovery notifies cannot pay for or emit this cast again.
  PendingCast=false;ReleaseRevision=ActionRevision;AuthoredReleases++;
  FVector Origin=Hero.SelfSkills.LaunchOrigin(Hero.CombatLocation()+CastOriginOffset,CastDirection);
  Hero.Rules.UpdateOwner(Hero.OwnerSlot,Origin,CastDirection);UOpusNative::TargetPoint(Hero.Rules,Hero.OwnerSlot,CastTarget);
  bool Accepted=Hero.Rules.CastBuildPower(CastPlan,Hero.OwnerSlot,Origin,CastDirection,CastPower)>=0;
  if(Accepted){CastSerial++;Hero.SelfSkills.Released();}else RejectedCasts++;
  if(UOpusNative::Flag("DWODebug"))UOpusNative::WriteLog("HERO_CAST_RELEASE revision="+ActionRevision+" progress="+Anim.GetCleanActionProgress()+" accepted="+Accepted+" manaBefore="+P.Mana+" manaAfter="+Hero.Rules.GetPlayerPools().Mana);
 }
 void HandleActionEnd(UAnimSequenceBase Animation)
 {
  if(!Hero.HasAuthority()||ActionState!=4||Animation!=ActiveActionSequence||PendingCast||ReleaseRevision!=ActionRevision)return;
  ActionEndRevision=ActionRevision;
  if(UOpusNative::Flag("DWODebug"))UOpusNative::WriteLog("HERO_ACTION_END revision="+ActionRevision+" progress="+Anim.GetCleanActionProgress());
  // Retire the action at its authored marker. The existing inertialization
  // smooths the remaining pose without retaining root motion or input locks.
  Anim.CancelCleanActionForMovement();CancelPendingCast();ActionState=0;ActiveActionSequence=nullptr;
  Hero.SetCleanRootMotionScale(1);Hero.SetRootMotionTranslationYaw(0);
  Hero.SetNetworkActionLock(false,Hero.ActorRotation.Yaw,false);
 }
 bool CanDodge() const
 {
  if(!Ready||Anim==nullptr||Hero.Rules.GetPlayerPools().bDead||Hero.Rules.GetPlayerPools().ActionScale<=0||RollCooldown>0||ActionState==1||!Hero.CharacterMovement.IsMovingOnGround())return false;
  if(ActionState!=0&&Anim.GetCleanActionProgress()<(ActionState==4?0.75:0.65))return false;return true;
 }
 FCleanRollClip RollEntry(int Index) const
 {
  if(Index==1)return RollSet.ForwardRight;if(Index==2)return RollSet.Right;if(Index==3)return RollSet.BackRight;
  if(Index==4)return RollSet.Back;if(Index==5)return RollSet.BackLeft;if(Index==6)return RollSet.Left;if(Index==7)return RollSet.ForwardLeft;return RollSet.Forward;
 }
 void Dodged(FVector WorldDirection)
 {
  if(!Ready||RollSet==nullptr)return;
  // Port of Zero clean3c/roll.lua index/correction. CMC extracts real root motion.
  FVector Direction=WorldDirection.IsNearlyZero()?Hero.ActorForwardVector:WorldDirection.GetSafeNormal2D();
  double Angle=Math::Fmod(Direction.Rotation().Yaw-Hero.ActorRotation.Yaw+360.0,360.0);
  int Index=Math::FloorToInt((Angle+22.5)/45.0)%8;auto Entry=RollEntry(Index);if(Entry.Animation==nullptr)return;
  double MotionYaw=Math::UnwindDegrees(Direction.Rotation().Yaw-Hero.ActorRotation.Yaw-Entry.MotionYaw);
  Hero.SetRootMotionTranslationYaw(float(MotionYaw));Hero.SetCleanRootMotionScale(RollSet.DistanceScale);
  if(StartAction(FName("roll_"+Index),1,RollSet.PlayRate)){DodgeSerial++;Hero.ConsumeMovementInputVector();}
 }
 bool MovementPressed()
 {
  if(Hero!=nullptr&&Hero.Rules!=nullptr&&Hero.Rules.GetPlayerPools().ActionScale<=0)return false;
  if(!Ready||ActionState==0||ActionState==1||ActionState==8)return false;
  // Zero CancelForMovement stops root extraction and blends the previous pose.
  CancelPendingCast();Anim.CancelCleanActionForMovement();ActionState=0;ActiveActionSequence=nullptr;Hero.SetCleanRootMotionScale(1);Hero.SetRootMotionTranslationYaw(0);
  Hero.TopDown.bRecoveringRollAim=true;Hero.TopDown.RollAimAngularSpeed=0;Hero.BeginNetworkRollRecovery(540);
  Hero.CharacterMovement.StopMovementImmediately();Hero.SetNetworkActionLock(false,Hero.ActorRotation.Yaw,false);return true;
 }
 void Stunned(double Seconds)
 {
  CancelPendingCast();if(!Ready||Anim==nullptr)return;
  Hero.SetCleanRootMotionScale(1);Hero.SetRootMotionTranslationYaw(0);HitSerial++;
  StartAction(n"hit",7,Clips[n"hit"].GetPlayLength()/Math::Max(0.05,Seconds));
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Hero==nullptr||!Failure.IsEmpty())return;
  if(!Ready)
  {
   UOpusAssets::Pump(this);
   LoadingTime+=DeltaSeconds;FString Error=UOpusAssets::Error(this,Assets);if(!Error.IsEmpty()){Fail(Error);return;}
   if(LoadingTime>120){Fail("Zero 3C streaming timed out");return;}
   if(Hero.Rules!=nullptr&&UOpusAssets::Ready(this,Assets))FinishLoading();return;
  }
  auto P=Hero.Rules.GetPlayerPools();double Dt=Math::Clamp(double(DeltaSeconds),0.0,0.1);VisualVelocity=Hero.GetVelocity();
  if(PendingCast&&CastRequiresEnemy&&!Hero.IsCastTargetValid(CastEnemy))MovementPressed();
  bool Frozen=Hero.Tower.Preparing||(Hero.Tower.Paused&&!P.bDead);Hero.Mesh.bPauseAnims=Frozen;Hero.Wizard.bPauseAnims=Frozen;if(Frozen)return;
  if(!Hero.Tower.CanUseSkills()||P.bDead)CancelPendingCast();
  RollCooldown=Math::Max(0.0,RollCooldown-Dt);
  if(P.bDead)
  {
   if(ActionState!=8){Hero.SetCleanRootMotionScale(1);Hero.SetRootMotionTranslationYaw(0);StartAction(n"death",8);Hero.CharacterMovement.DisableMovement();}
   return;
  }
  if(ActionState!=0)Anim.Montage_SetPlayRate(nullptr,float(ActionRate*(ActionState==7?1:Math::Max(0.0,P.ActionScale))));
  if(ActionState==7&&Hero.Stun.Remaining()<=0)MovementPressed();
  if(ActionState!=0&&!Anim.IsCleanActionActive())
  {
   CancelPendingCast();bool Rolled=ActionState==1;ActionState=0;ActiveActionSequence=nullptr;Hero.SetCleanRootMotionScale(1);Hero.SetRootMotionTranslationYaw(0);
   Hero.CharacterMovement.StopMovementImmediately();Hero.SetNetworkActionLock(false,Hero.ActorRotation.Yaw);
   if(Rolled){RollCooldown=0.2;Hero.TopDown.bRecoveringRollAim=true;Hero.TopDown.RollAimAngularSpeed=0;Hero.BeginNetworkRollRecovery(540);}
  }
  if(ActionState==0)AnimationState=VisualVelocity.Size2D()>5?n"locomotion":n"idle";
 }
}
