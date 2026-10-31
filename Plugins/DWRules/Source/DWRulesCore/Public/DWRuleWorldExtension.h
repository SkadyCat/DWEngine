#pragma once
#include "DWRuleTypes.h"

namespace DWRules
{
struct FDWRoot;
struct FDWSummon;
struct FDWEvent;
struct FDWDamageRecord;

// Semantic delivery data. Presentation consumes this data without entering the VM.
struct FDWDamageInfo
{
	int32 RootIndex = INDEX_NONE, SourceIndex = INDEX_NONE;
	uint32 SourceId = InvalidId, ZoneId = 0;
	uint64 Tags = 0;
	FVector3f Position = FVector3f::ZeroVector, Center = FVector3f::ZeroVector;
	float Radius = 0;
	FName Kind, Strike;
	int32 Depth = 0;
	bool bHasCenter = false, bChain = false;
	double ZoneMore = 1;
	double Converted = 0;
	bool bExecuted = false;
	// Observed before target resistance; reflection never re-applies attacker offense.
	double OriginalAmount = 0;
	bool bReflected = false;
};
struct FDWDamageResult
{
	float Applied = 0;
	bool bCritical = false, bKilled = false, bMissed = false;
};
struct FDWDamageMod
{
	bool bMiss = false;
	double Flat = 0, Increased = 0, More = 1, Critical = 0, CriticalScale = 1, CriticalMultiplier = 0;
	double Penetration = 0, Taken = 1;
	TFunction<double(double, bool)> Adjust;
};
struct FDWAreaRequest
{
	int32 RootIndex = INDEX_NONE;
	FName Trait;
	const FDWEvent *Event = nullptr;
	const FDWEvent *Base = nullptr;
	uint16 EventType = 0;
	float Radius = 0, Amount = 0;
	uint8 Element = 0;
	uint64 Tags = 0;
};
struct FDWSpawnInfo
{
	int32 Ordinal = 0, Count = 1;
	bool bEmit = true;
	FVector3f Direction = FVector3f(1, 0, 0), Origin = FVector3f::ZeroVector;
};

// Optional native host policy. The bare deterministic world remains usable without it.
// All callbacks execute on the owner thread; motion policy runs before parallel sweeps.
class DWRULESCORE_API IDWRuleWorldExtension
{
  public:
	virtual ~IDWRuleWorldExtension() = default;
	virtual FDWDamageMod ModifyDamage(const FDWRoot &, int32, float, uint8, FDWDamageInfo &)
	{
		return {};
	}
	virtual bool ModifyStatus(uint32, uint64, float &, float &, int32)
	{
		return true;
	}
    virtual void StatusConsumed(uint32, uint64, int32) {}
	virtual bool Area(const FDWAreaRequest &)
	{
		return false;
	}
	virtual void Spawn(int32, int32, const FDWSpawnInfo &) {}
	virtual void Motion(int32, float) {}
	virtual bool Contact(int32, int32, const FDWDamageResult &)
	{
		return false;
	}
	virtual bool Wall(int32)
	{
		return false;
	}
	virtual bool End(int32, EEndReason)
	{
		return false;
	}
	virtual int32 Ricochet(int32, const FDWEvent &, float)
	{
		return INDEX_NONE;
	}
	virtual void Redirect(int32, ERedirect) {}
	virtual int32 SummonCap(int32 Cap, uint16, int32)
	{
		return Cap;
	}
	virtual void Summon(FDWSummon &, int32, const FDWSummon *) {}
	virtual int32 SummonTarget(FDWSummon &, int32 Nearest)
	{
		return Nearest;
	}
	virtual void SummonEnd(const FDWSummon &) {}
    virtual bool Sacrifice(int32 Root,int32 Count,FName Kind){return false;}
    virtual void DerivedRoot(int32 Root, int32 Source, bool Repeat) {}
    virtual bool Intercept(int32 Index,int32 Target,const FVector3f& At){return false;}
    virtual void AfterMotion() {}
    virtual float WallRadius(int32 Index,float Radius){return Radius;}
    virtual void ProjectileHit(int32 Index,const FVector3f& At) {}
    virtual bool SpawnMonster(FName Kind, int32 Count, float Duration, float Radius, bool Invulnerable, int32 Owner) { return false; }
};
} // namespace DWRules
