#pragma once

#include "DWRuleTypes.h"

// Authoring-side program model. Mirrors the SkillStudio affix-framework contract
// (public/affix-framework/contract.mjs) closely enough for one-to-one import.
namespace DWRules
{
	// Collection/item reference. Overrides and stitched payloads remain typed after JSON loading.
	struct FDWTraitEntry
	{
		FName Id;
		TMap<FName, double> Params;
		TArray<FDWTraitEntry> With;
		double Duration = 0.0;
		// Authoring metadata; compiled sources only receive the saved concrete Params.
		int32 Tier = 0;
	};

	struct FDWSheetValue
	{
		enum class EKind : uint8 { Number, Param, Add, Sub, Mul, Div };
		EKind Kind = EKind::Number;
		double Number = 0.0;
		FName Param;
		TArray<FDWSheetValue> Args;
		DWRULESCORE_API bool Evaluate(const TMap<FName, double>& Params, double& Out) const;
	};

	struct FDWSheetEntry
	{
		FName Stat;
		FName Trigger;
        FName GrantSkill;
		FDWSheetValue Value;
	};

	struct FDWExprSource
	{
		enum class EKind : uint8
		{
			Const,
			Param,
			Event,
			State,
			Stat,
			Mul,
			Add,
			Div,
			Min,
			Max,
			Pow
		};

		EKind Kind = EKind::Const;
		double Const = 0.0;
		FName Name;
		EScope Scope = EScope::Owner;
		FName Channel;
		TArray<FDWExprSource> Args;

		static FDWExprSource Number(double Value)
		{
			FDWExprSource Expr;
			Expr.Const = Value;
			return Expr;
		}

		static FDWExprSource Param(FName Name)
		{
			FDWExprSource Expr;
			Expr.Kind = EKind::Param;
			Expr.Name = Name;
			return Expr;
		}

		static FDWExprSource Event(FName Field)
		{
			FDWExprSource Expr;
			Expr.Kind = EKind::Event;
			Expr.Name = Field;
			return Expr;
		}
	};

	struct FDWModifierSource
	{
		FString Stat;
		EModOp Op = EModOp::Add;
		FDWExprSource Value;
	};

	struct FDWConditionSource
	{
		FName Op;
		FName Tag;
		FName Key;
		FName Kind;
		FName Channel;
		EScope Scope = EScope::Owner;
		ECompare Compare = ECompare::Gte;
		FDWExprSource Value;
	};

	struct FDWActionSource
	{
		EOp Op = EOp::EmitEvent;
		// Numeric fields by contract name: count, spread, speed, lifetime, damage, radius,
		// amount, value, cap, threshold, pierce.
		TMap<FName, FDWExprSource> Numbers;
		FName Kind;
		FName Event;
		FName EmitEvent;
		FName EndEvent;
		FName Key;
		FName Channel;
		EScope Scope = EScope::Owner;
		ELayout Layout = ELayout::Fan;
		EOrigin Origin = EOrigin::Event;
		ERedirect Mode = ERedirect::Ricochet;
		int8 Element = -1;
		bool bInherit = true;
		bool bConsumeAll = false;
		bool bParabolic = false;
        bool bAimTarget = false, bHemisphere = false, bFeed = false, bInvulnerable = false;
        FName Targeting;
		TArray<FName> Tags;
		TArray<FDWActionSource> Then;
		bool bHasGrant = false;
		bool bGrantStitched = false;
		TArray<FDWTraitEntry> GrantEntries;

		FDWActionSource& Set(FName Field, double Value)
		{
			Numbers.Add(Field, FDWExprSource::Number(Value));
			return *this;
		}

		FDWActionSource& SetExpr(FName Field, const FDWExprSource& Value)
		{
			Numbers.Add(Field, Value);
			return *this;
		}
	};

	struct FDWRuleSource
	{
		FName Id;
		FName Event;
		TArray<FDWConditionSource> Conditions;
		TArray<FDWActionSource> Actions;
		int32 MaxDepth = 2;
		// 0 selects the world default.
		int32 MaxEvents = 0;
		FName Channel;
	};

	struct FDWTraitSource
	{
		FName Id;
		// A trait is active only while its parent is active (support -> rune/talent chains).
		FName ParentId;
		TMap<FName, double> Params;
		TArray<FDWModifierSource> Modifiers;
		TArray<FDWRuleSource> Rules;
		TArray<FDWSheetEntry> Sheet;
		TArray<FDWTraitSource> Stitched;
	};

	struct FDWBuildSource
	{
		FName ActiveId;
		TArray<FDWTraitSource> Traits;
	};

	struct FDWGrantDef
	{
		FString Key;
		TArray<FDWTraitEntry> Entries;
		TArray<FDWTraitSource> Sources;
	};

	// Stable parameter/payload identity shared by compilation, held effects and host caches.
	DWRULESCORE_API FString TraitSourceKey(const FDWTraitSource& Source);
}
