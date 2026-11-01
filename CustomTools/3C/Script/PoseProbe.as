// Isolated native-pose verification, selected explicitly with -C3PoseProbe=on.
// The probe freezes a recovery pose and the destination idle so observed motion
// can only come from UE's actual montage blend, rather than clip playback.
class AThreeCPoseProbe : AActor
{
 UPROPERTY() AThreeCGameMode Arena;
 UPROPERTY() AThreeCFighter Selected;
 TArray<FString> Models;
 int CaseIndex=0;
 double Clock=0,TransitionAt=-1;
 bool Switched=false;
 FString CSV="model,blended,time,transition_time,slot_weight,head_x,head_y,head_z,left_x,left_y,left_z,right_x,right_y,right_z\n";
 void Setup(AThreeCGameMode InArena)
 {
  Arena=InArena;Arena.Paused=true;Arena.PoseModel="";Arena.AutoCamera=false;Arena.CameraMode=1;
  SetTickGroup(ETickingGroup::TG_PostUpdateWork);
  for(auto M:C3Models::All())if(C3::Option("C3ProbeModel").IsEmpty()||C3::Option("C3ProbeModel")==M.Id)Models.Add(M.Id);
  for(auto F:Arena.Fighters){F.SetActorTickEnabled(false);F.Mesh.SetComponentTickEnabled(false);F.SetActorHiddenInGame(true);F.StopTravel();F.CharacterMovement.SetMovementMode(EMovementMode::MOVE_None);}
  BeginCase();
 }
 void BeginCase()
 {
  if(Selected!=nullptr){Selected.Probing=false;Selected.SetActorTickEnabled(false);Selected.SetActorHiddenInGame(true);Selected.Mesh.SetComponentTickEnabled(false);}
  FString Model=Models[CaseIndex/2];for(auto F:Arena.Fighters)if(F.ModelId==Model){Selected=F;break;}
  Arena.Focus=Selected.Index;Selected.SetActorHiddenInGame(false);Selected.Mesh.SetComponentTickEnabled(true);Selected.Mesh.GlobalAnimRateScale=1;
  Selected.Probing=true;Selected.SetActorTickEnabled(true);
  Selected.SetActorLocation(FVector(0,0,85));Selected.SetActorRotation(FRotator::ZeroRotator);
  Selected.PoseBlending=CaseIndex%2==1;Selected.SlotReady=false;Selected.State=NAME_None;Selected.ActionLeft=0;
  Selected.Play(n"attack1",false);
  double Position=Selected.Clips[n"attack1"].GetPlayLength()*0.98;
  if(Selected.UsesPoseBlends())Selected.Mesh.GetAnimInstance().Montage_SetPosition(Selected.ActivePoseMontage,float32(Position));else Selected.Mesh.SetPosition(float32(Position),false);
  Selected.Mesh.SetPlayRate(0);Clock=0;TransitionAt=-1;Switched=false;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Arena==nullptr)return;Clock+=DeltaSeconds;
  if(!Switched&&Clock>=0.8)
  {
   Switched=true;TransitionAt=Clock;Selected.Play(n"idle",true);Selected.Mesh.SetPlayRate(0);
  }
  if(Clock>=0.45)
  {
   auto Mesh=Selected.Mesh;FVector H=Mesh.GetSocketLocation(n"head"),L=Mesh.GetSocketLocation(n"hand_l"),R=Mesh.GetSocketLocation(n"hand_r");
   double Weight=Selected.UsesPoseBlends()?Mesh.GetAnimInstance().Blueprint_GetSlotMontageLocalWeight(n"DefaultSlot"):1;
   CSV+=Selected.ModelId+","+(Selected.PoseBlending?1:0)+","+Clock+","+(Switched?Clock-TransitionAt:-1)+","+Weight+","+H.X+","+H.Y+","+H.Z+","+L.X+","+L.Y+","+L.Z+","+R.X+","+R.Y+","+R.Z+"\n";
  }
  if(Clock>=1.65)
  {
   CaseIndex++;
   if(CaseIndex<Models.Num()*2)BeginCase();else
   {
    FString Output=FPaths::ProjectSavedDir()+"Diagnostics/pose-probe.csv";
    FFileHelper::SaveStringToFile(CSV,Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Log("C3_POSE_PROBE_PASS models="+Models.Num()+" cases="+CaseIndex+" output="+Output);
    System::QuitGame(Gameplay::GetPlayerController(0),EQuitPreference::Quit,false);SetActorTickEnabled(false);
   }
  }
 }
}
