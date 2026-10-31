// Opt-in transient native coverage, with real slots, bones, capsule movement,
// rules and equipment. No test path writes the formal player profile.
class ADWO3CBarrier : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) UStaticMeshComponent Body;
 default Body.SetCollisionProfileName(n"BlockAll");
 void Place(FVector At,FVector Scale){Body.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cube.Cube")));SetActorLocation(At);Body.SetRelativeScale3D(Scale);}
}
class UDWO3CIntegrationTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TArray<ADWOEnemy> Subjects;
 UPROPERTY() ADWOEnemy Selected;
 UPROPERTY() ADWOEnemy Blocker;
 UPROPERTY() ADWO3CBarrier Gate;
 UPROPERTY() ADWO3CBarrier LeftWall;
 UPROPERTY() ADWO3CBarrier RightWall;
 TArray<FString> Assets;
 TArray<FVector> PreviousHands;
 TArray<FVector> PreviousHeads,PreviousLeft;
 TArray<int> Intermediate;
 int Stage=0,ClipIndex=0,GripSamples=0,StableFrames=0,Starts=0,BeforeCasts=0,GuardCase=0;
 double Clock=0,Total=0,MaxGripError=0,HoldYaw=0,HoldX=0,FrozenPose=0,ReleaseX=0;
 bool Done=false,EncounterStarted=false;
 int ActiveFrames=0;
 double ActiveSeconds=0,SlowestFrame=0;
 bool Check(bool Good,FString Why){if(Good)return true;Done=true;UOpusNative::WriteLog("THREEC_FAIL "+Why);UOpusNative::Quit(false);return false;}
 void Pass(FString Why){UOpusNative::WriteLog("THREEC_PASS "+Why);}
 void Next(int Value){Stage=Value;Clock=0;}
 void Initialize(ADWOTowerGameMode Game)
 {
  Tower=Game;SetTickGroup(ETickingGroup::TG_PostUpdateWork);Tower.Hero.ControlProbe=true;Tower.Rules.SetPlayerGod(true);
  for(auto Id:DWO3CModels::All())DWO3CAssets::Prepare(Id,Assets);UOpusAssets::Request(this,Assets);
 }
 ADWOEnemy SpawnSubject(FString Id,int Index)
 {
  FOpusMonsterData Data;for(auto D:UOpusNative::Monsters(Tower.Rules))if(D.Id==n"claw_beast"){Data=D;break;}
  Data.Health=1000000;auto Enemy=Cast<ADWOEnemy>(SpawnActor(ADWOEnemy,FVector((Index%9-4)*250,(Index/9-1)*300,85)));
  Enemy.Rules=Tower.Rules;Enemy.Tower=Tower;Enemy.EntityId=++Tower.NextId;Enemy.RuleTarget.RadiusCm=34;Enemy.Speed=0;
  TArray<FOpusSkillData> Skills;FString Error;
  if(!Check(UOpusNative::RegisterEnemy(Tower.Rules,Enemy,Enemy.EntityId,Data,n"monster_rarity_common",1,34,"",Skills,Error),"register "+Id+" "+Error))return Enemy;
  Enemy.ConfigureAppearance(Id);Enemy.StopTravel();Enemy.CharacterMovement.DisableMovement();Enemy.SetActorEnableCollision(false);Enemy.SetActorTickEnabled(false);Tower.Enemies.Add(Enemy);return Enemy;
 }
 void Shot(FString Name){if(UOpusNative::Flag("DWO3CCapture"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/ThreeC/"+Name+".png -nosuffix");}
 void CheckEquipment()
 {
  for(auto E:Subjects)for(int I=0;I<E.Gear.Num();I++)
  {
   auto G=E.Gear[I];auto C=E.GearComponent(I);FTransform Expected=FTransform(G.Rotation,G.Offset,G.Scale)*E.Model.GetSocketTransform(G.Socket,ERelativeTransformSpace::RTS_World);
   FVector Grip=C.WorldTransform.TransformPosition(G.AssetGrip),Hand=E.Model.GetSocketTransform(G.Hand,ERelativeTransformSpace::RTS_World).TransformPosition(G.HandGrip);
   MaxGripError=Math::Max(MaxGripError,(Grip-Hand).Size());GripSamples++;
   if(!Check(C.GetAttachParent()==E.Model&&(C.WorldLocation-Expected.Location).Size()<0.05&&(Grip-Hand).Size()<0.05,"native evaluated equipment attachment "+E.Appearance.Id))return;
  }
 }
 void BeginBlockCase()
 {
  Selected=Subjects[1]; // Explicitly find the calibrated goblin bow below.
  for(auto E:Subjects)if(E.Appearance.Id=="goblin_archer")Selected=E;
  for(auto E:Subjects)if(E.Appearance.Id=="swordsman")Blocker=E;
  Selected.SetActorTickEnabled(true);Selected.SetActorEnableCollision(true);Selected.SetActorHiddenInGame(false);Selected.SetActorLocation(FVector(0,0,85));Selected.SetActorRotation(FRotator::ZeroRotator);
  Selected.ActionRemaining=0;Selected.Play(n"idle",true);Selected.Clock=0;Selected.Speed=260;Selected.StopDistance=130;Selected.PreviousPosition=Selected.ActorLocation;Selected.ClearTravelGuard();Selected.BlockedEntries=0;
  Selected.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);Selected.CharacterMovement.SetAvoidanceEnabled(true);
  Tower.Hero.SetActorLocation(FVector(700,0,101));Tower.Hero.TopDown.Stop();Tower.Hero.CharacterMovement.DisableMovement();
  Gate.SetActorEnableCollision(GuardCase==0);Gate.SetActorHiddenInGame(GuardCase!=0);
  Blocker.SetActorLocation(FVector(245,0,85));Blocker.SetActorEnableCollision(GuardCase==1);Blocker.SetActorHiddenInGame(GuardCase!=1);Blocker.CharacterMovement.DisableMovement();
  StableFrames=0;HoldX=0;HoldYaw=0;Next(10);
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Done||Tower==nullptr)return;Clock+=DeltaSeconds;Total+=DeltaSeconds;if(Total>160){Check(false,"timeout stage="+Stage);return;}
  if(Stage>0){ActiveFrames++;ActiveSeconds+=DeltaSeconds;SlowestFrame=Math::Max(SlowestFrame,double(DeltaSeconds));}
  if(Stage==0)
  {
   FString Error=UOpusAssets::Error(this,Assets);if(!Check(Error.IsEmpty(),Error))return;
   if(Tower.Preparing||!Tower.Hero.Visual.Ready||!UOpusAssets::Ready(this,Assets))return;
   // The low-FPS comparison begins after asset admission and the ordinary
   // encounter are ready; loading thousands of dependent poses is not combat.
   if(!EncounterStarted){EncounterStarted=true;Tower.StartChallenge();return;}
   if(!Tower.CanFight())return;
   System::ExecuteConsoleCommand("t.MaxFPS "+UOpusNative::IntOption("DWO3CFPS",60));
   for(auto E:Tower.Enemies)if(IsValid(E)){E.Staged=true;E.StopTravel();E.CharacterMovement.DisableMovement();E.SetActorTickEnabled(false);E.SetActorEnableCollision(false);E.SetActorHiddenInGame(true);}
   int Index=0;for(auto Id:DWO3CModels::All())Subjects.Add(SpawnSubject(Id,Index++));
   int Montages=0,Gear=0,Equipped=0;
   for(auto E:Subjects){Montages+=E.PoseMontages.Num();Gear+=E.Gear.Num();if(E.EquipmentReady)Equipped++;E.Model.GlobalAnimRateScale=1;E.Play(n"attack1",false);}
   if(!Check(Subjects.Num()==36&&Montages==229&&Gear==14&&Equipped==15,"all model/clip/equipment coverage models="+Subjects.Num()+" montages="+Montages+" gear="+Gear+" equipped="+Equipped))return;
   Pass("AssetsAndGear models=36 montages=229 external=14 equipped=15");Next(1);return;
  }
  if(Stage==1)
  {
   CheckEquipment();if(Done)return;
   if(Clock<1.1)return;
   TArray<FName> Keys;Keys.Add(n"attack1");Keys.Add(n"hit");Keys.Add(n"idle");Keys.Add(n"run");Keys.Add(n"death");
   ClipIndex++;if(ClipIndex<Keys.Num()){for(auto E:Subjects)E.Play(Keys[ClipIndex],ClipIndex==2||ClipIndex==3);Clock=0;return;}
   Pass("EvaluatedEquipment samples="+GripSamples+" maxGripCm="+MaxGripError);Shot("all-lineages");
   PreviousHands.SetNum(Subjects.Num());PreviousHeads.SetNum(Subjects.Num());PreviousLeft.SetNum(Subjects.Num());Intermediate.SetNumZeroed(Subjects.Num());
   for(auto E:Subjects){E.ActionRemaining=0;E.Play(n"attack1",false);E.Model.GetAnimInstance().Montage_SetPosition(E.ActivePoseMontage,float32(E.Clips[n"attack1"].GetPlayLength()*.5));E.SetPoseRate(0);}
   Next(2);return;
  }
  if(Stage==2)
  {
   if(Clock<.4)return;
   for(int I=0;I<Subjects.Num();I++){auto E=Subjects[I];PreviousHands[I]=E.Model.GetSocketLocation(n"hand_r");PreviousHeads[I]=E.Model.GetSocketLocation(n"head");PreviousLeft[I]=E.Model.GetSocketLocation(n"hand_l");E.Play(n"idle",true,0);}
   Next(3);return;
  }
  if(Stage==3)
  {
   for(int I=0;I<Subjects.Num();I++)
   {
    auto E=Subjects[I];FVector Now=E.Model.GetSocketLocation(n"hand_r"),Head=E.Model.GetSocketLocation(n"head"),Left=E.Model.GetSocketLocation(n"hand_l");
    double Change=(Now-PreviousHands[I]).Size()+(Head-PreviousHeads[I]).Size()+(Left-PreviousLeft[I]).Size();
    if(Clock<.28&&Change>.01)Intermediate[I]++;PreviousHands[I]=Now;PreviousHeads[I]=Head;PreviousLeft[I]=Left;
    if(!Check(E.Model.GetAnimInstance().Blueprint_GetSlotMontageLocalWeight(n"DefaultSlot")>.99,"slot keeps full weight "+E.Appearance.Id))return;
   }
   if(Clock<.4)return;
   for(int I=0;I<Subjects.Num();I++)if(DWO3CAssets::Calibrated(Subjects[I].Appearance.Id)&&!Check(Intermediate[I]>=2,"real intermediate bone poses "+Subjects[I].Appearance.Id+" count="+Intermediate[I]))return;
   Pass("NativePoseCrossfade fifteenCalibratedModels=1");Selected=Subjects[0];Selected.Clock=0;Selected.ActionRemaining=0;Selected.CommitPose(n"attack1",FVector(1000,0,85),1);Starts=Selected.AnimationStarts;
   Next(4);return;
  }
  if(Stage==4)
  {
   Selected.PresentHit(true,FVector(-1000,0,85),1);
   if(!Check(Selected.AnimationStarts==Starts&&Selected.AnimationState==n"attack1","repeated hurt cannot restart or cut committed attack"))return;
   if(Clock<.35)return;
   Selected.ActionRemaining=0;Selected.PresentHit(true,FVector(1000,0,85),1);Starts=Selected.AnimationStarts;
   if(!Check(Selected.AnimationState==n"hit","one hurt reaction begins when available"))return;
   Next(5);return;
  }
  if(Stage==5)
  {
   Selected.PresentHit(true,FVector(-1000,0,85),1);
   if(!Check(Selected.AnimationStarts==Starts,"continuous hurt coalesces"))return;
   if(Clock<.4)return;
   Pass("CommittedActionsAndCoalescedHurt count="+Selected.CoalescedHits);
   Selected.ActualSpeed=25;Selected.Play(n"walk",true,Selected.LocomotionRate(n"walk"));
   if(!Check(Math::Abs(Selected.ActionRate-25/95.212678748)<.0001,"walk uses its calibrated speed"))return;
   Selected.ActualSpeed=0;Selected.SetPoseRate(Selected.LocomotionRate(n"walk"));
   if(!Check(Selected.ActionRate==0,"zero travel freezes stride without minimum speed"))return;
   Pass("WalkRunActualDisplacementSync");
   LeftWall=Cast<ADWO3CBarrier>(SpawnActor(ADWO3CBarrier));LeftWall.Place(FVector(250,-95,100),FVector(12,.3,2));
   RightWall=Cast<ADWO3CBarrier>(SpawnActor(ADWO3CBarrier));RightWall.Place(FVector(250,95,100),FVector(12,.3,2));
   Gate=Cast<ADWO3CBarrier>(SpawnActor(ADWO3CBarrier));Gate.Place(FVector(245,0,100),FVector(.3,1.6,2));
   BeginBlockCase();return;
  }
  if(Stage==10)
  {
   if(Clock>2.5)
   {
    if(!Check(Selected.Blocked&&Selected.AnimationState==n"idle","contact settles into blocked wait case="+GuardCase+" x="+Selected.ActorLocation.X+" state="+Selected.AnimationState))return;
    if(StableFrames==0){HoldX=Selected.ActorLocation.X;HoldYaw=Selected.ActorRotation.Yaw;Starts=Selected.AnimationStarts;}
    if(!Check(Math::Abs(Selected.ActorLocation.X-HoldX)<.1&&Math::Abs(Math::FindDeltaAngleDegrees(HoldYaw,Selected.ActorRotation.Yaw))<.1&&Selected.AnimationStarts==Starts,"blocked motor keeps position facing and pose"))return;
    StableFrames++;
   }
   if(Clock<3.5)return;
   ReleaseX=Selected.ActorLocation.X;Gate.SetActorEnableCollision(false);Blocker.SetActorEnableCollision(false);Next(11);return;
  }
  if(Stage==11)
  {
   if(!Check(Clock<1.2,"released passage must resume"))return;
   if(Selected.ActorLocation.X-ReleaseX<25)return;
   Pass("BlockedClearance case="+GuardCase+" stableFrames="+StableFrames+" recovery="+Clock);Shot(GuardCase==0?"wall-release":"crowd-release");
   if(GuardCase++==0){Selected.StopTravel();BeginBlockCase();return;}
   LeftWall.DestroyActor();RightWall.DestroyActor();Gate.DestroyActor();
   Selected.StopTravel();Selected.Speed=0;Selected.ActionRemaining=0;Selected.Play(n"idle",true);Selected.SetActorLocation(Tower.Hero.CombatLocation()+FVector(-125,0,25));Selected.PreviousPosition=Selected.ActorLocation;
   BeforeCasts=Selected.AttackSerial;Next(12);return;
  }
  if(Stage==12)
  {
   if(!Check(Clock<8,"native monster attacks must still release"))return;
   if(Selected.AttackSerial<=BeforeCasts||!Selected.Committed())return;
   HoldYaw=Selected.ActorRotation.Yaw;HoldX=Selected.ActorLocation.X;Starts=Selected.AnimationStarts;
   // Move target around the live attacker during its committed action.
   Tower.Hero.SetActorLocation(Tower.Hero.ActorLocation+FVector(0,100,0));Next(13);return;
  }
  if(Stage==13)
  {
   if(Selected.Committed()&&!Check(Math::Abs(Math::FindDeltaAngleDegrees(HoldYaw,Selected.ActorRotation.Yaw))<.01&&Math::Abs(Selected.ActorLocation.X-HoldX)<.1,"attack locks yaw and travel"))return;
   if(Clock<.15)return;
   Tower.SetPaused(true);FrozenPose=Selected.Model.GetAnimInstance().Montage_GetPosition(Selected.ActivePoseMontage);Next(14);return;
  }
  if(Stage==14)
  {
   if(Clock<.3)return;
   if(!Check(Math::Abs(Selected.Model.GetAnimInstance().Montage_GetPosition(Selected.ActivePoseMontage)-FrozenPose)<.001,"pause freezes full montage channel"))return;
   Tower.SetPaused(false);Pass("NativeCastFacingAndPause");
   auto A=Subjects[2],B=Subjects[3];Tower.Hero.TopDown.AutoTarget=A;Tower.Hero.TopDown.AutoTargetHeld=0;
   Tower.Hero.SetActorLocation(FVector(0,0,101));A.SetActorLocation(FVector(600,0,85));B.SetActorLocation(FVector(590,0,85));
   for(auto E:Subjects)if(E!=A&&E!=B)E.SetActorLocation(FVector(6000,6000,85));
   if(!Check(Tower.Hero.TopDown.SelectAutoTarget(.2)==A,"slightly closer target cannot steal engagement"))return;
   B.SetActorLocation(FVector(100,0,85));
   if(!Check(Tower.Hero.TopDown.SelectAutoTarget(1.3)==B,"clearly closer target replaces after hold"))return;
   Pass("AutomaticTargetHysteresis");Shot("stable-combat");Next(15);return;
  }
  if(Stage==15)
  {
   if(Clock<.2)return;Selected.Die();if(!Check(Selected.Dead&&Selected.AnimationState==n"death"&&!Selected.HealthBar.Visible,"death immediately overrides presentation"))return;
   Pass("ImmediateDeath");double Average=ActiveSeconds/Math::Max(1,ActiveFrames);
   if(!Check(UOpusNative::IntOption("DWO3CFPS",60)!=15||Average>=.06,"low-frame comparison actually runs at 15 FPS"))return;
   Pass("Timing frames="+ActiveFrames+" averageDt="+Average+" maxDt="+SlowestFrame);Pass("Complete");Done=true;UOpusNative::Quit(true);
  }
 }
}
