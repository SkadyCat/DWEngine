// Actual evaluated bones and attached components through complete clip ranges.
class AThreeCEquipmentProbe : AActor
{
 UPROPERTY() AThreeCGameMode Arena;
 UPROPERTY() AThreeCFighter Selected;
 TArray<FString> Models;
 TArray<FName> States;
 int CaseIndex=0;
 double Clock=0;
 FString CSV="model,state,slot,kind,time,follow_error,grip_error,ready,collision,parent_ok,pose_time,clip_length,hand_x,hand_y,hand_z\n";
 void Setup(AThreeCGameMode InArena)
 {
  Arena=InArena;Arena.Paused=true;Arena.PoseModel="";Arena.AutoCamera=false;Arena.CameraMode=1;SetTickGroup(ETickingGroup::TG_PostUpdateWork);
  States.Add(n"idle");States.Add(n"run");States.Add(n"attack1");States.Add(n"hit");States.Add(n"death");
  for(auto M:C3Models::All())Models.Add(M.Id);
  for(auto F:Arena.Fighters){F.Probing=true;F.StopTravel();F.SetActorTickEnabled(false);F.Mesh.SetComponentTickEnabled(false);F.SetActorHiddenInGame(true);F.CharacterMovement.SetMovementMode(EMovementMode::MOVE_None);}
  BeginCase();
 }
 void BeginCase()
 {
  if(Selected!=nullptr){Selected.SetActorTickEnabled(false);Selected.Mesh.SetComponentTickEnabled(false);Selected.SetActorHiddenInGame(true);}
  FString Model=Models[CaseIndex/States.Num()];for(auto F:Arena.Fighters)if(F.ModelId==Model){Selected=F;break;}
  FName State=States[CaseIndex%States.Num()];
  Selected.SetActorLocation(FVector(0,0,85));Selected.SetActorRotation(FRotator::ZeroRotator);Selected.SetActorHiddenInGame(false);Selected.SetActorTickEnabled(true);Selected.Mesh.SetComponentTickEnabled(true);Selected.Mesh.GlobalAnimRateScale=1;
  Selected.State=NAME_None;Selected.ActionLeft=0;Selected.SlotReady=false;Selected.PoseBlending=true;
  Selected.Play(State,false);Selected.Mesh.SetPlayRate(0);Arena.Focus=Selected.Index;Clock=0;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Arena==nullptr)return;
  if(Clock>=0.08)
  {
   double PoseTime=Selected.Mesh.GetAnimInstance().Montage_GetPosition(Selected.ActivePoseMontage);
   double Length=Selected.Clips[Selected.State].GetPlayLength();
   FVector Hand=Selected.Mesh.GetSocketLocation(n"hand_r");
   if(Selected.Gear.Num()==0)CSV+=Selected.ModelId+","+Selected.State.ToString()+",-1,built-in,"+Clock+",0,0,"+(Selected.EquipmentReady?1:0)+",0,1,"+PoseTime+","+Length+","+Hand.X+","+Hand.Y+","+Hand.Z+"\n";
   for(int I=0;I<Selected.Gear.Num();I++)
   {
    auto Component=Selected.GearComponent(I);auto Primitive=Cast<UPrimitiveComponent>(Component);
    bool ParentOK=Component.GetAttachParent()==Selected.Mesh&&Component.GetAttachSocketName()==Selected.Gear[I].Socket;
    Hand=Selected.Mesh.GetSocketLocation(Selected.Gear[I].Hand);
    CSV+=Selected.ModelId+","+Selected.State.ToString()+","+I+","+Selected.Gear[I].Kind+","+Clock+","+Selected.EquipmentFollowError(I)+","+Selected.EquipmentGripError(I)+","+(Selected.EquipmentReady?1:0)+","+(Primitive.GetCollisionEnabled()==ECollisionEnabled::NoCollision?0:1)+","+(ParentOK?1:0)+","+PoseTime+","+Length+","+Hand.X+","+Hand.Y+","+Hand.Z+"\n";
   }
  }
  // Loading a previously unused montage must not skip all intermediate poses.
  // Seek the next pose, then let the real skeletal component evaluate it.
  Clock+=Math::Min(double(DeltaSeconds),0.05);
  Selected.Mesh.GetAnimInstance().Montage_SetPosition(Selected.ActivePoseMontage,float32(Selected.Clips[Selected.State].GetPlayLength()*Math::Min(1.0,Clock/0.85)));
  if(Clock>=1.05)
  {
   CaseIndex++;
   if(CaseIndex<Models.Num()*States.Num())BeginCase();else
   {
    FString Output=FPaths::ProjectSavedDir()+"Diagnostics/equipment-probe.csv";
    FFileHelper::SaveStringToFile(CSV,Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Log("C3_EQUIPMENT_PROBE_PASS models="+Models.Num()+" cases="+CaseIndex+" output="+Output);
    System::QuitGame(Gameplay::GetPlayerController(0),EQuitPreference::Quit,false);SetActorTickEnabled(false);
   }
  }
 }
}
