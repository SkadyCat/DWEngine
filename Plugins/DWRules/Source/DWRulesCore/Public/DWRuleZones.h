#pragma once
#include "DWRuleCombat.h"

namespace DWRules
{
struct FDWZoneTemplate
{
	int32 Root = INDEX_NONE;
	FName Effect, Shape = TEXT("circle");
	FVector3d Position = FVector3d::ZeroVector;
	uint64 Tags = 0;
	uint16 EventType = uint16(EBuiltinEvent::Hit);
	FDWEvent Base;
	uint8 Element = 0, Generation = 1;
	double Amount = 0, Radius = 1, Inner = 0, Outer = 1, Angle = 0, SectorRadius = 1, Length = 7, Width = 1.2;
	double Direction = 0, Delay = 0, Life = 0;
    double RoamSpeed=0,RoamRange=0,RoamInterval=.6;
	bool bFollowSelf = false, bPrimary = false, bPersistent = false, bOwnPlan = false;
	bool bSecondary = false, bRecast = false, bGround = false, bRepeat = true, bSpread = true;
	uint32 FollowTarget = InvalidId;
};
struct FDWZone : FDWZoneTemplate
{
	uint32 Id = 0;
	double Time = 0, Tick = 0, SpreadRadius = 0, NextAt = 0, CurrentRadius = 0, NextSpread = 0, Interval = .5,
		   MaxRadius = 1;
	int32 RepeatIndex = 0, RepeatsLeft = 0, SpreadsLeft = 0, KillSpreads = 0;
	bool bFired = false, bEnded = false, bExpand = true;
    FVector3d RoamOrigin=FVector3d::ZeroVector;
    double RoamTurnLeft=.6;
    uint32 RoamSeed=1;
    bool bRoaming=false;
	TMap<uint32, int32> HitCounts;
	TMap<uint32, double> LastHit, Stay;
	TSharedRef<TSet<uint32>> HitIds = MakeShared<TSet<uint32>>();
};
class DWRULESCORE_API FDWZones
{
  public:
	explicit FDWZones(FDWCombatRuntime &Runtime) : R(Runtime) {}
	FDWCombatRuntime &R;
	FDWZones(const FDWZones &) = delete;
	FDWZones &operator=(const FDWZones &) = delete;
	TArray<TUniquePtr<FDWZone>> Zones;
	bool OnArea(const FDWAreaRequest &Request);
	void Instant(const FDWAreaRequest &Request, double Radius);
	void Cast(const FDWAreaRequest &Request, double Radius, bool Triggered, bool OwnPlan);
	FDWZone *Spawn(const FDWZoneTemplate &Template);
	TArray<TPair<uint32, double>> Victims(const FDWZone &Zone, double Radius = -1) const;
	int32 Resolve(FDWZone &Zone, double Scale = 1, double Radius = -1, TOptional<FVector2d> Ring = {}, double Tick = 1);
	void Pull(const FDWZone &Zone, const TArray<uint32> &Targets, double Seconds);
	void OnHit(const FDWCombatHit &Hit);
	void OnKill(const FDWCombatHit &Hit);
	void Tick(double Dt);
	void TickPersistent(FDWZone &Zone, double Dt);
	void End(FDWZone &Zone);

  private:
	struct FRecent
	{
		uint32 Zone;
		double Time;
	};
	TMap<uint64, FRecent> RecentHits;
	uint32 NextId = 1;
	double S(FName Name) const
	{
		return R.S(Name);
	}
};
} // namespace DWRules
