#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DWWebVFXComponent.generated.h"

class UInstancedStaticMeshComponent;
class UDWWebVFXPrefab;
class UDWUEVFXRouter;
class UDWUEVFXMap;
struct FDWWebVFXState;

// Plain presentation inputs. No target search, collisions, skills or damage.
struct FDWWebVFXParameters
{
	float Inner = -1, Angle = -1, Clock = -1, Fade = -1, BillboardScale = -1;
	bool bPersistent = false;
	FName MappingId;
	uint64 InstanceId = 0;
};

UCLASS(ClassGroup=(Rendering), meta=(BlueprintSpawnableComponent))
class DWRULES_API UDWWebVFXComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UDWWebVFXComponent();
	virtual ~UDWWebVFXComponent() override;
	bool LoadPrefabs();
	void BeginFrame(float Clock);
	UFUNCTION(BlueprintCallable) bool DrawPrefab(UDWWebVFXPrefab* Prefab, float Age, const FTransform& Placement);
	bool Draw(FName Prefab, float Age, const FTransform& Placement, const FDWWebVFXParameters& Parameters = {});
	void EndFrame();
	bool HasPrefab(FName Id) const;
	TArray<FName> Catalog() const;
	int32 VisibleInstances() const;
	TArray<FName> DrawnPrefabs() const;
	void SetUEVFXMap(UDWUEVFXMap* Map);
	void ClearUEVFX();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	TSharedPtr<FDWWebVFXState> State;
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> MeshBatches;
	UPROPERTY(Transient) TObjectPtr<UDWUEVFXRouter> UERouter;
};
