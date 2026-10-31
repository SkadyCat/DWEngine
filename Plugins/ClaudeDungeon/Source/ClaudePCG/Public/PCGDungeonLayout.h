#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "DungeonTypes.h"
#include "PCGDungeonLayout.generated.h"

class UStaticMesh;

namespace DungeonPCGPins
{
	const FName Floors = TEXT("Floors");
	const FName Walls = TEXT("Walls");
	const FName Trim = TEXT("Trim");
	const FName Props = TEXT("Props");
	const FName Clutter = TEXT("Clutter");
	const FName Debris = TEXT("Debris");
	const FName Torches = TEXT("Torches");
	const FName Candelabras = TEXT("Candelabras");
}

/**
 * PCG node that generates a complete top-down dungeon (rooms, corridors, walls, arches, columns, themed props,
 * debris and light anchors) from a seed. Every output point carries a "Mesh" soft object path attribute
 * (except light anchors) so a Static Mesh Spawner in "By Attribute" mode can instance it.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class CLAUDEPCG_API UPCGDungeonLayoutSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGDungeonLayoutSettings();

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("DungeonLayout")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.85f, 0.35f, 0.1f); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** When the graph runs on an ADungeonGenerator, read the layout parameters from the actor instead of this node. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bUseOwnerParams = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	FDungeonGenerationParams Params;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Meshes)
	TSoftObjectPtr<UStaticMesh> FloorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Meshes)
	TSoftObjectPtr<UStaticMesh> WallMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Meshes)
	TSoftObjectPtr<UStaticMesh> ArchMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Meshes)
	TSoftObjectPtr<UStaticMesh> ColumnMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Meshes)
	TSoftObjectPtr<UStaticMesh> TrimMesh;
};

class FPCGDungeonLayoutElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
