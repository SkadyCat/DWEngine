#pragma once

#include "CoreMinimal.h"
#include "DungeonTypes.generated.h"

UENUM(BlueprintType)
enum class EDungeonRoomTheme : uint8
{
	Start,
	Hall,
	Library,
	Dining,
	Storage,
	Crypt,
	Treasure
};

/** All knobs that drive the dungeon layout. Shared by the PCG node and the generator actor. */
USTRUCT(BlueprintType)
struct CLAUDEPCG_API FDungeonGenerationParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	int32 Seed = 20261008;

	/** Size of the square layout grid, in macro cells (1 macro cell = 400uu = 2x2 floor tiles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "12", ClampMax = "64"))
	int32 GridSize = 26;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "2", ClampMax = "40"))
	int32 MaxRooms = 11;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "3", ClampMax = "10"))
	int32 MinRoomSize = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "3", ClampMax = "12"))
	int32 MaxRoomSize = 6;

	/** Chance to add loops on top of the minimum spanning tree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0", ClampMax = "1"))
	float ExtraConnectionChance = 0.35f;

	/** Wall height in 200uu tiers. 3 matches the kit's native 600uu story (columns and arches fit unscaled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walls", meta = (ClampMin = "1", ClampMax = "3"))
	int32 WallTiers = 3;

	/** Lower the walls that face the top-down camera so the floor stays readable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walls")
	bool bCutawayFrontWalls = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walls", meta = (ClampMin = "0", ClampMax = "3", EditCondition = "bCutawayFrontWalls"))
	int32 FrontWallTiers = 1;

	/** Place a wall torch every N wall edges (rooms). Corridors use N+1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dressing", meta = (ClampMin = "1", ClampMax = "8"))
	int32 TorchSpacing = 2;

	/** Probability that a floor tile spawns a debris candidate (further thinned by the PCG density filter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dressing", meta = (ClampMin = "0", ClampMax = "1"))
	float DebrisChance = 0.45f;
};
