// Native saved-resource inspection only, separate from the combat acceptance probe.
class ADWORecentVFXGalleryMode : AGameModeBase
{
 default DefaultPawnClass=ASpectatorPawn;
 UPROPERTY() ACameraActor Camera;
 UPROPERTY() ADWWebVFXActor Sword;
 UPROPERTY() ADWWebVFXActor Gate;
 double Clock=0;int Stage=0;
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  if(!UOpusNative::Flag("DWOTransient")){UOpusNative::Quit(false);return;}
  SpawnActor(ADWOHeroProbeRoom);
  Camera=SpawnActor(ACameraActor,FVector(-300,-1600,1500));Camera.SetActorRotation((FVector(0,0,500)-Camera.ActorLocation).Rotation());Gameplay::GetPlayerController(0).SetViewTargetWithBlend(Camera,0);
  Sword=SpawnActor(ADWWebVFXActor,FVector(200,0,600));Sword.SetActorRotation(FRotator(-25,0,0));Sword.PlayRate=0;Sword.bLoop=false;
  Sword.Prefab=Cast<UDWWebVFXPrefab>(LoadObject(nullptr,"/Game/WebVFX/Prefabs/projectile_treasury_ea/DA_projectile_treasury_ea.DA_projectile_treasury_ea"));
  Gate=SpawnActor(ADWWebVFXActor,FVector(200,0,600));Gate.SetActorRotation(Sword.ActorRotation);Gate.bLoop=false;
  Gate.Prefab=Cast<UDWWebVFXPrefab>(LoadObject(nullptr,"/Game/WebVFX/Variants/DA_treasury_ea_gate.DA_treasury_ea_gate"));
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;if(Clock>.1)Gate.PlayRate=0;if(Clock<2)return;
  if(Stage==0){System::ExecuteConsoleCommand("Shot filename="+FPaths::ProjectSavedDir()+"Verification/UEIntegration/treasury-saved-prefab.png -nosuffix");Stage=1;Clock=0;return;}
  if(Clock>.5){bool Ok=Sword.Prefab!=nullptr&&Gate.Prefab!=nullptr;UOpusNative::WriteLog(Ok?"GALLERY_TEST_PASS":"GALLERY_TEST_FAIL");UOpusNative::Quit(Ok);}
 }
}
