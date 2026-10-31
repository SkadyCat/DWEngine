#include "DWRuleStats.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, DWRulesCore);

namespace DWRules
{
	namespace
	{
		const TCHAR* const ElementNames[NumElements] = {
			TEXT("fire"),
			TEXT("frost"),
			TEXT("lightning"),
			TEXT("physical"),
			TEXT("poison"),
			TEXT("void")
		};

		const TCHAR* const ElementStatNames[NumElementStats] = {
			TEXT("damage_flat"),
			TEXT("damage_percent"),
			TEXT("resistance"),
			TEXT("max_resistance"),
			TEXT("penetration")
		};

		const TCHAR* const EventNames[int32(EBuiltinEvent::Num)] = {
			TEXT("cast"),
			TEXT("spawn"),
			TEXT("hit"),
			TEXT("critical"),
			TEXT("kill"),
			TEXT("projectile_end")
		};

		const TCHAR* const TagNames[Tags::NumBuiltin] = {
			TEXT("manual"),
			TEXT("root"),
			TEXT("direct"),
			TEXT("first_hit"),
			TEXT("critical"),
			TEXT("owner_hit"),
			TEXT("owner_kill"),
			TEXT("projectile"),
			TEXT("area"),
			TEXT("refraction"),
			TEXT("ricochet"),
			TEXT("reflect"),
			TEXT("return"),
			TEXT("end_expired"),
			TEXT("end_impact"),
			TEXT("end_capacity")
		};

		TArray<FDWStatInfo> BuildStatTable()
		{
			TArray<FDWStatInfo> Table;
			Table.Reserve(NumStats);
#define DW_RULE_STAT_INFO(Id, Name, Default, Min, Max) Table.Add({ FString(Name), Default, Min, Max });
			DW_RULE_SCALAR_STATS(DW_RULE_STAT_INFO)
#undef DW_RULE_STAT_INFO
			for (int32 Element = 0; Element < NumElements; ++Element)
			{
				for (int32 Kind = 0; Kind < NumElementStats; ++Kind)
				{
					FDWStatInfo Info;
					const EElementStat Stat = EElementStat(Kind);
					const bool bSupported = ResistanceIndex(EElement(Element)) != INDEX_NONE ||
						Stat == EElementStat::DamageFlat || Stat == EElementStat::DamagePercent;
					// Keep dense native slots, but expose only the four real resistance types.
					if (bSupported)
					{
						Info.Name = FString::Printf(TEXT("element.%s.%s"), ElementNames[Element], ElementStatNames[Kind]);
					}
					Info.Default = Stat == EElementStat::MaxResistance ? 75.f : 0.f;
					Info.Min = Stat == EElementStat::DamagePercent ? -100.f : (Stat == EElementStat::Resistance ? -1.0e9f : 0.f);
					Info.Max = (Stat == EElementStat::MaxResistance) ? 100.f : 1.0e9f;
					Table.Add(Info);
				}
			}
			return Table;
		}

		const TArray<FDWStatInfo>& StatTable()
		{
			static const TArray<FDWStatInfo> Table = BuildStatTable();
			return Table;
		}
	}

	bool ParseElement(const FString& Text, EElement& Out)
	{
		if (Text.Equals(TEXT("cold"), ESearchCase::IgnoreCase))
		{
			Out = EElement::Frost;
			return true;
		}
		for (int32 Index = 0; Index < NumElements; ++Index)
		{
			if (Text.Equals(ElementNames[Index], ESearchCase::IgnoreCase))
			{
				Out = EElement(Index);
				return true;
			}
		}
		return false;
	}

	const TCHAR* ElementName(EElement Element)
	{
		return ElementNames[int32(Element)];
	}

	namespace
	{
		const TCHAR* const StatusEffectNames[] = {
			TEXT("damage_taken"), TEXT("damage_dealt"), TEXT("critical_chance"), TEXT("damage_over_time"),
			TEXT("move_speed"), TEXT("action_speed"), TEXT("control")
		};
		static_assert(UE_ARRAY_COUNT(StatusEffectNames) == int32(EStatusEffect::Num), "status effect names");
	}

