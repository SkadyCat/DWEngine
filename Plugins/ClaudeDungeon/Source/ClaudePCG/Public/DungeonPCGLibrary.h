#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DungeonPCGLibrary.generated.h"

class UPCGNode;
class UPCGSettings;
class UStaticMesh;
class ANavMeshBoundsVolume;

/** Scripting helpers used by Scripts/build_dungeon.py to assemble the PCG graph and level. */
UCLASS()
class CLAUDEPCG_API UDungeonPCGLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Editor-only brush construction; navigation/AI decisions remain in Lua/Mass. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Assets")
	static bool ConfigureNavigationBounds(ANavMeshBoundsVolume* Volume, FVector Extent);
	/** Switches a Static Mesh Spawner to "By Attribute" mesh selection and sets instance collision/shadow flags. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|PCG")
	static bool ConfigureMeshSpawner(UPCGSettings* Settings, FName MeshAttribute, bool bCollision, bool bCastShadow, float CullDistance = 0.f);

	/** Sets the template actor class of a Spawn Actor node (no merging, so lights/particles survive). */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|PCG")
	static bool ConfigureActorSpawner(UPCGSettings* Settings, TSubclassOf<AActor> ActorClass);

	UFUNCTION(BlueprintCallable, Category = "Dungeon|PCG")
	static void SetNodePosition(UPCGNode* Node, int32 X, int32 Y);

	UFUNCTION(BlueprintCallable, Category = "Dungeon|PCG")
	static void SetNodeTitle(UPCGNode* Node, FName Title);

	/** Makes a mesh use its render geometry as collision (kit meshes ship without simple collision). */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Assets")
	static bool UseComplexAsSimpleCollision(UStaticMesh* Mesh);
};
