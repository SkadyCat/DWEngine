// Visual-only transient fixture: render the saved shipping wind entry at birth and later.
class ADWOConstantWindSample : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UNiagaraComponent Effect;
 default Effect.bAutoActivate=false;
 void Configure()
 {
  auto Map=Cast<UDWUEVFXMap>(LoadObject(nullptr,"/Game/WebVFX/DA_UEVFXMap.DA_UEVFXMap"));auto Entry=Map.Effects[n"whirlwind"];
  Effect.SetAsset(Cast<UNiagaraSystem>(Entry.System.ToSoftObjectPath().TryLoad()));Effect.SetRelativeScale3D(Entry.Scale);
  for(auto Pair:Entry.UserFloats)Effect.SetVariableFloat(Pair.Key,Pair.Value);
  for(auto Pair:Entry.UserColors)Effect.SetVariableLinearColor(Pair.Key,Pair.Value);
  for(auto Pair:Entry.UserBools)Effect.SetVariableBool(Pair.Key,Pair.Value);
  Effect.Activate(true);
 }
}
class ADWOWhirlwindBirthProbeMode : AGameModeBase
{
 default DefaultPawnClass=ASpectatorPawn;
 UPROPERTY() ADWOConstantWindSample Sample;
 UPROPERTY() ACameraActor Camera;
 double Clock=0;
 int Stage=0;
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  if(!UOpusNative::Flag("DWOTransient")){UOpusNative::Quit(false);return;}
  SpawnActor(ADWOHeroProbeRoom);
  Camera=SpawnActor(ACameraActor,FVector(0,-1300,1050));Camera.SetActorRotation((FVector(0,0,160)-Camera.ActorLocation).Rotation());Gameplay::GetPlayerController(0).SetViewTargetWithBlend(Camera,0);
  Sample=SpawnActor(ADWOConstantWindSample,FVector(0,0,2));Sample.Configure();
 }
 void Capture(FString Name){System::ExecuteConsoleCommand("Shot filename="+FPaths::ProjectSavedDir()+"Verification/WhirlwindParity/DungeonGround/wind-"+Name+".png -nosuffix");UOpusNative::WriteLog("WIND_BIRTH_CAPTURE "+Name);}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Math::Clamp(double(Dt),0.0,.05);
  if(Stage==0&&Clock>=8){Sample.Effect.Activate(true);Stage=1;Clock=0;}
  if(Stage==1&&Clock>=.1){Capture("010");Stage=2;}
  if(Stage==2&&Clock>=.7){Capture("070");Stage=3;}
  if(Stage==3&&Clock>=2){Capture("200");Stage=4;}
  if(Stage==4&&Clock>=3){UOpusNative::WriteLog("WIND_BIRTH_DONE");UOpusNative::Quit(true);}
 }
}