	bool ParseStatusEffect(const FString& Text, EStatusEffect& Out)
	{
		for (int32 Index = 0; Index < int32(EStatusEffect::Num); ++Index)
		{
			if (Text.Equals(StatusEffectNames[Index], ESearchCase::IgnoreCase))
			{
				Out = EStatusEffect(Index);
				return true;
			}
		}
		return false;
	}

	const TCHAR* StatusEffectName(EStatusEffect Effect)
	{
		return int32(Effect) < int32(EStatusEffect::Num) ? StatusEffectNames[int32(Effect)] : TEXT("");
	}

	const TCHAR* BuiltinEventName(EBuiltinEvent Event)
	{
		return EventNames[int32(Event)];
	}

	const TCHAR* BuiltinTagName(uint32 Index)
	{
		return Index < Tags::NumBuiltin ? TagNames[Index] : TEXT("");
	}

	bool Compare(ECompare Op, float A, float B)
	{
		switch (Op)
		{
		case ECompare::Eq: return A == B;
		case ECompare::Ne: return A != B;
		case ECompare::Gt: return A > B;
		case ECompare::Gte: return A >= B;
		case ECompare::Lt: return A < B;
		default: return A <= B;
		}
	}

	uint64 HashName(const FString& Text)
	{
		uint64 Hash = 1469598103934665603ull;
		for (TCHAR Char : Text)
		{
			Hash ^= uint64(FChar::ToLower(Char));
			Hash *= 1099511628211ull;
		}
		return Hash;
	}

	const FDWStatInfo& GetStatInfo(uint16 Stat)
	{
		return StatTable()[Stat];
	}

	bool FindStat(const FString& Path, uint16& OutStat)
	{
		FString Canonical = Path;
		const FString LegacyPrefix = TEXT("element.cold.");
		if (Canonical.StartsWith(LegacyPrefix, ESearchCase::IgnoreCase))
		{
			Canonical = TEXT("element.frost.") + Canonical.RightChop(LegacyPrefix.Len());
		}
		const TArray<FDWStatInfo>& Table = StatTable();
		for (int32 Index = 0; Index < Table.Num(); ++Index)
		{
			if (!Table[Index].Name.IsEmpty() && Table[Index].Name.Equals(Canonical, ESearchCase::IgnoreCase))
			{
				OutStat = uint16(Index);
				return true;
			}
		}
		return false;
	}

	FDWStatBlock::FDWStatBlock()
	{
		Parts.SetNum(NumStats);
		Resolved.SetNumUninitialized(NumStats);
		for (int32 Index = 0; Index < NumStats; ++Index)
		{
			Resolved[Index] = GetStatInfo(uint16(Index)).Default;
		}
	}

	void FDWStatBlock::Apply(uint16 Stat, EModOp Op, float Value)
	{
		FDWStatPartition& Part = Parts[Stat];
		Part.bModified = true;
		switch (Op)
		{
		case EModOp::Add:
			Part.Add += Value;
			break;
		case EModOp::Multiply:
			Part.More *= Value;
			break;
		case EModOp::Set:
			Part.bSet = true;
			Part.SetValue = Value;
			break;
		case EModOp::IncreasedPercent:
			Part.Increased += Value;
			break;
		case EModOp::ReducedPercent:
			Part.Increased -= Value;
			break;
		case EModOp::Min:
			Part.Lower = FMath::Max(Part.Lower, Value);
			break;
		case EModOp::Max:
			Part.Upper = FMath::Min(Part.Upper, Value);
			break;
		}
	}

	void FDWStatBlock::Finalize()
	{
		for (int32 Index = 0; Index < NumStats; ++Index)
		{
			Resolved[Index] = From(uint16(Index), GetStatInfo(uint16(Index)).Default);
		}
	}

	float FDWStatBlock::From(uint16 Stat, float Base) const
	{
		const FDWStatPartition& Part = Parts[Stat];
		const FDWStatInfo& Info = GetStatInfo(Stat);
		const float Start = Part.bSet ? Part.SetValue : Base;
		const float Scale = FMath::Max(0.f, 1.f + Part.Increased / 100.f);
		const float Value = (Start + Part.Add) * Scale * Part.More;
		const float Lower = FMath::Max(Info.Min, Part.Lower);
		const float Upper = FMath::Min(Info.Max, Part.Upper);
		return FMath::Clamp(Value, Lower, FMath::Max(Lower, Upper));
	}
}
