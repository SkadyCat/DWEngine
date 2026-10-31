// A separate Entry world hosts storage; the Zero dungeon remains the tower scene.
class ADWOHideoutRoom : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UBoxComponent Floor;
 UPROPERTY(DefaultComponent,Attach=Root) UBoxComponent Chest;
 UPROPERTY(DefaultComponent,Attach=Root) UBoxComponent Counter;
 UPROPERTY(DefaultComponent,Attach=Root) UPointLightComponent Light;
 UPROPERTY() UDWWebVFXPrefab OriginalScene;
 UPROPERTY() ADWWebVFXActor Decoration;
 default Floor.SetCollisionProfileName(n"BlockAll");
 default Chest.SetCollisionProfileName(n"BlockAll");
 default Counter.SetCollisionProfileName(n"BlockAll");
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  Floor.SetBoxExtent(FVector(800,600,10));Floor.SetRelativeLocation(FVector(0,0,-10));Floor.SetHiddenInGame(true);
  Chest.SetBoxExtent(FVector(150,100,100));Chest.SetRelativeLocation(FVector(-420,120,100));Chest.SetHiddenInGame(true);
  Counter.SetBoxExtent(FVector(170,197.5,110));Counter.SetRelativeLocation(FVector(400,172.5,110));Counter.SetHiddenInGame(true);
  OriginalScene=Cast<UDWWebVFXPrefab>(LoadObject(nullptr,"/Game/WebVFX/Variants/DA_hideout_room.DA_hideout_room"));
  Decoration=Cast<ADWWebVFXActor>(SpawnActor(ADWWebVFXActor));Decoration.Prefab=OriginalScene;Decoration.PlayRate=0;Decoration.bLoop=false;
  Light.SetRelativeLocation(FVector(0,0,800));Light.SetIntensity(220000);Light.SetAttenuationRadius(3000);Light.SetLightColor(FLinearColor(.92,.85,.7));
 }
}
