// Presentation-only inspection of the saved Niagara mapping. Never used by tower gameplay.
class ADWOWindVFXSample : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UNiagaraComponent Effect;
 UPROPERTY(DefaultComponent,Attach=Root) UTextRenderComponent Label;
 default Effect.bAutoActivate=false;
 void Configure(UDWUEVFXMap Map,FName Key)
 {
  auto Entry=Map.Effects[Key];
  Effect.SetAsset(Cast<UNiagaraSystem>(Entry.System.ToSoftObjectPath().TryLoad()));
  Effect.SetRelativeScale3D(Entry.Scale*FVector(1.4,1.4,1));
  Effect.SetRelativeLocation(Entry.Offset);
  for(auto Pair:Entry.UserFloats)Effect.SetVariableFloat(Pair.Key,Pair.Value);
  for(auto Pair:Entry.UserColors)Effect.SetVariableLinearColor(Pair.Key,Pair.Value);
  for(auto Pair:Entry.UserBools)Effect.SetVariableBool(Pair.Key,Pair.Value);
  Label.SetText(FText::FromString(Key.ToString()));Label.SetWorldSize(30);
  Label.SetRelativeLocation(FVector(0,0,450));Label.SetRelativeRotation(FRotator(0,-90,0));
  Effect.Activate(true);
 }
}
class ADWOWindVFXGalleryMode : AGameModeBase
{
 default DefaultPawnClass=ASpectatorPawn;
 UPROPERTY() ACameraActor Camera;
 double Clock=0;
 bool Captured=false;
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  if(!UOpusNative::Flag("DWOTransient")){UOpusNative::Quit(false);return;}
  SpawnActor(ADWOHeroProbeRoom);
  Camera=SpawnActor(ACameraActor,FVector(0,-2300,2300));Camera.SetActorRotation((FVector(0,100,100)-Camera.ActorLocation).Rotation());
  Gameplay::GetPlayerController(0).SetViewTargetWithBlend(Camera,0);
  auto Map=Cast<UDWUEVFXMap>(LoadObject(nullptr,"/Game/WebVFX/DA_UEVFXMap.DA_UEVFXMap"));
  for(int I=0;I<7;I++)
  {
   auto Sample=SpawnActor(ADWOWindVFXSample,FVector((I%4-1.5)*650,(I/4)*850,0));
   Sample.Configure(Map,I==0?n"whirlwind":FName("whirlwind@"+(I-1)));
  }
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Math::Clamp(double(Dt),0.0,.05);
  if(Clock>12&&!Captured){Captured=true;System::ExecuteConsoleCommand("Shot filename="+FPaths::ProjectSavedDir()+"Verification/WindVFX/gallery.png -nosuffix");UOpusNative::WriteLog("WIND_GALLERY_CAPTURED");}
  if(Clock>14&&!UOpusNative::Flag("DWOWindGalleryHold"))UOpusNative::Quit(true);
 }
}
