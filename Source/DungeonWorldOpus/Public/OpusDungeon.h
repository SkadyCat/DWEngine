#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpusDungeon.generated.h"

class ADungeonGenerator;

/** Engine geometry/navigation access only; encounter and movement policy remain in AS. */
UCLASS()
class DUNGEONWORLDOPUS_API UOpusDungeon : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static ADungeonGenerator* Generator(UObject* Context);
 UFUNCTION(BlueprintPure) static bool Ready(UObject* Context);
 UFUNCTION(BlueprintCallable) static bool Project(UObject* Context,FVector Position,FVector& Result);
 UFUNCTION(BlueprintCallable) static bool Surface(UObject* Context,FVector Position,FVector& Point,FVector& Normal);
 UFUNCTION(BlueprintCallable) static TArray<FVector> WalkablePoints(UObject* Context,float Clearance=95);
 UFUNCTION(BlueprintCallable) static TArray<FVector> Path(UObject* Context,FVector Start,FVector Goal);
 UFUNCTION(BlueprintCallable) static FVector Move(UObject* Context,FVector Start,FVector Delta,float Radius);
 UFUNCTION(BlueprintPure) static float Sweep(UObject* Context,FVector Start,FVector End,float Radius);
 UFUNCTION(BlueprintPure) static int32 InstanceCount(UObject* Context);
 UFUNCTION(BlueprintPure) static TArray<FVector4> MapCells(UObject* Context);
 UFUNCTION(BlueprintPure) static TArray<FVector4> MapRooms(UObject* Context);
};
