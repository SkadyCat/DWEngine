#include "OpusDungeon.h"
#include "DungeonGenerator.h"
#include "DungeonLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

ADungeonGenerator* UOpusDungeon::Generator(UObject* Context) { return ADungeonGenerator::FindInWorld(Context); }
TArray<FVector4> UOpusDungeon::MapCells(UObject* Context)
{
 TArray<FVector4> Out;auto* G=Generator(Context);if(!G)return Out;const auto& L=G->GetLayout();
 for(int Y=0;Y<L.Height;Y++)for(int X=0;X<L.Width;X++)if(L.IsFloor({X,Y}))
 {const auto A=G->GetActorTransform().TransformPosition(FVector(X*L.CellSize,Y*L.CellSize,0));Out.Add(FVector4(A.X/100,A.Y/100,L.CellSize/100,L.CellSize/100));}
 return Out;
}
TArray<FVector4> UOpusDungeon::MapRooms(UObject* Context)
{
 TArray<FVector4> Out;auto* G=Generator(Context);if(!G)return Out;const auto& L=G->GetLayout();
 for(const auto& R:L.Rooms){const auto A=G->GetActorTransform().TransformPosition(FVector(R.Min.X*L.CellSize,R.Min.Y*L.CellSize,0));Out.Add(FVector4(A.X/100,A.Y/100,R.Size.X*L.CellSize/100,R.Size.Y*L.CellSize/100));}return Out;
}
bool UOpusDungeon::Surface(UObject* Context,FVector Position,FVector& Point,FVector& Normal)
{
 UWorld* World=Context?Context->GetWorld():nullptr;if(!World)return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(OpusGroundSurface),false);
 FHitResult Hit;
 if(!World->LineTraceSingleByObjectType(Hit,Position+FVector(0,0,20),Position-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic),Query)||Hit.ImpactNormal.Z<.5)return false;
 Point=Hit.ImpactPoint;Normal=Hit.ImpactNormal;return true;
}
bool UOpusDungeon::Project(UObject* Context,FVector Position,FVector& Result)
{
 UWorld* World=Context?Context->GetWorld():nullptr;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
 FNavLocation Point;
 if(!Nav||!Nav->ProjectPointToNavigation(Position,Point,FVector(130,130,300)))return false;
 Result=Point.Location;return true;
}
bool UOpusDungeon::Ready(UObject* Context)
{
 auto* Gen=Generator(Context);if(!Gen||Gen->IsGenerationInProgress())return false;
 UWorld* World=Gen->GetWorld();
 if(UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))return false;
 FVector Point;return Project(Context,Gen->GetPlayerStartLocation(),Point);
}
TArray<FVector> UOpusDungeon::Path(UObject* Context,FVector Start,FVector Goal)
{
 FVector A,B;if(!Project(Context,Start,A)||!Project(Context,Goal,B))return {};
 auto* Route=UNavigationSystemV1::FindPathToLocationSynchronously(Context,A,B);
 if(!Route||!Route->IsValid()||Route->IsPartial())return {};
 return Route->PathPoints;
}
TArray<FVector> UOpusDungeon::WalkablePoints(UObject* Context,float Clearance)
{
 TArray<FVector> Out;auto* Gen=Generator(Context);if(!Gen)return Out;
 const auto& Layout=Gen->GetLayout();UWorld* World=Gen->GetWorld();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(OpusDungeonSpawn),false);
 const auto Shape=FCollisionShape::MakeCapsule(Clearance,FMath::Max(100.f,Clearance));
 for(int32 Y=0;Y<Layout.Height;++Y)for(int32 X=0;X<Layout.Width;++X)
 {
  FIntPoint Cell(X,Y);if(!Layout.IsFloor(Cell))continue;
  FVector P;if(!Project(Context,Gen->GetActorTransform().TransformPosition(Layout.CellCenter(Cell)),P))continue;
  if(World->OverlapAnyTestByObjectType(P+FVector(0,0,110),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Query))continue;
  if(Path(Context,Gen->GetPlayerStartLocation(),P).IsEmpty())continue;
  Out.Add(P+FVector(0,0,60));
 }
 return Out;
}
FVector UOpusDungeon::Move(UObject* Context,FVector Start,FVector Delta,float Radius)
{
 auto* World=Context?Context->GetWorld():nullptr;if(!World)return Start;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(OpusDungeonMove),false);
 const FVector Offset(0,0,50);
 FCollisionShape Shape=FCollisionShape::MakeCapsule(FMath::Max(1.f,Radius),FMath::Max(100.f,Radius));
 FHitResult Hit;
 if(!World->SweepSingleByObjectType(Hit,Start+Offset,Start+Delta+Offset,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Query))return Start+Delta;
 FVector Result=Start+Delta*FMath::Max(0.f,Hit.Time-.002f);
 FVector Slide=FVector::VectorPlaneProject(Delta*(1-Hit.Time),Hit.Normal);Slide.Z=0;
 if(!Slide.IsNearlyZero())
 {
  FHitResult Other;
  if(World->SweepSingleByObjectType(Other,Result+Offset,Result+Slide+Offset,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Query))Slide*=FMath::Max(0.f,Other.Time-.002f);
  Result+=Slide;
 }
 return Result;
}
float UOpusDungeon::Sweep(UObject* Context,FVector Start,FVector End,float Radius)
{
 auto* World=Context?Context->GetWorld():nullptr;if(!World)return -1;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(OpusDungeonProjectile),false);
 FHitResult Hit;
 if(World->SweepSingleByObjectType(Hit,Start,End,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeSphere(FMath::Max(1.f,Radius)),Query))return Hit.Time;
 return -1;
}
int32 UOpusDungeon::InstanceCount(UObject* Context)
{
 int32 Count=0;auto* World=Context?Context->GetWorld():nullptr;if(!World)return 0;
 for(TActorIterator<AActor> It(World);It;++It)
 {
  TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
  for(auto* C:Components)Count+=C->GetInstanceCount();
 }
 return Count;
}
