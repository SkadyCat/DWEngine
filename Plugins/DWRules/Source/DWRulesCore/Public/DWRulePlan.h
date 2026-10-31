#pragma once

#include "DWRuleStats.h"
#include "DWRuleProgram.h"

// Immutable, flattened result of compiling one build. Shared by every cast of that build.
namespace DWRules
{
	enum class EExprOp : uint8
	{
		Const,
		EventAmount,
		EventDamage,
		EventStored,
		EventGeneration,
		EventSequence,
		State,
		Mul,
		Add,
		Div,
		Min,
		Max,
		Pow
	};

	struct FDWExprInstr
	{
		EExprOp Op = EExprOp::Const;
		uint8 Arity = 0;
		EScope Scope = EScope::Owner;
		float Value = 0.f;
		uint64 Key = 0;
	};

	// Count == 0 means the expression folded to Const at compile time.
	struct FDWExprRef
	{
		float Const = 0.f;
		int32 First = 0;
		uint16 Count = 0;
		bool bSet = false;
	};

	enum class ESlot : uint8
	{
		Count,
		Spread,
		Speed,
		Lifetime,
		Damage,
		Radius,
		Amount,
		Value,
		Cap,
		Threshold,
		Pierce,
		Duration,
		Interval,
		ArcHeight,
        DecayInterval, Rate, OffsetForward, OffsetUp, ScatterRadius,
		Num
	};

	DWRULESCORE_API bool FindSlot(FName Field, ESlot& Out);

	struct FDWActionDef
	{
		enum EFlag : uint8
		{
			Inherit = 1 << 0,
			ConsumeAll = 1 << 1,
			Parabolic = 1 << 2
		};

		EOp Op = EOp::EmitEvent;
		ELayout Layout = ELayout::Fan;
		EOrigin Origin = EOrigin::Event;
		EScope Scope = EScope::Owner;
		ERedirect Mode = ERedirect::Ricochet;
		int8 Element = -1;
		uint8 Flags = 0;
		uint16 Event = 0;
		uint16 EndEvent = 0;
		uint16 Kind = 0;
		uint16 ThenFirst = 0;
		uint16 ThenCount = 0;
		uint64 AddTags = 0;
		uint64 StateKey = 0;
		int32 Grant = INDEX_NONE;
        bool bAimTarget = false, bHemisphere = false, bFeed = false, bInvulnerable = false;
        FName Targeting;
		FDWExprRef Slots[int32(ESlot::Num)];

		const FDWExprRef& Slot(ESlot Which) const
		{
			return Slots[int32(Which)];
		}
	};

	struct FDWConditionDef
	{
		ECond Kind = ECond::Generation;
		ECompare Compare = ECompare::Gte;
		EScope Scope = EScope::Owner;
		uint16 EntityKind = 0;
		uint64 Key = 0;
		FDWExprRef Value;
		FName Active;
	};

	struct FDWRuleDef
	{
		FName Id;
		uint16 Event = 0;
		uint8 MaxDepth = 2;
		int8 Channel = -1;
		uint16 MaxEvents = 0;
		uint16 Trait = 0;
		uint64 RequireAll = 0;
		uint64 RequireAny = 0;
		uint64 Forbid = 0;
		uint16 ConditionFirst = 0;
		uint16 ConditionCount = 0;
		uint16 ActionFirst = 0;
		uint16 ActionCount = 0;
	};

	struct DWRULESCORE_API FDWCompiledPlan
	{
		uint64 Hash = 0;
        FName ActiveId;
		FDWStatBlock Stats;
		TArray<FDWRuleDef> Rules;
		TArray<FDWConditionDef> Conditions;
		TArray<FDWActionDef> Actions;
		TArray<FDWExprInstr> Expr;
		TArray<FDWGrantDef> Grants;
		// Rules are sorted by event; per event id: [first, first + count).
		TArray<uint16> EventRuleFirst;
		TArray<uint16> EventRuleCount;
		TArray<FName> EventNames;
		TArray<FName> TagNames;
		TArray<FName> KindNames;
		TArray<FName> ChannelNames;
		TArray<FName> TraitIds;

		bool HasRules(uint16 Event) const
		{
			return Event < EventRuleCount.Num() && EventRuleCount[Event] > 0;
		}

		int32 FindEvent(FName Name) const
		{
			return EventNames.IndexOfByKey(Name);
		}

		int32 FindKind(FName Name) const
		{
			return KindNames.IndexOfByKey(Name);
		}
	};

	// Stack evaluator shared by compile-time folding and the VM. Leaf resolves non-arithmetic ops.
	template <typename FLeaf>
	float EvaluateExpr(const FDWExprInstr* Code, int32 Count, FLeaf&& Leaf)
	{
		float Stack[MaxExprStack];
		int32 Top = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FDWExprInstr& In = Code[Index];
			switch (In.Op)
			{
			case EExprOp::Const:
				Stack[Top++] = In.Value;
				break;
			case EExprOp::Mul:
			case EExprOp::Add:
			case EExprOp::Div:
			case EExprOp::Min:
			case EExprOp::Max:
			case EExprOp::Pow:
			{
				const int32 Arity = In.Arity;
				float* Args = &Stack[Top - Arity];
				float Result = Args[0];
				for (int32 Arg = 1; Arg < Arity; ++Arg)
				{
					const float V = Args[Arg];
					switch (In.Op)
					{
					case EExprOp::Mul: Result *= V; break;
					case EExprOp::Add: Result += V; break;
					case EExprOp::Div: Result = V != 0.f ? Result / V : 0.f; break;
					case EExprOp::Min: Result = FMath::Min(Result, V); break;
					case EExprOp::Pow:
						Result = FMath::Pow(Result, V);
						Result = FMath::IsNaN(Result) ? 0.f : FMath::Clamp(Result, -MAX_flt, MAX_flt);
						break;
					default: Result = FMath::Max(Result, V); break;
					}
				}
				Top -= Arity;
				Stack[Top++] = Result;
				break;
			}
			default:
				Stack[Top++] = Leaf(In);
				break;
			}
		}
		return Top > 0 ? Stack[Top - 1] : 0.f;
	}
}
