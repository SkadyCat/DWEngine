// Native saved-asset comparison. No player, save, rules, HUD or browser runtime.
class ADWOTreasuryTrailProofMode : AGameModeBase
{
 default DefaultPawnClass=ASpectatorPawn;
 UPROPERTY() ACameraActor Camera;
 UPROPERTY() TArray<ADWWebVFXActor> Effects;
 double Clock=0,Total=0;
 int Stage=0;
 bool Frozen=false;
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  if(!UOpusNative::Flag("DWOTransient")){UOpusNative::Quit(false);return;}
  auto Room=SpawnActor(ADWOHeroProbeRoom);
  Camera=SpawnActor(ACameraActor,FVector(-1000,0,1600));
  Camera.SetActorRotation((FVector(-100,0,140)-Camera.ActorLocation).ToOrientationRotator());
  Gameplay::GetPlayerController(0).SetViewTargetWithBlend(Camera,0);
  for(int I=0;I<3;I++)
  {
   auto Effect=SpawnActor(ADWWebVFXActor,FVector(0,(I-1)*420,180));
   Effect.SetActorRotation(FRotator(0,-35+I*35,0));Effect.bLoop=false;
   Effect.Prefab=Cast<UDWWebVFXPrefab>(LoadObject(nullptr,"/Game/WebVFX/Variants/DA_projectile_treasury_weapon_element_style"+I+".DA_projectile_treasury_weapon_element_style"+I));
   if(Effect.Prefab==nullptr){UOpusNative::WriteLog("TREASURY_TRAIL_FAIL missing style "+I);UOpusNative::Quit(false);return;}
   UOpusNative::WriteLog("TREASURY_TRAIL_ASSET style="+I+" hash="+Effect.Prefab.SourceHash);
   Effects.Add(Effect);
  }
 }
 void Capture(FString View)
 {
  FString Name=UOpusNative::StringOption("DWOTrailCapture");if(Name.IsEmpty())Name="after";
  System::ExecuteConsoleCommand("Shot filename="+FPaths::ProjectSavedDir()+"Verification/TreasuryTrail/"+Name+"-"+View+".png -nosuffix");
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Total+=Dt;Clock+=Dt;
  if(!Frozen&&Total>.16){for(auto Effect:Effects)Effect.PlayRate=0;Frozen=true;}
  if(Clock<3)return;
  if(Stage==0){Capture("top");Stage=1;Clock=0;return;}
  if(Stage==1){Camera.SetActorLocation(FVector(-1700,0,650));Camera.SetActorRotation((FVector(-100,0,170)-Camera.ActorLocation).ToOrientationRotator());Stage=2;Clock=0;return;}
  if(Stage==2){Capture("side");Stage=3;Clock=0;return;}
  bool Rendered=Effects.Num()==3;
  for(auto Effect:Effects){TArray<UInstancedStaticMeshComponent> Parts;Effect.GetComponentsByClass(Parts);bool Found=false;for(auto Part:Parts)Found=Found||(Part.IsVisible()&&Part.GetInstanceCount()>0);Rendered=Rendered&&Found;}
  UOpusNative::WriteLog(Rendered?"TREASURY_TRAIL_PASS ThreeSavedStylesRendered":"TREASURY_TRAIL_FAIL no native instances");
  UOpusNative::Quit(Rendered);
 }
}
