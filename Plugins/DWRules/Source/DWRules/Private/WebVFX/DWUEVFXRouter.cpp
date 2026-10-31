#include "DWUEVFXRouter.h"
#include "DWUEVFXMap.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FDWUEVFXKey
{
	FName Id;
	uint64 Instance = 0;
	bool operator==(const FDWUEVFXKey& Other) const { return Id == Other.Id && Instance == Other.Instance; }
	friend uint32 GetTypeHash(const FDWUEVFXKey& Key) { return HashCombineFast(GetTypeHash(Key.Id), GetTypeHash(Key.Instance)); }
};
struct FResolved
{
	FDWUEVFXEntry Entry;
	UNiagaraSystem* System = nullptr;
};
struct FLive
{
	TWeakObjectPtr<UNiagaraComponent> Component;
	uint64 Seen = 0;
	float Started = 0, Limit = 3;
	bool bFollow = false, bFinished = false;
	FVector LastPosition = FVector::ZeroVector;
};
void Release(FLive& Live)
{
	if (UNiagaraComponent* Component = Live.Component.Get())
	{
		Component->DeactivateImmediate();
		Component->ReleaseToPool();
	}
	Live.Component.Reset();
	Live.bFinished = true;
}
}

struct FDWUEVFXState
{
	TMap<FName, FResolved> Resolved;
	TMap<FDWUEVFXKey, FLive> Live;
	uint64 Frame = 0;
	float Clock = 0;
	int32 Active = 0, Spawns = 0;
	bool bVerify = FParse::Param(FCommandLine::Get(), TEXT("DWOVFXVerify"));
	int32 FollowMoves = 0, Returns = 0;
	TMap<FName, int32> SpawnCounts;
};

UDWUEVFXRouter::UDWUEVFXRouter() : State(MakeShared<FDWUEVFXState>()) {}
UDWUEVFXRouter::~UDWUEVFXRouter() = default;
UWorld* UDWUEVFXRouter::GetWorld() const { return GetOuter() ? GetOuter()->GetWorld() : nullptr; }

void UDWUEVFXRouter::Initialize(UDWUEVFXMap* Map)
{
	Clear();
	Settings = Map;
	State->Resolved.Reset();
	LoadedSystems.Reset();
	if (!Map) return;
	// Resolve once, never perform asset I/O in Draw or for a negative lookup.
	for (const auto& Pair : Map->Effects)
	{
		if (Pair.Value.System.IsNull()) continue;
		UNiagaraSystem* System = Pair.Value.System.LoadSynchronous();
		if (!System) continue;
#if WITH_EDITOR
		System->RequestCompile(false);
#endif
		LoadedSystems.AddUnique(System);
		State->Resolved.Add(Pair.Key, {Pair.Value, System});
	}
}

void UDWUEVFXRouter::BeginFrame(float Clock)
{
	if (Clock < State->Clock) Clear();
	State->Clock = Clock;
	++State->Frame;
}

