namespace C3
{
 FString Option(FString Key,FString Fallback="")
 {
  TArray<FString> Tokens,Switches;FCommandLine::Parse(FCommandLine::Get(),Tokens,Switches);for(auto S:Switches)if(S.StartsWith(Key+"="))return S.RightChop(Key.Len()+1);return Fallback;
 }
}
class AThreeCController : APlayerController { default bShowMouseCursor=true; }
class AThreeCView : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UCameraComponent Camera;
 default Camera.FieldOfView=55;
}
class AThreeCSceneFill : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UDirectionalLightComponent Fill;
 default Fill.RelativeRotation=FRotator(-40,150,0);
 default Fill.Intensity=1.8;
 default Fill.SetCastShadows(false);
}
class AThreeCGameMode : AGameModeBase
{
 default DefaultPawnClass=nullptr;
 default PlayerControllerClass=AThreeCController;
 default HUDClass=AThreeCHUD;
 UPROPERTY() TArray<AThreeCFighter> Fighters;
 UPROPERTY() AThreeCView View;
 UPROPERTY() bool Controlled=true,Paused=false;
 UPROPERTY() int Focus=0,CameraMode=0;
 double Clock=0,RealClock=0,SensorClock=0,FlushClock=0,Seconds=0,LastSampleReal=0,FrameMS=0;
 FString Tag,CSV,Output,PoseModel,PoseState,ShotName,LastControl;
 double PoseTime=0,ShotAt=4,ControlClock=0,PoseAngle=0.4;
 bool Completed=false,AutoCamera=false,ShotTaken=false;
 int CameraPass=0;
 FLinearColor TeamColor(int Team) const{return Team==0?FLinearColor(1,0.26,0.18):Team==1?FLinearColor(0.18,0.58,1):FLinearColor(0.24,1,0.40);}
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  SetTickGroup(ETickingGroup::TG_PostUpdateWork);
  System::ExecuteConsoleCommand("r.DynamicGlobalIlluminationMethod 0");System::ExecuteConsoleCommand("r.ReflectionMethod 0");System::ExecuteConsoleCommand("sg.ShadowQuality 1");
  EnsureFill();
  Controlled=C3::Option("C3Mode","controlled")!="opus";Tag=C3::Option("C3Tag","live");Seconds=String::Conv_StringToDouble(C3::Option("C3Seconds","0"));
  System::ExecuteConsoleCommand("t.MaxFPS "+C3::Option("C3FPS","60"));
  Output=FPaths::ProjectSavedDir()+"Diagnostics/"+Tag+".csv";
  FFileHelper::SaveStringToFile("time,mode,id,team,model,state,health,speed,turn_deg_s,x,y,z,foot_l_z,foot_r_z,foot_l_speed,foot_r_speed,bounds_bottom,bounds_height,feet_valid,attacks,hits,deaths,real_time,frame_ms,sample_dt,root_offset,anim_time,target_id,target_changes,short_target_changes,action_retargets,anim_starts,hit_restarts,short_clip_cuts,action_turn_frames,abrupt_turn_frames,turn_reversals,signed_turn_deg_s,blocked,block_entries\n",Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  auto Models=C3Models::All();
  TArray<AActor> Placed;Gameplay::GetAllActorsOfClass(AThreeCFighter,Placed);
  for(int I=0;I<100;I++)
  {
   int Team=I%3,Slot=I/3;double Angle=Team*2.09439510239;FVector Center=FVector(Math::Cos(Angle)*1850,Math::Sin(Angle)*1850,85);
   FVector At=Center+FVector((Slot%6-2.5)*150,(Slot/6-2.5)*150,0);
   AThreeCFighter F;for(auto A:Placed){auto Candidate=Cast<AThreeCFighter>(A);if(Candidate!=nullptr&&Candidate.Index==I){F=Candidate;break;}}
   if(F==nullptr)F=Cast<AThreeCFighter>(SpawnActor(AThreeCFighter,At));Fighters.Add(F);F.Configure(this,Models[(I/3+Team*5)%Models.Num()],I,Team,At);
  }
  int Equipped=0,ExternalGear=0,BuiltInGear=0;for(auto F:Fighters){if(F.EquipmentReady)Equipped++;ExternalGear+=F.Gear.Num();BuiltInGear+=C3Equipment::BuiltInCount(F.ModelId);}
  if(Equipped!=100)Error("C3_EQUIPMENT_READY_FAILED units="+Equipped);
  Log("C3_EQUIPMENT_READY units="+Equipped+" external="+ExternalGear+" builtin="+BuiltInGear);
  View=Cast<AThreeCView>(SpawnActor(AThreeCView,FVector(3900,-4400,5200)));View.SetActorRotation((FVector(0,0,80)-View.ActorLocation).Rotation());
  auto PC=Gameplay::GetPlayerController(0);PC.SetViewTargetWithBlend(View,0);AutoCamera=C3::Option("C3Camera")=="cycle";CameraMode=C3::Option("C3Camera")=="closeup"?1:0;
  ShotName=C3::Option("C3Shot");ShotAt=String::Conv_StringToDouble(C3::Option("C3ShotAt","4"));
  PoseModel=C3::Option("C3Pose");PoseState=C3::Option("C3State","idle");PoseTime=String::Conv_StringToDouble(C3::Option("C3PoseTime","0.75"));
  PoseAngle=String::Conv_StringToDouble(C3::Option("C3Angle","0.4"));FFileHelper::LoadFileToString(LastControl,FPaths::ProjectSavedDir()+"control.txt");
  if(!PoseModel.IsEmpty())
  {
   InspectPose(PoseModel,PoseState,PoseTime,PoseAngle);
  }
  Log("C3_READY count="+Fighters.Num()+" factions=34/33/33 models="+Models.Num()+" mode="+(Controlled?"controlled":"opus"));
  if(C3::Option("C3PoseProbe")=="on"){auto Probe=Cast<AThreeCPoseProbe>(SpawnActor(AThreeCPoseProbe));Probe.Setup(this);}
  if(C3::Option("C3GaitProbe")=="on"){auto Probe=Cast<AThreeCGaitProbe>(SpawnActor(AThreeCGaitProbe));Probe.Setup(this);}
  if(C3::Option("C3BlockProbe")=="on"){auto Probe=Cast<AThreeCBlockProbe>(SpawnActor(AThreeCBlockProbe));Probe.Setup(this);}
  if(C3::Option("C3EquipmentProbe")=="on"){auto Probe=Cast<AThreeCEquipmentProbe>(SpawnActor(AThreeCEquipmentProbe));Probe.Setup(this);}
 }
 void EnsureFill(){TArray<AActor> Lights;Gameplay::GetAllActorsOfClass(AThreeCSceneFill,Lights);if(Lights.Num()==0)SpawnActor(AThreeCSceneFill);TArray<AActor> Suns;Gameplay::GetAllActorsOfClass(ADirectionalLight,Suns);for(auto A:Suns){auto D=Cast<UDirectionalLightComponent>(A.GetComponentByClass(UDirectionalLightComponent));if(D!=nullptr)D.SetForwardShadingPriority(1);}}
 void ResumeArena()
 {
  PoseModel="";Paused=false;CameraMode=0;for(auto F:Fighters){F.SetActorHiddenInGame(false);F.SetActorTickEnabled(true);F.Mesh.SetComponentTickEnabled(true);F.Reset();}
 }
 void InspectPose(FString Model,FString Clip,double Phase,double Angle)
 {
  AThreeCFighter Selected;for(auto F:Fighters)if(F.ModelId==Model){Selected=F;break;}
  if(Selected==nullptr||!Selected.Clips.Contains(FName(Clip))){Warning("C3_BAD_POSE "+Model+" "+Clip);return;}
  PoseModel=Model;PoseState=Clip;PoseTime=Phase;PoseAngle=Angle;Paused=true;AutoCamera=false;CameraMode=1;Focus=Selected.Index;
  for(auto F:Fighters)
  {
   bool IsSelected=F==Selected;F.SetActorHiddenInGame(!IsSelected);F.SetActorTickEnabled(IsSelected);F.Mesh.SetComponentTickEnabled(IsSelected);
  }
  Selected.SetActorLocation(FVector(0,0,85));Selected.SetActorRotation(FRotator::ZeroRotator);Selected.Play(FName(Clip),false);Selected.Mesh.SetPosition(float32(Selected.Clips[FName(Clip)].GetPlayLength()*Phase),false);Selected.Mesh.SetPlayRate(0);Selected.ActualSpeed=0;Selected.TurnRate=0;
  Log("C3_POSE_READY model="+PoseModel+" state="+PoseState+" phase="+PoseTime+" angle="+PoseAngle);
 }
 void Control()
 {
  FString Request;if(!FFileHelper::LoadFileToString(Request,FPaths::ProjectSavedDir()+"control.txt")||Request==LastControl)return;LastControl=Request;
  TArray<FString> Parts;Request.TrimStartAndEnd().ParseIntoArray(Parts," ");if(Parts.Num()<2)return;FString Op=Parts[1];
  if(Op=="resume"||Op=="tour"){ResumeArena();AutoCamera=Op=="tour";}
  else if(Op=="overview"){AutoCamera=false;CameraMode=0;}
  else if(Op=="mode"&&Parts.Num()>2){ResumeArena();SetMode(Parts[2]=="controlled");}
  else if(Op=="pose"&&Parts.Num()>=6)InspectPose(Parts[2],Parts[3],String::Conv_StringToDouble(Parts[4]),String::Conv_StringToDouble(Parts[5]));
  else if(Op=="focus"&&Parts.Num()>2){AutoCamera=false;CameraMode=1;for(auto F:Fighters)if(F.ModelId==Parts[2]){Focus=F.Index;break;}}
  else if(Op=="shot"&&Parts.Num()>2)System::ExecuteConsoleCommand("HighResShot 1 filename="+FPaths::ProjectSavedDir()+"Screenshots/"+Parts[2]+".png");
  else if(Op=="audit")System::ExecuteConsoleCommand("py "+FPaths::ProjectDir()+"Tools/dump_runtime.py");
  else if(Op=="equipmentaudit")System::ExecuteConsoleCommand("py "+FPaths::ProjectDir()+"Tools/audit_equipment.py");
  else if(Op=="equipmentbuild")System::ExecuteConsoleCommand("py "+FPaths::ProjectDir()+"Tools/prepare_equipment.py");
  else if(Op=="equipmentrefresh"){for(auto F:Fighters)F.EquipVisuals();Log("C3_EQUIPMENT_REFRESH_PASS count="+Fighters.Num());}
  else if(Op=="lighting")EnsureFill();
  else if(Op=="sensors")
  {
   auto F=Fighters[Focus];FVector L=F.Mesh.GetSocketLocation(n"foot_l"),R=F.Mesh.GetSocketLocation(n"foot_r");double Before=L.Z;
   FVector Center,Extent;float32 Radius=0;System::GetComponentBounds(F.Mesh,Center,Extent,Radius);
   Log("C3_SENSOR model="+F.ModelId+" name="+F.Data.LeftFoot+" before="+Before+" after="+L.Z+" right="+R.Z+" direct="+F.Mesh.GetSocketLocation(n"foot_l").Z+" dynamic="+F.Mesh.GetSocketLocation(FName(F.Data.LeftFoot)).Z+" bounds="+Center.Z+" radius="+Radius);
  }
  Log("C3_CONTROL "+Request);
 }
 void SetMode(bool Value){Controlled=Value;for(auto F:Fighters)F.SetControlled(Value);Log("C3_MODE "+(Controlled?"controlled":"opus"));}
 void Sample()
 {
  double SampleDt=Math::Max(0.001,RealClock-LastSampleReal);LastSampleReal=RealClock;
  for(auto F:Fighters)
  {
   bool Valid=!F.Data.LeftFoot.IsEmpty()&&!F.Data.RightFoot.IsEmpty();FVector L,R;double LS=0,RS=0;
   if(Valid){L=F.Mesh.GetSocketLocation(FName(F.Data.LeftFoot));R=F.Mesh.GetSocketLocation(FName(F.Data.RightFoot));if(F.PreviousFeet){LS=(L-F.PreviousLeft).Size2D()/SampleDt;RS=(R-F.PreviousRight).Size2D()/SampleDt;}F.PreviousLeft=L;F.PreviousRight=R;F.PreviousFeet=true;}
   FVector Center,Extent;float32 Radius=0;System::GetComponentBounds(F.Mesh,Center,Extent,Radius);
   double RootOffset=(F.Mesh.GetSocketLocation(n"root")-F.Mesh.WorldLocation).Size2D();
   CSV+=""+Clock+","+(Controlled?"controlled":"opus")+","+F.Index+","+F.Team+","+F.ModelId+","+F.State.ToString()+","+F.Health+","+F.ActualSpeed+","+F.TurnRate+","+F.ActorLocation.X+","+F.ActorLocation.Y+","+F.ActorLocation.Z+","+L.Z+","+R.Z+","+LS+","+RS+","+(Center.Z-Extent.Z)+","+(Extent.Z*2)+","+(Valid?1:0)+","+F.AttackCount+","+F.HitCount+","+F.DeathCount+","+RealClock+","+FrameMS+","+SampleDt+","+RootOffset+","+F.Mesh.GetPosition()+","+(IsValid(F.Target)?F.Target.Index:-1)+","+F.TargetChanges+","+F.ShortTargetChanges+","+F.RetargetsWhileActing+","+F.AnimationStarts+","+F.HitRestarts+","+F.ShortClipCuts+","+F.ActionTurnFrames+","+F.AbruptTurnFrames+","+F.TurnReversals+","+F.SignedTurnRate+","+(F.Blocked?1:0)+","+F.BlockedEntries+"\n";
  }
 }
 void Flush(){if(!CSV.IsEmpty()){FFileHelper::SaveStringToFile(CSV,Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,uint32(EFileWrite::Append));CSV="";}}
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  double Dt=Controlled?double(DeltaSeconds):Math::Min(double(DeltaSeconds),0.05);FrameMS=DeltaSeconds*1000;RealClock+=DeltaSeconds;if(!Paused)Clock+=Dt;
  auto PC=Gameplay::GetPlayerController(0);if(PC==nullptr||View==nullptr)return;
  if(PC.WasInputKeyJustPressed(EKeys::P))Paused=!Paused;
  if(PC.WasInputKeyJustPressed(EKeys::C))SetMode(!Controlled);
  if(PC.WasInputKeyJustPressed(EKeys::V)){AutoCamera=false;CameraMode=(CameraMode+1)%3;}
  if(PC.WasInputKeyJustPressed(EKeys::Tab)){AutoCamera=false;Focus=(Focus+1)%Fighters.Num();CameraMode=1;}
  if(PC.WasInputKeyJustPressed(EKeys::R)){Clock=0;ResumeArena();}
  if(PC.WasInputKeyJustPressed(EKeys::LeftMouseButton)){FHitResult Hit;if(PC.GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility,false,Hit)){auto F=Cast<AThreeCFighter>(Hit.Actor);if(F!=nullptr){Focus=F.Index;CameraMode=1;AutoCamera=false;}}}
  if(AutoCamera){CameraMode=RealClock<9?0:1;int Pass=int(Math::FloorToInt(Math::Max(0.0,RealClock-9)/5));if(Pass!=CameraPass){CameraPass=Pass;auto Models=C3Models::All();FString Wanted=Models[Pass%Models.Num()].Id;for(auto F:Fighters)if(F.ModelId==Wanted&&!F.Dead){Focus=F.Index;break;}}}
  auto Selected=Fighters[Focus];
  if(CameraMode==0)
  {
   View.SetActorLocation(FVector(3900,-4400,5200));View.SetActorRotation((FVector(0,0,50)-View.ActorLocation).Rotation());
  }
  else if(CameraMode==1)
  {
   double Angle=!PoseModel.IsEmpty()?PoseAngle:AutoCamera?RealClock*0.12:1.0;FVector Look=Selected.ActorLocation+FVector(0,0,55);double Distance=650;
   if(!PoseModel.IsEmpty()){FVector Center,Extent;float32 Radius=0;System::GetComponentBounds(Selected.Mesh,Center,Extent,Radius);FVector Head=Selected.Mesh.GetSocketLocation(n"head");FVector Low=Center-Extent,High=Center+Extent;Low.X=Math::Min(Low.X,Head.X-25);Low.Y=Math::Min(Low.Y,Head.Y-25);Low.Z=Math::Min(Low.Z,Head.Z-25);High.X=Math::Max(High.X,Head.X+25);High.Y=Math::Max(High.Y,Head.Y+25);High.Z=Math::Max(High.Z,Head.Z+25);Look=(Low+High)*0.5;Distance=Math::Max(800.0,(High-Low).Size()*2.1);}
   FVector Goal=Look+FVector(Math::Cos(Angle)*Distance,Math::Sin(Angle)*Distance,Distance*0.5);
   View.SetActorLocation(Math::VInterpTo(View.ActorLocation,Goal,DeltaSeconds,4));View.SetActorRotation((Look-View.ActorLocation).Rotation());
  }
  else
  {
   FVector Move=FVector::ZeroVector;auto Rot=View.ActorRotation;FVector Forward=Rot.ForwardVector,Right=Rot.RightVector;
   if(PC.IsInputKeyDown(EKeys::W))Move+=Forward;if(PC.IsInputKeyDown(EKeys::S))Move-=Forward;if(PC.IsInputKeyDown(EKeys::D))Move+=Right;if(PC.IsInputKeyDown(EKeys::A))Move-=Right;
   if(PC.IsInputKeyDown(EKeys::E))Move.Z+=1;if(PC.IsInputKeyDown(EKeys::Q))Move.Z-=1;View.AddActorWorldOffset(Move.GetSafeNormal()*1600*Dt);
   if(PC.IsInputKeyDown(EKeys::RightMouseButton)){float32 X=0,Y=0;PC.GetInputMouseDelta(X,Y);Rot.Yaw+=X*0.2;Rot.Pitch=Math::Clamp(Rot.Pitch-Y*0.2,-85.0,70.0);View.SetActorRotation(Rot);}
  }
  if(!Paused&&(Seconds>0||RealClock<120)){SensorClock+=Dt;if(SensorClock>=0.1){SensorClock-=0.1;Sample();}}
  FlushClock+=DeltaSeconds;if(FlushClock>=1){FlushClock=0;Flush();}
  ControlClock+=DeltaSeconds;if(ControlClock>=0.25){ControlClock=0;Control();}
  if(!ShotTaken&&!ShotName.IsEmpty()&&RealClock>=ShotAt){ShotTaken=true;System::ExecuteConsoleCommand("HighResShot 1 filename="+FPaths::ProjectSavedDir()+"Screenshots/"+ShotName+".png");}
  if(Seconds>0&&(PoseModel.IsEmpty()?Clock:RealClock)>=Seconds&&!Completed){Completed=true;Flush();Log("C3_RUN_PASS count="+Fighters.Num()+" time="+Clock+" output="+Output);System::QuitGame(PC,EQuitPreference::Quit,false);}
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){Flush();}
}
