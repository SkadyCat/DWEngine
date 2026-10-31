#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DWUEVFXRouter.generated.h"

class UDWUEVFXMap;
class UNiagaraSystem;
class UNiagaraComponent;
struct FDWUEVFXState;

// Frame-driven presentation router. Niagara's world pool owns component reuse.
UCLASS()
class DWRULES_API UDWUEVFXRouter : public UObject
{
	GENERATED_BODY()
public:
	UDWUEVFXRouter();
	virtual ~UDWUEVFXRouter() override;
	virtual UWorld* GetWorld() const override;
	void Initialize(UDWUEVFXMap* Map);
	void BeginFrame(float Clock);
	bool Draw(FName Id, uint64 InstanceId, const FTransform& Placement);
	void EndFrame();
	void Clear();
	int32 ActiveSystems() const;
	int32 SpawnCount() const;
	int32 MappingCount() const;
private:
	TSharedPtr<FDWUEVFXState> State;
	UPROPERTY(Transient) TObjectPtr<UDWUEVFXMap> Settings;
	UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraSystem>> LoadedSystems;
};
