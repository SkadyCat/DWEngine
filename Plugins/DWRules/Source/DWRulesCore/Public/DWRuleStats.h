#pragma once

#include "DWRuleTypes.h"

// Scalar stat table: Id, path, default base, hard min, hard max.
#define DW_RULE_SCALAR_STATS(X) \
	X(OwnerMaxLife,           TEXT("owner.max_life"),               100.f,  1.f,      1.0e9f) \
	X(OwnerMaxMana,           TEXT("owner.max_mana"),               100.f,  0.f,      1.0e9f) \
	X(OwnerCriticalChance,    TEXT("owner.critical_chance"),        5.f,    0.f,      100.f) \
	X(OwnerCriticalMultiplier,TEXT("owner.critical_multiplier"),    150.f,  100.f,    1.0e9f) \
	X(OwnerHitChanceBonus,    TEXT("owner.hit_chance_bonus"),       0.f,    -1.0e9f,  1.0e9f) \
	X(OwnerManaCost,          TEXT("owner.mana_cost"),              0.f,    0.f,      1.0e9f) \
	X(CastSpeedPercent,       TEXT("cast.speed_percent"),           0.f,    -90.f,    1.0e9f) \
	X(CastExtraCostPercent,   TEXT("cast.extra_cost_percent"),      0.f,    -100.f,   1.0e9f) \
	X(ProjectileCount,        TEXT("projectile.count"),             1.f,    0.f,      512.f) \
	X(ProjectileSpread,       TEXT("projectile.spread"),            30.f,   0.f,      360.f) \
	X(ProjectileSpeed,        TEXT("projectile.speed"),             15.f,   0.f,      1000.f) \
	X(ProjectileLifetime,     TEXT("projectile.lifetime"),          1.2f,   0.01f,    60.f) \
	X(ProjectileDamage,       TEXT("projectile.damage"),            10.f,   0.f,      1.0e9f) \
	X(ProjectileRadius,       TEXT("projectile.radius"),            0.2f,   0.01f,    50.f) \
	X(ProjectileCapacity,     TEXT("projectile.capacity"),          1200.f, 1.f,      65535.f) \
	X(ProjectilePierce,       TEXT("projectile.pierce"),            0.f,    0.f,      1000.f) \
	X(ProjectileRicochetCount,TEXT("projectile.ricochet_count"),    0.f,    0.f,      1000.f) \
	X(ProjectileRicochetRadius,TEXT("projectile.ricochet_radius"),  8.f,    0.f,      1000.f) \
	X(ProjectileReflectionCount,TEXT("projectile.reflection_count"),0.f,    0.f,      1000.f) \
	X(ProjectileReturnCount,  TEXT("projectile.return_count"),      0.f,    0.f,      1000.f) \
	X(ProjectileRefractionCount,TEXT("projectile.refraction_count"),0.f,    0.f,      64.f) \
	X(ProjectileRefractionAngle,TEXT("projectile.refraction_angle"),30.f,   0.f,      360.f) \
	X(AreaRadius,             TEXT("area.radius"),                  0.f,    0.f,      1000.f) \
	X(ExplosionRadius,        TEXT("explosion.radius"),             0.f,    0.f,      1000.f) \
	X(ExplosionDamagePercent, TEXT("explosion.damage_percent"),     0.f,    -100.f,   1.0e9f) \
	X(OwnerDamageMultiplier,  TEXT("owner.damage_multiplier"),      1.f,    0.f,      1.0e9f)

namespace DWRules
{
	enum class EStat : uint16
	{
#define DW_RULE_STAT_ENUM(Id, Name, Default, Min, Max) Id,
		DW_RULE_SCALAR_STATS(DW_RULE_STAT_ENUM)
#undef DW_RULE_STAT_ENUM
		NumScalar
	};

	enum class EElementStat : uint8
	{
		DamageFlat,
		DamagePercent,
		Resistance,
		MaxResistance,
		Penetration,
		Num
	};

	constexpr int32 NumScalarStats = int32(EStat::NumScalar);
	constexpr int32 NumElementStats = int32(EElementStat::Num);
	constexpr int32 NumStats = NumScalarStats + NumElements * NumElementStats;

	FORCEINLINE uint16 StatId(EStat Stat)
	{
		return uint16(Stat);
	}

	FORCEINLINE uint16 ElementStatId(uint8 Element, EElementStat Kind)
	{
		return uint16(NumScalarStats + int32(Element) * NumElementStats + int32(Kind));
	}

	struct FDWStatInfo
	{
		FString Name;
		float Default = 0.f;
		float Min = 0.f;
		float Max = 0.f;
	};

	DWRULESCORE_API const FDWStatInfo& GetStatInfo(uint16 Stat);
	// Six damage types; resistance/max_resistance/penetration exist for the four elements.
	// "element.cold.*" remains an alias of "element.frost.*".
	DWRULESCORE_API bool FindStat(const FString& Path, uint16& OutStat);

	// PoE-style partitions: (set or base + Σadd) * max(0, 1 + (Σinc - Σred)/100) * Πmore, then bounds.
	struct FDWStatPartition
	{
		float Add = 0.f;
		float Increased = 0.f;
		float More = 1.f;
		float SetValue = 0.f;
		float Lower = -MAX_flt;
		float Upper = MAX_flt;
		bool bSet = false;
		bool bModified = false;
	};

	class DWRULESCORE_API FDWStatBlock
	{
	public:
		FDWStatBlock();

		void Apply(uint16 Stat, EModOp Op, float Value);
		void Finalize();

		float Get(uint16 Stat) const
		{
			return Resolved[Stat];
		}

		float Get(EStat Stat) const
		{
			return Resolved[uint16(Stat)];
		}

		// Evaluates the partition against an arbitrary base (e.g. an action's own field).
		float From(uint16 Stat, float Base) const;

		float From(EStat Stat, float Base) const
		{
			return From(uint16(Stat), Base);
		}

		bool IsModified(uint16 Stat) const
		{
			return Parts[Stat].bModified;
		}

	private:
		TArray<FDWStatPartition> Parts;
		TArray<float> Resolved;
	};
}
