#include "DWRuleSamples.h"

namespace DWRules
{
	namespace
	{
		FDWModifierSource Modifier(const TCHAR* Stat, EModOp Op, double Value)
		{
			FDWModifierSource Result;
			Result.Stat = Stat;
			Result.Op = Op;
			Result.Value = FDWExprSource::Number(Value);
			return Result;
		}

		FDWConditionSource TagCondition(FName Tag)
		{
			FDWConditionSource Result;
			Result.Op = TEXT("tag");
			Result.Tag = Tag;
			return Result;
		}
	}

	FDWTraitSource FDWRuleSamples::IceLance()
	{
		FDWTraitSource Trait;
		Trait.Id = TEXT("ice_lance");
		Trait.Params.Add(TEXT("damage"), 12.0);
		Trait.Modifiers.Add(Modifier(TEXT("projectile.damage"), EModOp::Set, 12.0));
		Trait.Modifiers.Add(Modifier(TEXT("projectile.speed"), EModOp::Set, 18.0));
		Trait.Modifiers.Add(Modifier(TEXT("projectile.lifetime"), EModOp::Set, 1.5));
		Trait.Modifiers.Add(Modifier(TEXT("projectile.radius"), EModOp::Set, 0.25));
		Trait.Modifiers.Add(Modifier(TEXT("element.cold.damage_flat"), EModOp::Add, 2.0));

		FDWActionSource Emit;
		Emit.Op = EOp::Emit;
		Emit.Kind = TEXT("ice_lance");
		Emit.Element = int8(EElement::Cold);
		Emit.Origin = EOrigin::Event;
		Emit.Tags.Add(TEXT("cold"));
		Emit.SetExpr(TEXT("damage"), FDWExprSource::Param(TEXT("damage")));

		FDWRuleSource Rule;
		Rule.Id = TEXT("cast_lance");
		Rule.Event = TEXT("cast");
		Rule.Actions.Add(Emit);
		Trait.Rules.Add(Rule);
		return Trait;
	}

	FDWTraitSource FDWRuleSamples::Multiple(int32 Extra)
	{
		FDWTraitSource Trait;
		Trait.Id = TEXT("multiple_projectiles");
		Trait.ParentId = TEXT("ice_lance");
		Trait.Modifiers.Add(Modifier(TEXT("projectile.count"), EModOp::Add, Extra));
		Trait.Modifiers.Add(Modifier(TEXT("projectile.spread"), EModOp::Set, 50.0));
		Trait.Modifiers.Add(Modifier(TEXT("projectile.damage"), EModOp::Multiply, 0.8));
		return Trait;
	}

	FDWTraitSource FDWRuleSamples::Pierce(int32 Count)
	{
		FDWTraitSource Trait;
		Trait.Id = TEXT("pierce");
		Trait.ParentId = TEXT("ice_lance");
		Trait.Modifiers.Add(Modifier(TEXT("projectile.pierce"), EModOp::Add, Count));
		return Trait;
	}

	FDWTraitSource FDWRuleSamples::Ricochet(int32 Count)
	{
		FDWTraitSource Trait;
		Trait.Id = TEXT("ricochet");
		Trait.ParentId = TEXT("ice_lance");
		Trait.Modifiers.Add(Modifier(TEXT("projectile.ricochet_count"), EModOp::Add, Count));

		FDWActionSource Redirect;
		Redirect.Op = EOp::Redirect;
		Redirect.Mode = ERedirect::Ricochet;

		FDWRuleSource Rule;
		Rule.Id = TEXT("ricochet_on_hit");
		Rule.Event = TEXT("hit");
		Rule.MaxDepth = 8;
		Rule.Conditions.Add(TagCondition(TEXT("projectile")));
		Rule.Actions.Add(Redirect);
		Trait.Rules.Add(Rule);
		return Trait;
	}

	FDWTraitSource FDWRuleSamples::ShatterOnKill()
	{
		FDWTraitSource Trait;
		Trait.Id = TEXT("shatter");
		Trait.ParentId = TEXT("ice_lance");
		Trait.Params.Add(TEXT("radius"), 3.0);
		Trait.Params.Add(TEXT("amount"), 15.0);

		FDWActionSource Burst;
		Burst.Op = EOp::DamageArea;
		Burst.Element = int8(EElement::Cold);
		Burst.EmitEvent = TEXT("shatter_hit");
		Burst.Tags.Add(TEXT("shatter"));
		Burst.SetExpr(TEXT("radius"), FDWExprSource::Param(TEXT("radius")));
		Burst.SetExpr(TEXT("amount"), FDWExprSource::Param(TEXT("amount")));

		FDWRuleSource Rule;
		Rule.Id = TEXT("shatter_on_kill");
		Rule.Event = TEXT("kill");
		Rule.Channel = TEXT("shatter");
		// Chain explosions at most 3 deep per root, independent of projectile generation.
		Rule.MaxDepth = 3;
		Rule.Actions.Add(Burst);
		Trait.Rules.Add(Rule);
		return Trait;
	}

	FDWTraitSource FDWRuleSamples::FrostReservoir()
	{
		FDWTraitSource Trait;
		Trait.Id = TEXT("frost_reservoir");
		Trait.ParentId = TEXT("ice_lance");

		FDWActionSource Nova;
		Nova.Op = EOp::Emit;
		Nova.Kind = TEXT("ice_shard");
		Nova.Layout = ELayout::Radial;
		Nova.bInherit = false;
		Nova.Element = int8(EElement::Cold);
		Nova.Tags.Add(TEXT("nova"));
		Nova.Set(TEXT("count"), 8);
		Nova.Set(TEXT("speed"), 14);
		Nova.Set(TEXT("lifetime"), 0.6);
		Nova.Set(TEXT("damage"), 6);
		Nova.Set(TEXT("radius"), 0.2);

		FDWActionSource Store;
		Store.Op = EOp::Accumulate;
		Store.Key = TEXT("frost");
		Store.Scope = EScope::Owner;
		Store.SetExpr(TEXT("value"), FDWExprSource::Event(TEXT("amount")));
		Store.Set(TEXT("cap"), 2000);
		Store.Set(TEXT("threshold"), 250);
		Store.Then.Add(Nova);

		FDWRuleSource Rule;
		Rule.Id = TEXT("store_frost");
		Rule.Event = TEXT("hit");
		Rule.MaxDepth = 3;
		Rule.Conditions.Add(TagCondition(TEXT("cold")));
		Rule.Actions.Add(Store);
		Trait.Rules.Add(Rule);
		return Trait;
	}

	FDWTraitSource FDWRuleSamples::ColdMastery(float IncreasedPercent)
	{
		FDWTraitSource Trait;
		Trait.Id = TEXT("cold_mastery");
		Trait.Modifiers.Add(Modifier(TEXT("element.cold.damage_percent"), EModOp::IncreasedPercent, IncreasedPercent));
		Trait.Modifiers.Add(Modifier(TEXT("owner.critical_chance"), EModOp::Add, 10.0));
		return Trait;
	}

	FDWBuildSource FDWRuleSamples::StackedIceLance()
	{
		FDWBuildSource Build;
		Build.ActiveId = TEXT("ice_lance");
		Build.Traits.Add(IceLance());
		Build.Traits.Add(Multiple(4));
		Build.Traits.Add(Pierce(2));
		Build.Traits.Add(Ricochet(2));
		Build.Traits.Add(ShatterOnKill());
		Build.Traits.Add(FrostReservoir());
		Build.Traits.Add(ColdMastery(60.f));
		return Build;
	}
}
