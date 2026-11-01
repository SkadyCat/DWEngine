struct FC3GaitCase
{
 FString Model;
 FName State;
 double Fraction=0.65;
}
// Straight-line locomotion probe: real mesh evaluation with the same playback
// rate function as combat. Ground-contact velocity is measured from world feet.
class AThreeCGaitProbe : AActor
{
 UPROPERTY() AThreeCGameMode Arena;
 UPROPERTY() AThreeCFighter Selected;
 TArray<FC3GaitCase> Cases;
 int CaseIndex=0;
 double Clock=0,Duration=0,Speed=0;
 FString CSV="model,state,fraction,synced,time,body_speed,play_rate,anim_time,x,left_x,left_y,left_z,right_x,right_y,right_z\n";
 void Setup(AThreeCGameMode InArena)
 {
  Arena=InArena;Arena.Paused=true;Arena.PoseModel="";Arena.AutoCamera=false;Arena.CameraMode=1;SetTickGroup(ETickingGroup::TG_PostUpdateWork);
  for(auto M:C3Models::All())if(C3Gaits::Striding(M.Id))
  {
   FC3GaitCase Run;Run.Model=M.Id;Run.State=n"run";Cases.Add(Run);
   if(M.Clips.Contains(n"walk")){FC3GaitCase Walk;Walk.Model=M.Id;Walk.State=n"walk";Walk.Fraction=0.45;Cases.Add(Walk);}
   if(M.Id=="cloth_grunt"||M.Id=="goblin_archer"||M.Id=="swordsman"){FC3GaitCase Slow;Slow.Model=M.Id;Slow.State=n"run";Slow.Fraction=0.2;Cases.Add(Slow);}
  }
  for(auto F:Arena.Fighters){F.SetActorTickEnabled(false);F.Mesh.SetComponentTickEnabled(false);F.SetActorHiddenInGame(true);F.StopTravel();F.CharacterMovement.SetMovementMode(EMovementMode::MOVE_None);}
  BeginCase();
 }
 void BeginCase()
 {
  if(Selected!=nullptr){Selected.Probing=false;Selected.SetActorTickEnabled(false);Selected.SetActorHiddenInGame(true);Selected.Mesh.SetComponentTickEnabled(false);}
  auto Case=Cases[CaseIndex/2];for(auto F:Arena.Fighters)if(F.ModelId==Case.Model){Selected=F;break;}
  Selected.Probing=true;Selected.SetActorTickEnabled(true);Selected.Mesh.SetComponentTickEnabled(true);Selected.Mesh.GlobalAnimRateScale=1;Selected.SetActorHiddenInGame(false);Arena.Focus=Selected.Index;
  Selected.SetActorLocation(FVector(0,0,85));Selected.SetActorRotation(FRotator::ZeroRotator);Selected.StopTravel();
  Selected.GaitSync=CaseIndex%2==1;Selected.SlotReady=false;Selected.State=NAME_None;Selected.ActionLeft=0;Selected.PoseBlending=true;
  Speed=Selected.Data.Gait*Case.Fraction;Selected.Play(Case.State,true,Selected.LocomotionRate(Case.State,Speed,1));
  double Rate=Speed/(C3Gaits::Speed(Case.Model,Case.State)*Selected.Data.Scale);
  Duration=Math::Max(2.2,Selected.Clips[Case.State].GetPlayLength()/Math::Max(0.05,Rate)*1.1+0.4);Clock=0;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Arena==nullptr)return;Clock+=DeltaSeconds;auto Case=Cases[CaseIndex/2];
  Selected.AddActorWorldOffset(FVector(Speed*DeltaSeconds,0,0));
  Selected.ActionRate=Selected.LocomotionRate(Case.State,Speed,DeltaSeconds);Selected.Mesh.SetPlayRate(float32(Selected.ActionRate));
  FVector L=Selected.Mesh.GetSocketLocation(n"foot_l"),R=Selected.Mesh.GetSocketLocation(n"foot_r");
  CSV+=Selected.ModelId+","+Case.State.ToString()+","+Case.Fraction+","+(Selected.GaitSync?1:0)+","+Clock+","+Speed+","+Selected.ActionRate+","+Selected.Mesh.GetPosition()+","+Selected.ActorLocation.X+","+L.X+","+L.Y+","+L.Z+","+R.X+","+R.Y+","+R.Z+"\n";
  if(Clock>=Duration)
  {
   CaseIndex++;
   if(CaseIndex<Cases.Num()*2)BeginCase();else
   {
    FString Output=FPaths::ProjectSavedDir()+"Diagnostics/gait-probe.csv";
    FFileHelper::SaveStringToFile(CSV,Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Log("C3_GAIT_PROBE_PASS variants="+Cases.Num()+" cases="+CaseIndex+" output="+Output);
    System::QuitGame(Gameplay::GetPlayerController(0),EQuitPreference::Quit,false);SetActorTickEnabled(false);
   }
  }
 }
}
