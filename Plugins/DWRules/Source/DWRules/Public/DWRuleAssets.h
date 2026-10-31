#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DWRuleAssets.generated.h"

class UStaticMesh;
class UMaterialInterface;

// One trait source: active skill, support, rune, equipment affix or talent. Program text is the
// SkillStudio affix-framework program as JSON ({"modifiers":[...],"rules":[...]}).
UCLASS(BlueprintType)
class DWRULES_API UDWTraitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FName TraitId;

	// The trait is active only while its parent is part of the active build.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FName ParentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	TMap<FName, float> Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait", meta = (MultiLine = true))
	FString ProgramJson;
};

USTRUCT(BlueprintType)
struct DWRULES_API FDWRuleVisual
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UMaterialInterface> Material;

	// Mesh scale per meter of carrier radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector ScalePerMeter = FVector(4.0, 1.0, 1.0);
};

// World budgets and carrier visuals. All limits live here, never in code paths.
UCLASS(BlueprintType)
class DWRULES_API UDWRuleWorldConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "64"))
	int32 MaxProjectiles = 4096;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "100"))
	int32 WorkPerFrame = 20000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "16"))
	int32 RootWorkCap = 16384;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "1"))
	int32 MaxWaves = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0"))
	int32 MaxDeferredEvents = 65536;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "15", ClampMax = "240"))
	float SimulationRate = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxSubsteps = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.5"))
	float CellSizeMeters = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TMap<FName, FDWRuleVisual> Visuals;
};
