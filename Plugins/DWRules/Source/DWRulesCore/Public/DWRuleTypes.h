#pragma once

#include "CoreMinimal.h"

// Shared vocabulary of the rule runtime. Units follow SkillStudio: meters, seconds.
namespace DWRules
{
	constexpr int32 NumElements = 6;
	constexpr int32 NumResistances = 4;
	constexpr int32 MaxChannels = 4;
	constexpr int32 MaxGeneration = 64;
	constexpr int32 MaxActionDepth = 12;
	constexpr int32 MaxExprStack = 32;
	constexpr int32 MaxTags = 64;
	constexpr uint32 InvalidId = MAX_uint32;

	enum class EElement : uint8
	{
		Fire,
		Frost,
		Lightning,
		Physical,
		Poison,
		Void,
		// Compatibility with existing C++ sample builds and JSON's "cold" spelling.
		Cold = Frost
	};

	// Target resistance arrays contain fire, frost, lightning and poison only.
	// Physical and void damage bypass elemental resistance.
	constexpr int32 ResistanceIndex(EElement Element)
	{
		switch (Element)
		{
		case EElement::Fire: return 0;
		case EElement::Frost: return 1;
		case EElement::Lightning: return 2;
		case EElement::Poison: return 3;
		default: return INDEX_NONE;
		}
	}

	// Events produced by the runtime itself. Author events get ids after Num.
	enum class EBuiltinEvent : uint16
	{
		Cast,
		Spawn,
		Hit,
		Critical,
		Kill,
		ProjectileEnd,
		Num
	};

	enum class EOp : uint8
	{
		Emit,
		Copy,
		DamageArea,
		DamageTarget,
		Counter,
		Accumulate,
		Consume,
		EmitEvent,
		Redirect,
		// Timed, named effect on the event target (chill, shock, ...). Strongest value wins.
		ApplyStatus,
		GrantTraits = 10,
		Summon = 11,
		Storm = 12,
        Sacrifice = 13,
        ConsumeStatus = 14,
        RepeatSkill = 15,
        MaintainEmitter = 16,
        SpawnMonster = 17,
        MultiplyDamage = 18
	};

	enum class ECond : uint8
	{
		Generation,
		State,
		Chance,
		EntityCount,
		// Live status magnitude on the event target.
		Status,
        Active
	};

	enum class ECompare : uint8
	{
		Eq,
		Ne,
		Gt,
		Gte,
		Lt,
		Lte
	};

	enum class EScope : uint8
	{
		Owner,
		Target,
		Cast
	};

	enum class EModOp : uint8
	{
		Add,
		Multiply,
		Set,
		IncreasedPercent,
		ReducedPercent,
		Min,
		Max
	};

	enum class ELayout : uint8
	{
		Fan,
		Radial,
		Point
	};

	enum class EOrigin : uint8
	{
		Event,
		Owner,
		Source,
		Target
	};

	enum class ERedirect : uint8
	{
		Ricochet,
		Reflect,
		Return
	};

	enum class EEndReason : uint8
	{
		Expired,
		Impact,
		Capacity
	};

	// Built-in tag bits. Author tags are interned after NumBuiltin per plan.
	namespace Tags
	{
		enum : uint8
		{
			Manual,
			Root,
			Direct,
			FirstHit,
			Critical,
			OwnerHit,
			OwnerKill,
			Projectile,
			Area,
			Refraction,
			Ricochet,
			Reflect,
			Return,
			EndExpired,
			EndImpact,
			EndCapacity,
			NumBuiltin
		};
	}

	constexpr uint64 TagBit(uint32 Index)
	{
		return uint64(1) << Index;
	}

	// Delivery facts and end reasons describe one event; derived carriers never inherit them.
	constexpr uint64 NonInheritedTags =
		TagBit(Tags::Manual) | TagBit(Tags::Root) | TagBit(Tags::Direct) | TagBit(Tags::FirstHit) |
		TagBit(Tags::Critical) | TagBit(Tags::OwnerHit) | TagBit(Tags::OwnerKill) |
		TagBit(Tags::EndExpired) | TagBit(Tags::EndImpact) | TagBit(Tags::EndCapacity);

	DWRULESCORE_API bool ParseElement(const FString& Text, EElement& Out);
	DWRULESCORE_API const TCHAR* ElementName(EElement Element);
	DWRULESCORE_API const TCHAR* BuiltinEventName(EBuiltinEvent Event);
	DWRULESCORE_API const TCHAR* BuiltinTagName(uint32 Index);
	DWRULESCORE_API bool Compare(ECompare Op, float A, float B);
	DWRULESCORE_API uint64 HashName(const FString& Text);

	// Statuses are global by name (not per trait): one chill, whoever applied it.
	inline uint64 StatusKey(const FString& Name)
	{
		return HashName(TEXT("status:") + Name);
	}

	// What the runtime does with a live status magnitude (Content/Rules/Statuses/*.json).
	// Engine-side: DamageTaken, DamageDealt, CriticalChance, DamageOverTime. Host-side: MoveSpeed, ActionSpeed, Control.
	enum class EStatusEffect : uint8
	{
		DamageTaken,
		DamageDealt,
		CriticalChance,
		DamageOverTime,
		MoveSpeed,
		ActionSpeed,
		Control,
		Num
	};

	enum class EStatusStacking : uint8
	{
		// Strongest magnitude, longest remaining duration (default, also for undefined statuses).
		Strongest,
		// Latest application replaces magnitude and duration.
		Refresh,
		// +1 stack per application up to MaxStacks; magnitude = value x stacks; duration refreshed.
		Stack
	};

	enum class EStatusControl : uint8
	{
		None,
		Freeze,
		Stun
	};

	constexpr int32 MaxStatusStacks = 100;
	constexpr int32 MaxTicksPerSubstep = 8;
	constexpr double MinTickInterval = 0.05;

	DWRULESCORE_API bool ParseStatusEffect(const FString& Text, EStatusEffect& Out);
	DWRULESCORE_API const TCHAR* StatusEffectName(EStatusEffect Effect);

	FORCEINLINE uint64 Mix64(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}

	// Order-independent randomness: a value depends only on (seed, sequence, salt).
	FORCEINLINE float Random01(uint64 Seed, uint64 Sequence, uint64 Salt)
	{
		const uint64 Bits = Mix64(Seed ^ Mix64(Sequence ^ Mix64(Salt)));
		return float(Bits >> 40) * (1.0f / 16777216.0f);
	}
}