bool UDWUEVFXRouter::Draw(FName Id, uint64 InstanceId, const FTransform& Placement)
{
	const FResolved* Resolved = State->Resolved.Find(Id);
	if (!Resolved || !InstanceId || !Settings) return false;
	const FDWUEVFXKey Key{Id, InstanceId};
	FLive* Live = State->Live.Find(Key);
	if (Live && Live->bFollow && !Live->Component.IsValid())
	{
		if (!Live->bFinished) --State->Active;
		State->Live.Remove(Key);
		Live = nullptr;
	}
	const FTransform Local(Resolved->Entry.Rotation, Resolved->Entry.Offset, Resolved->Entry.Scale);
	const FTransform Transform = Local * Placement;
	if (!Live)
	{
		if (State->Active >= Settings->MaxActiveSystems) return false;
		if (!Resolved->System->IsReadyToRun()) return false;
		UWorld* World = GetWorld();
		if (!World || World->bIsTearingDown) return false;
		UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Resolved->System,
			Transform.GetLocation(), Transform.Rotator(), Transform.GetScale3D(), false, false, ENCPoolMethod::ManualRelease, false);
		if (!Component) return false;
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(false);
		for (const auto& Parameter : Resolved->Entry.UserFloats) Component->SetVariableFloat(Parameter.Key, Parameter.Value);
		for (const auto& Parameter : Resolved->Entry.UserColors) Component->SetVariableLinearColor(Parameter.Key, Parameter.Value);
		for (const auto& Parameter : Resolved->Entry.UserBools) Component->SetVariableBool(Parameter.Key, Parameter.Value);
		Component->Activate(true);
		FLive Entry;
		Entry.Component = Component;
		Entry.Started = State->Clock;
		Entry.bFollow = Resolved->Entry.bFollowSource;
		Entry.LastPosition = Transform.GetLocation();
		Entry.Limit = FMath::Max(.05f, Resolved->Entry.MaxVisualSeconds);
		Live = &State->Live.Add(Key, Entry);
		++State->Active;
		++State->Spawns;
		if (State->bVerify)
		{
			int32& Count = State->SpawnCounts.FindOrAdd(Id);
			++Count;
			if (Count == 1) UE_LOG(LogTemp, Display, TEXT("UE_VFX_SPAWN id=%s active=%d system=%s"), *Id.ToString(), Component->IsActive(), *Resolved->System->GetPathName());
		}
	}
	Live->Seen = State->Frame;
	if (Live->bFollow && !Live->bFinished)
	{
		if (UNiagaraComponent* Component = Live->Component.Get())
		{
			if (State->bVerify && !Live->LastPosition.Equals(Transform.GetLocation(), .01)) ++State->FollowMoves;
			Live->LastPosition = Transform.GetLocation();
			Component->SetWorldTransform(Transform);
			// Some authored flight systems are finite: restart the same pooled
			// component, while the authoritative carrier still exists.
			if (Component->IsComplete()) Component->Activate(true);
		}
	}
	return true;
}

void UDWUEVFXRouter::EndFrame()
{
	for (auto It = State->Live.CreateIterator(); It; ++It)
	{
		FLive& Live = It.Value();
		const bool Seen = Live.Seen == State->Frame;
		UNiagaraComponent* Component = Live.Component.Get();
		if (!Live.bFinished && (!Component || (Live.bFollow && !Seen) ||
			(!Live.bFollow && (Component->IsComplete() || State->Clock - Live.Started >= Live.Limit))))
		{
			Release(Live);
			--State->Active;
			if (State->bVerify) ++State->Returns;
		}
		// A completed burst remains a tombstone while its event is still drawn,
		// preventing one impact from spawning again on the next frame.
		if (Live.bFinished && !Seen) It.RemoveCurrent();
	}
}

void UDWUEVFXRouter::Clear()
{
	for (auto& Pair : State->Live)
	{
		if (State->bVerify && !Pair.Value.bFinished) ++State->Returns;
		Release(Pair.Value);
	}
	State->Live.Reset();
	State->Active = 0;
	if (State->bVerify && State->Spawns > 0)
	{
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetNumberField(TEXT("spawns"), State->Spawns);
		Report->SetNumberField(TEXT("followMoves"), State->FollowMoves);
		Report->SetNumberField(TEXT("returns"), State->Returns);
		Report->SetNumberField(TEXT("activeAfterClear"), State->Active);
		TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
		for (const auto& Pair : State->SpawnCounts) Counts->SetNumberField(Pair.Key.ToString(), Pair.Value);
		Report->SetObjectField(TEXT("systems"), Counts);
		FString Json;
		FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
		FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectSavedDir() / TEXT("Verification/UEVFX/runtime.json")));
		UE_LOG(LogTemp, Display, TEXT("UE_VFX_VERIFIED %s"), *Json);
	}
}
int32 UDWUEVFXRouter::ActiveSystems() const { return State->Active; }
int32 UDWUEVFXRouter::SpawnCount() const { return State->Spawns; }
int32 UDWUEVFXRouter::MappingCount() const { return State->Resolved.Num(); }
