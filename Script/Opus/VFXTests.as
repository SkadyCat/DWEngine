// Scoped real-world flight/impact probe. Enabled only with DWOUEVFXTest + DWOTransient.
class ADWOUEVFXTarget : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Body;
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  Body.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cube.Cube")));
  Body.SetRelativeScale3D(FVector(0.6));Body.SetCollisionEnabled(ECollisionEnabled::NoCollision);
  UOpusNative::Tint(Body,FLinearColor(0.25,0.35,0.5));
 }
}
class UDWOUEVFXTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOUEVFXTarget Target;
 double Clock=0,NextCast=0.5;
 int TargetId=0x10012345,CastCount=0;
 bool Captured=false;
 void Initialize(ADWOTowerGameMode T)
 {
  Tower=T;
  // Optional diagnostic warmup allows first-time Niagara/shader compilation
  // before counting casts; normal gameplay and the default probe are unchanged.
  Clock=-Math::Clamp(UOpusNative::IntOption("DWOUEVFXWarmup",0),0,30);
  Target=SpawnActor(ADWOUEVFXTarget,FVector(600,0,60));
  Tower.Rules.UpdateActorTarget(Target,TargetId,1,1000,1000,60);
  UOpusNative::SetEnabled(Tower.Rules,true);
  UOpusNative::WriteLog("UE_VFX_TEST_READY plan="+UOpusNative::ActivePlan(Tower.Rules));
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Tower==nullptr||Target==nullptr)return;
  Clock+=Math::Clamp(double(Dt),0.0,0.05);
  if(Clock>=NextCast&&CastCount<10)
  {
   NextCast+=0.7;
   FVector Dir=(Target.ActorLocation-Tower.Hero.ActorLocation).GetSafeNormal();
   FVector Origin=Tower.Hero.ActorLocation+Dir*60;
   Tower.Rules.UpdateOwner(Tower.Hero.OwnerSlot,Origin,Dir);
   UOpusNative::TargetPoint(Tower.Rules,Tower.Hero.OwnerSlot,Target.ActorLocation);
   if(Tower.Rules.CastBuildPower(UOpusNative::ActivePlan(Tower.Rules),Tower.Hero.OwnerSlot,Origin,Dir,30)>=0)CastCount++;
  }
  if(Clock>7&&!Captured)
  {
   Captured=true;FString Path=UOpusNative::StringOption("DWOUEVFXCapture");
   if(!Path.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Path+" -nosuffix");
  }
  if(Clock>=9)
  {
   auto Pools=Tower.Rules.GetTargetPools(TargetId);
   bool Ok=CastCount>0&&Pools.bActive&&Pools.Life<1000;
   UOpusNative::WriteLog((Ok?"UE_VFX_TEST_PASS":"UE_VFX_TEST_FAIL")+" real projectile hits plan="+UOpusNative::ActivePlan(Tower.Rules)+" casts="+CastCount+" life="+Pools.Life);
   Tower.Rules.ForgetGameTarget(TargetId);Target.DestroyActor();Target=nullptr;
   UOpusNative::Quit(Ok);
  }
 }
}
