#include "DWRuleAilments.h"
#include "DWRuleCombat.h"
#include "DWRuleMinions.h"
#include "DWRuleProjectileAffixes.h"

namespace DWRules
{
FDWDamageMod FDWCombatRuntime::ModifyDamage(const FDWRoot &Root, int32 Target, float Amount, uint8 Element,
											FDWDamageInfo &Info)
{
	FDWDamageMod M;
    if(World.TargetData.ExternalId[Target]==World.GetOwnerStatusId(Hero.Owner)&&CanReceiveHeroDamage&&!CanReceiveHeroDamage())
    { M.bMiss=true;return M; }
	if (Info.bReflected) return M;
	if (Root.Owner != Hero.Owner)
	{
		if (const auto *Attacker = EnemyOfOwner(Root.Owner))
			if (Ailments->Layers(Attacker->Id, TEXT("poison")))
				M.More = FMath::Max(0., 1 - S(TEXT("poison.enemy_weaken")) / 100);
		return M;
	}
	const uint32 Id = World.TargetData.ExternalId[Target];
	auto *RI = RootInfo(Info.RootIndex);
	const auto &Plan = World.GetPlan(Root.Plan);
	const bool Projectile = !!(Info.Tags & TagBit(Tags::Projectile)), Area = !!(Info.Tags & TagBit(Tags::Area));
	const bool Minion = (RI && RI->bMinion) || HasTag(Plan, Info.Tags, TEXT("summon")),
			   Triggered = RI && RI->bTriggered;
	const auto *Enemy = this->Enemy(Id);
	const double Life = LifeFraction(Id);
	const int32 Poisoned = Ailments->Layers(Id, TEXT("poison"));
	const bool Shocked = Status(Id, TEXT("shock")) > 0, Chilled = Status(Id, TEXT("chill")) > 0;
	const bool Frozen = Status(Id, TEXT("freeze")) > 0, Burning = Ailments->IsBurning(Id);
	if (RI)
	{
		M.More *= RI->More != 0 ? RI->More : 1;
		M.Increased += RI->Increased;
		M.Critical += RI->CriticalBonus;
	}
	if (Projectile)
	{
		M.Increased += S(TEXT("proj.damage"));
		M.More *= S(TEXT("proj.more"));
		M.CriticalScale += S(TEXT("proj.crit")) / 100;
		M.CriticalMultiplier += S(TEXT("proj.crit_multi"));
	}
	if (Area)
	{
		M.Increased += S(TEXT("area.damage"));
		M.More *= S(TEXT("area.more"));
		M.CriticalScale += S(TEXT("area.crit")) / 100;
		M.CriticalMultiplier += S(TEXT("area.crit_multi"));
	}
	if (Triggered)
	{
		M.Increased += S(TEXT("trigger.damage"));
		M.More *= S(TEXT("trigger.more")) * (1 + S(TEXT("trigger.no_manual")) / 100);
	}
	if (Minion)
		Minions->DamageMod(M, RI, Element, Id);
	static const TCHAR *Names[] = {TEXT("fire"),	 TEXT("frost"),	 TEXT("lightning"),
								   TEXT("physical"), TEXT("poison"), TEXT("void")};
	M.Penetration += S(FName(*(FString(TEXT("damage.pen.")) + Names[FMath::Min<int32>(Element, 5)])));
	if (Element == 0)
	{
		M.CriticalMultiplier += S(TEXT("fire.crit_multi"));
		if (Burning)
			M.Increased += S(TEXT("burn.vs_fire"));
	}
	if (Element == 1)
	{
		if (Chilled)
			M.Increased += S(TEXT("chill.vs_frost"));
		for (const auto *T : this->T(TEXT("slow_frost")))
			M.More *= 1 + FMath::Min(T->Param(TEXT("max")), Ailments->SlowOf(Id)) / 100;
	}
	if (Element == 2)
	{
		M.CriticalScale += S(TEXT("lightning.crit")) / 100;
		if (Shocked)
			M.Increased += S(TEXT("shock.vs_lightning"));
		const int32 Layers = Ailments->ShockLayers(Id);
		for (const auto *T : this->T(TEXT("shock_per_layer")))
			M.Increased += T->Param(TEXT("n")) * Layers;
		if (S(TEXT("lightning.max")) || S(TEXT("lightning.min")))
		{
			const double Hi = 1.5 * (1 + S(TEXT("lightning.max")) / 100),
						 Lo = FMath::Min(Hi, .5 * (1 + S(TEXT("lightning.min")) / 100));
			M.More *= Lo + (Hi - Lo) * Random();
		}
		if (Layers > 0 && Has(TEXT("shock_consume")) && Info.Strike.IsNone())
		{
			for (const auto *T : this->T(TEXT("shock_consume")))
				M.More *= 1 + T->Param(TEXT("n")) * Layers / 100;
			Ailments->ConsumeShock(Id);
		}
	}
	if (Element == 5)
	{
		if (Life >= .999)
			M.Increased += S(TEXT("void.vs_full"));
		if (Life <= .35)
			M.Increased += S(TEXT("void.vs_low"));
		if (Enemy && Enemy->Shield > 0)
			M.Increased += S(TEXT("void.vs_shield"));
		const int32 Layers = Ailments->Layers(Id, TEXT("erosion"));
		for (const auto *T : this->T(TEXT("erosion_per_layer")))
			M.Increased += T->Param(TEXT("n")) * Layers;
		for (const auto *T : this->T(TEXT("poison_void_taken")))
			M.Taken *= 1 + FMath::Min(T->Param(TEXT("max")), T->Param(TEXT("n")) * Poisoned) / 100;
		for (const auto *T : this->T(TEXT("erosion_consume_layers")))
			if (Layers > 0 && Info.Strike.IsNone())
				M.More *= 1 + .15 * Ailments->RemoveLayers(Id, TEXT("erosion"), int32(T->Param(TEXT("n"))));
	}
	if (Poisoned)
	{
		M.Increased += S(TEXT("poison.vs_hit"));
		M.CriticalScale += S(TEXT("poison.vs_crit")) / 100;
	}
	if (Burning)
		M.Increased += S(TEXT("burn.vs_hit"));
	if (Frozen)
	{
		M.CriticalScale += S(TEXT("freeze.vs_crit")) / 100;
		M.CriticalMultiplier += S(TEXT("freeze.vs_crit_multi"));
	}
	if (Frozen && S(TEXT("freeze.consume")) && Info.Strike.IsNone())
	{
		M.More *= 1 + .3 * World.GetStatusRemaining(Id, StatusKey(TEXT("freeze")));
		Ailments->RemoveFreeze(Id, true);
	}
	M.Taken *= Ailments->TakenMultiplier(Id, Element);
	if (Projectile && Info.SourceIndex != INDEX_NONE)
		Projectiles->DamageMod(M, Info, Id, RI);
	if (Area && Info.ZoneMore != 0)
		M.More *= Info.ZoneMore;
	const double Converted = Info.Strike.IsNone() ? FMath::Min(100., ConvertTotal(Element)) : 0;
	const double Keep = FMath::Max(1.e-4, 1 - Converted / 100);
	if (Converted > 0)
		M.More *= Keep;
	double Execute = 0;
	if (Element == 5 && Enemy && !Enemy->bElite)
		for (const auto *T : this->T(TEXT("void_execute")))
			Execute = FMath::Max(Execute, T->Param(TEXT("n")));
	const double Health = World.TargetData.Health[Target];
	M.Adjust = [this, Id, Converted, Keep, Execute, Health, &Info](double Dealt, bool Critical) {
		LastCritical.Add(Id, Critical);
		if (Converted > 0)
			Info.Converted = Dealt / Keep * Converted / 100;
		const auto *M = this->Enemy(Id);
		if (Execute > 0 && M && (Health - Dealt) / FMath::Max(1., M->MaxHealth + M->MaxShield) * 100 < Execute &&
			Health - Dealt > 0)
		{
			Info.bExecuted = true;
			return Health;
		}
		return Dealt;
	};
	return M;
}
} // namespace DWRules
