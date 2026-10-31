#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonTypes.h"
#include "DungeonLayout.h"
#include "DungeonGenerator.generated.h"

class UBoxComponent;
class UPCGComponent;
class UPCGGraphInterface;

USTRUCT(BlueprintType)
struct FDungeonRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	FVector2D Size = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	EDungeonRoomTheme Theme = EDungeonRoomTheme::Storage;
};

/**
 * Owns the PCG component that runs the dungeon graph. Layout parameters live here so the
 * game code (player spawn, HUD, regeneration) and the PCG node see exactly the same dungeon.
 */
UCLASS(Blueprintable)
class CLAUDEPCG_API ADungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	ADungeonGenerator();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon")
	TObjectPtr<USceneComponent> Root;

	/** Invisible floor slab under the whole grid so characters never fall while instances stream in. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon")
	TObjectPtr<UBoxComponent> Ground;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon")
	TObjectPtr<UPCGComponent> PCG;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon", meta = (ShowOnlyInnerProperties))
	FDungeonGenerationParams Params;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon")
	void Regenerate();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Dungeon")
	void RandomizeSeed();

	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void SetupGraph(UPCGGraphInterface* Graph);

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	FVector GetPlayerStartLocation() const;

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	FVector GetDungeonCenter() const;

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	FVector GetDungeonExtent() const;

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	TArray<FDungeonRoomInfo> GetRooms() const;

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	bool IsGenerationInProgress() const;

	const FDungeonLayout& GetLayout() const;

	static ADungeonGenerator* FindInWorld(const UObject* WorldContext);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostRegisterAllComponents() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void UpdateGround();
	FVector ToWorld(const FVector& Local) const;

	mutable FDungeonLayout CachedLayout;
	mutable uint32 CachedHash = 0;
	mutable bool bCacheValid = false;
};
