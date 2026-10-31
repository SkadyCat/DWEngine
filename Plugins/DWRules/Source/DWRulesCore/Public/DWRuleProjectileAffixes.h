#pragma once
#include "DWRuleCombat.h"

namespace DWRules
{
struct FDWAffixProjectile
{
	int32 Generation = 0, Pierced = 0, Chained = 0, Bounced = 0, Hits = 0, Retargets = 0;
	int32 ChainsLeft = 0, BouncesLeft = 0, SplitsLeft = 0, Side = 1, Ordinal = 0, Count = 1;
	double Distance = 0, Speed0 = 0, Radius0 = 0, Age = 0, Hold = 0, LastSecondary = 0, RehitAt = 0;
	double HomingScale = 1;
	FVector3d Origin = FVector3d::ZeroVector;
	TOptional<FVector3f> SavedVelocity;
	TOptional<double> Phi;
	uint32 Target = InvalidId;
    TSharedPtr<TMap<uint32,int32>> Batch;
	int32 PiercedSinceSecondary = 0;
	bool bReturning = false, bSplitDone = false, bMinion = false, bNoHoming = false, bRecalled = false;
};
struct FDWProjectileFire
{
	FVector3d Position = FVector3d::ZeroVector;
	int32 Count = 1, Generation = 2;
	bool bRadial = false;
	double Direction = 0, Damage = 0, Speed = 18, Life = .5, Radius = .2;
	uint8 Element = 1;
	TArray<FName> Tags;
	FName Kind = TEXT("ice_lance");
	uint32 Exclude = InvalidId;
	const FDWAffixProjectile *From = nullptr;
	int32 FromIndex = INDEX_NONE;
};
struct FDWProjectileFieldView
{
	FVector3d Position;
	double Radius=0, Remaining=0;
	uint8 Element=0;
};
class DWRULESCORE_API FDWProjectileAffixes
{
  public:
	explicit FDWProjectileAffixes(FDWCombatRuntime &Runtime) : R(Runtime) {}
	FDWCombatRuntime &R;
	static constexpr double FieldRadius=1.2;
	TArray<FDWProjectileFieldView> FieldViews() const
	{
		TArray<FDWProjectileFieldView> Out;
		for(const auto& Field:Fields)if(Field.Left>0)Out.Add({Field.Position,FieldRadius,Field.Left,Field.Element});
		return Out;
	}
	FDWProjectileAffixes(const FDWProjectileAffixes &) = delete;
	FDWProjectileAffixes &operator=(const FDWProjectileAffixes &) = delete;
	TMap<uint32, TUniquePtr<FDWAffixProjectile>> States;
	int32 Recalled = 0;
	bool Homing() const;
	FDWAffixProjectile &NewState(int32 Index);
	void OnSpawn(int32 Index, int32 Root, const FDWSpawnInfo &Data);
	int32 Clone(int32 Index, int32 Root, double Angle = 0, FVector3d Offset = FVector3d::ZeroVector,
				double DamageScale = 1, double LifeScale = 1, int32 Generation = INDEX_NONE);
	void Fire(const FDWProjectileFire &Request);
	void Secondary(int32 Index, FDWAffixProjectile &State, int32 Count, TOptional<double> Direction = {});
	uint32 PickTarget(const FVector3d &Position, double Range, const TSet<uint32> &Exclude = {});
	void OnMotion(int32 Index, double Dt);
	void Bounced(int32 Index, FDWAffixProjectile &State);
	bool OnWall(int32 Index);
	void Split(int32 Index, FDWAffixProjectile &State, bool Force = false);
	bool OnContact(int32 Index, int32 Target, const FDWDamageResult &Result);
	void Pierced(int32 Index, FDWAffixProjectile &State, const FVector3d &Position, uint32 Target);
	void StartReturn(int32 Index, FDWAffixProjectile &State);
	void Recall(int32 Index, FDWAffixProjectile &State);
	bool OnEnd(int32 Index, EEndReason Reason);
	int32 OnRicochet(int32 Index, const FDWEvent &Event, double Radius);
	void OnRedirect(int32 Index, ERedirect Mode);
	void DamageMod(FDWDamageMod &Mod, const FDWDamageInfo &Info, uint32 Target, FDWCombatRoot *Root);
	void OnCast(int32 Root, int32 Plan);
	void Tick(double Dt);

  private:
	struct FShot
    {
        FVector3f Position,Velocity;
        float Life=0,Radius=0,Damage=0,Gravity=0;
        uint8 Element=0,Generation=0,Flags=0;
        uint16 Kind=0,Refract=0,EndEvent=0,Pierce=0,Ricochet=0,Return=0,Reflect=0;
        uint64 Tags=0;
        uint32 Lineage=0;
        int32 Side=1,Gen=0;
        double Hold=0;
    };
    void DamageSource(int32 Index,FDWStrikeOptions& Out);
    FShot Snapshot(int32 Index);
    struct FVolley;
    void RepeatVolley(const FVolley& Job);
    struct FField
	{
		FVector3d Position;
		double Left = 2, Tick = 0, Damage = 0;
		uint8 Element = 0;
        int32 Plan=INDEX_NONE;uint32 MinionId=0;bool bTriggered=false;
	};
	struct FVolley
	{
		double At = 0;
		int32 Root = INDEX_NONE, Plan = INDEX_NONE, Left = 0;
		TOptional<FVector3f> TargetPoint;
        uint32 MinionId=0;bool bTriggered=false;TArray<FShot> Shots;TSharedPtr<TMap<uint32,int32>> Batch;
	};
	TArray<FField> Fields;
	TArray<FVolley> Volleys;
	FDWProjectiles &C()
	{
		return R.World.Carriers;
	}
	double S(FName Name) const
	{
		return R.S(Name);
	}
};
} // namespace DWRules
