#pragma once
#include "DWRuleCombat.h"

namespace DWRules
{
struct FDWAilmentLayer
{
	double Dps = 0, Left = 0, Total = 0;
	int32 Keeps = 0, Generation = 0;
};
struct FDWLayeredAilment
{
	TArray<FDWAilmentLayer> Layers;
	double Tick = 0, Cooldown = 0;
};
struct FDWAilmentField
{
	FName Type;
	FVector3d Position = FVector3d::ZeroVector;
	double Radius = 0, Left = 0, Interval = .5, Tick = 0, Dps = 0;
	TSet<uint32> Inside;
};
struct FDWAilmentApplication
{
	double Amount = 0, Effect = 0, Duration = 2;
	int32 Layers = INDEX_NONE, Generation = 0;
	uint8 Element = uint8(EElement::Fire);
	bool bCritical = false, bSpread = false, bMinion = false;
};
class DWRULESCORE_API FDWAilments
{
  public:
	explicit FDWAilments(FDWCombatRuntime &Runtime) : R(Runtime) {}
	FDWCombatRuntime &R;
	TMap<uint32, FDWLayeredAilment> Poison, Erosion;
	TMap<uint32, TArray<FDWAilmentLayer>> Shock, Ignite;
	TMap<uint32, double> BurnTime, FreezeBuild, LastToVoid;
	TMap<uint32, bool> Frozen;
	TArray<FDWAilmentField> Fields;
	double PendingEffect = 0;
	int32 ShockApplied = 0, PoisonApplied = 0;
	bool bShatterFreeze = false;
	int32 Layers(uint32 Id, FName Name) const;
	int32 ShockLayers(uint32 Id) const;
	bool IsBurning(uint32 Id) const;
	double SlowOf(uint32 Id) const;
	double TakenMultiplier(uint32 Id, uint8 Element);
	bool OnStatus(uint32 Id, uint64 Key, float &Value, float &Duration, int32 Owner);
    void OnStatusConsumed(uint32 Id, uint64 Key, int32 Count);
	void AddShock(uint32 Id, double Value, double Duration);
	void SyncShock(uint32 Id);
	void ConsumeShock(uint32 Id);
	void AddIgnite(uint32 Id, double Dps, double Duration);
	void SyncIgnite(uint32 Id);
	double IgniteRemaining(uint32 Id) const;
	TMap<uint32, FDWLayeredAilment> &Store(FName Name);
	const TMap<uint32, FDWLayeredAilment> &Store(FName Name) const;
	int32 RemoveLayers(uint32 Id, FName Name, int32 Count = MAX_int32);
	double Remaining(uint32 Id, FName Name) const;
	void SyncLayers(uint32 Id, FName Name);
	void ApplyPoison(uint32 Id, const FDWAilmentApplication &Options);
	void ApplyErosion(uint32 Id, const FDWAilmentApplication &Options);
	void Collapse(uint32 Id, bool FromExecute);
	void Detonate(uint32 Id, int32 Layers = MAX_int32, bool Force = false);
	void SpreadLayers(uint32 Id, FName Name, int32 Count, double Radius, int32 Targets = 2, int32 Generation = 0,
					  const FVector3d *Origin = nullptr, double Average = -1);
	void PoisonSpread(uint32 Id, int32 Count, int32 Generation);
	void OnHit(const FDWCombatHit &Hit);
	void Chain(const FDWCombatHit &Hit);
	void BuildFreeze(const FDWCombatHit &Hit);
	void RemoveFreeze(uint32 Id, bool Consumed);
	void OnKill(const FDWCombatHit &Hit);
	void Forget(uint32 Id);
	void Tick(double Dt);

  private:
	struct FScheduledSpread
	{
		double At, Average;
		FVector3d Origin;
		uint32 Id;
		int32 Count, Generation;
	};
	TArray<FScheduledSpread> Scheduled;
	bool bDetonating = false;
	double S(FName Name) const
	{
		return R.S(Name);
	}
};
} // namespace DWRules
