#include "DWRuleAilments.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleProjectileAffixes.h"

namespace DWRules
{
void FDWAilments::OnHit(const FDWCombatHit &H)
{
	if (H.bDot)
	{
		if (H.bKilled)
			OnKill(H);
		return;
	}
	double Chance = 0, Effect = 0;
	if (H.bProjectile)
	{
		Chance += S(TEXT("proj.ailment_chance"));
		Effect += S(TEXT("proj.ailment_effect"));
	}
	if (H.bArea)
	{
		Chance += S(TEXT("area.ailment_chance"));
		Effect += S(TEXT("area.ailment_effect"));
	}
	if (H.bMinion)
	{
		Chance += S(TEXT("minion.ailment_chance"));
		Effect += S(TEXT("minion.ailment_effect"));
	}
	if (H.bTriggered)
		Effect += S(TEXT("trigger.effect"));
	PendingEffect = Effect;
	if (H.bSilent)
	{
		PendingEffect = 0;
		if (H.bKilled)
			OnKill(H);
		return;
	}
	FDWAilmentApplication O;
	O.Amount = H.Amount;
	O.Element = H.Element;
	O.bCritical = H.bCritical;
	O.Effect = Effect;
	O.bMinion = H.bMinion;
	if (R.Chance(S(TEXT("poison.chance")) + Chance) && !H.bKilled)
		ApplyPoison(H.Id, O);
	if (!H.bKilled)
	{
		if (R.Chance(S(TEXT("shock.chance")) + (H.Element == 2 ? Chance : 0)))
			R.World.ApplyStatusKey(H.Id, StatusKey(TEXT("shock")), 20, 3, R.Hero.Owner);
		if (R.Chance(S(TEXT("chill.chance")) + (H.Element == 1 ? Chance : 0)))
			R.World.ApplyStatusKey(H.Id, StatusKey(TEXT("chill")), 30, 2, R.Hero.Owner);
		if (R.Chance(S(TEXT("ignite.chance")) + (H.Element == 0 ? Chance : 0)))
			R.World.ApplyStatusKey(H.Id, StatusKey(TEXT("ignite")), float(H.Amount * .4), 4, R.Hero.Owner);
		if (R.Chance(S(TEXT("erosion.chance")) + (H.Element == 5 ? Chance : 0)))
			ApplyErosion(H.Id, O);
		if (H.Element == 1)
			BuildFreeze(H);
	}
	PendingEffect = 0;
	if (Layers(H.Id, TEXT("poison")) && H.Info.Strike.IsNone())
	{
		for (const auto *T : R.T(TEXT("poison_hit_detonate")))
			if (R.Chance(T->Param(TEXT("chance"))))
				Detonate(H.Id, int32(T->Param(TEXT("layers"))));
		if (H.bCritical)
			for (const auto *T : R.T(TEXT("poison_crit_detonate")))
				if (R.Chance(T->Param(TEXT("chance"))))
					Detonate(H.Id, int32(T->Param(TEXT("layers"))));
		if (H.bCritical && R.Chance(S(TEXT("poison.crit_spread_chance"))))
			PoisonSpread(H.Id, FMath::CeilToInt(Layers(H.Id, TEXT("poison")) / 2.), 0);
		const double Last = LastToVoid.Contains(H.Id) ? LastToVoid[H.Id] : -1;
		if (S(TEXT("poison.to_void")) > 0 && Last + .5 <= R.Time)
		{
			LastToVoid.Add(H.Id, R.Time);
			FDWStrikeOptions Options;
			Options.Effect = TEXT("poison_to_void");
			R.Hit(H.Id, Remaining(H.Id, TEXT("poison")) * S(TEXT("poison.to_void")) / 100, 5, Options);
		}
	}
	if (H.Info.Strike.IsNone())
	{
		if (H.Element == 0 && Layers(H.Id, TEXT("ignite")))
			for (const auto *T : R.T(TEXT("ignite_consume")))
			{
				FDWStrikeOptions Options;
				Options.Effect = TEXT("ignite_consume");
				R.Hit(H.Id, IgniteRemaining(H.Id) * T->Param(TEXT("n")) / 100, 0, Options);
				if (!R.Chance(S(TEXT("ignite.keep_chance"))))
				{
					Ignite.Remove(H.Id);
					SyncIgnite(H.Id);
				}
				R.Triggers->Ailment(TEXT("consumed"), H.Id);
			}
		if (H.Element == 5 && Layers(H.Id, TEXT("erosion")))
			for (const auto *T : R.T(TEXT("erosion_consume_all")))
			{
				FDWStrikeOptions Options;
				Options.Effect = TEXT("erosion_consume");
				R.Hit(H.Id, Remaining(H.Id, TEXT("erosion")) * T->Param(TEXT("n")) / 100, 5, Options);
				RemoveLayers(H.Id, TEXT("erosion"));
			}
	}
	if (R.Status(H.Id, TEXT("shock")) > 0 && R.Chance(S(TEXT("shock.arc_chance"))) && H.Depth < 2)
	{
		FVector3d P;
		if (R.TargetPosition(H.Id, P))
		{
			const auto Near = R.EnemiesNear(P, 5, {H.Id});
			if (Near.Num())
			{
				FDWStrikeOptions Options;
				Options.Only = Near[0];
				Options.bArea = false;
				Options.Effect = TEXT("arc");
				Options.Depth = H.Depth + 1;
				auto &Visual = R.Effects.AddDefaulted_GetRef();
				Visual.Kind = TEXT("arc");
				Visual.Shape = TEXT("beam");
				Visual.Position = P;
				Visual.End = R.Enemy(Near[0])->Position;
				Visual.Element = 2;
				Visual.From = H.Id;
				Visual.To = Near[0];
				R.Strike(R.Enemy(Near[0])->Position, .01, H.Amount * .5, 2, Options);
			}
		}
	}
	if (H.Element == 2 && S(TEXT("chain.count")) > 0 && !H.Info.bChain && H.Depth < 2)
		Chain(H);
	if (R.Status(H.Id, TEXT("freeze")) > 0 && H.Info.Strike.IsNone())
	{
		const auto *M = R.Enemy(H.Id);
		if (M)
			for (const auto *T : R.T(TEXT("frozen_spikes")))
			{
				FDWProjectileFire F;
				F.Position = M->Position;
				F.Count = int32(T->Param(TEXT("n")));
				F.bRadial = true;
				F.Damage = H.Amount * .4;
				F.Element = 1;
				F.Tags = {TEXT("cold"), TEXT("frost")};
				F.Exclude = H.Id;
				R.Projectiles->Fire(F);
			}
		if (M && H.Amount >= M->MaxHealth * .1 && S(TEXT("freeze.break_damage")) > 0)
		{
			FDWStrikeOptions Options;
			Options.Effect = TEXT("freeze_break");
			R.Hit(H.Id, S(TEXT("freeze.break_damage")), 1, Options);
			RemoveFreeze(H.Id, true);
		}
	}
	if (H.bKilled)
		OnKill(H);
}
void FDWAilments::Chain(const FDWCombatHit &H)
{
	int32 Hops = FMath::FloorToInt(S(TEXT("chain.count")));
	const double Range = 4 * (1 + S(TEXT("chain.distance")) / 100),
				 Decay = .2 * FMath::Max(0., 1 - S(TEXT("chain.decay")) / 100);
	TSet<uint32> HitSet{H.Id};
	uint32 From = H.Id, Previous = H.Id;
	double Amount = H.Amount;
	int32 K = 0;
	while (Hops > 0)
	{
		FVector3d P;
		if (!R.TargetPosition(From, P))
			break;
		const auto Near = R.EnemiesNear(P, Range, S(TEXT("chain.rehit")) ? TSet<uint32>{From, Previous} : HitSet);
		if (!Near.Num())
			break;
		++K;
		--Hops;
		Amount = H.Amount * FMath::Pow(1 - Decay, K);
		FDWStrikeOptions O;
		O.Only = Near[0];
		O.bArea = false;
		O.Effect = TEXT("chain");
		O.Depth = H.Depth + 1;
		O.Info.bChain = true;
		auto &Visual = R.Effects.AddDefaulted_GetRef();
		Visual.Kind = TEXT("chain");
		Visual.Shape = TEXT("beam");
		Visual.Position = P;
		Visual.End = R.Enemy(Near[0])->Position;
		Visual.Element = 2;
		Visual.From = From;
		Visual.To = Near[0];
		R.Strike(R.Enemy(Near[0])->Position, .01, Amount, 2, O);
		HitSet.Add(Near[0]);
		Previous = From;
		From = Near[0];
	}
	if (Hops > 0 && S(TEXT("chain.overflow")))
	{
		FDWStrikeOptions O;
		O.Only = From;
		O.bArea = false;
		O.Effect = TEXT("chain");
		O.Depth = H.Depth + 1;
		O.Info.bChain = true;
		R.Strike(FVector3d::ZeroVector, .01, Amount * .2 * Hops, 2, O);
	}
}
void FDWAilments::BuildFreeze(const FDWCombatHit &H)
{
	const auto *M = R.Enemy(H.Id);
	if (!(S(TEXT("freeze.buildup")) || S(TEXT("freeze.threshold")) || S(TEXT("freeze.elite"))) || !M)
		return;
	const double Efficiency =
		(1 + S(TEXT("freeze.buildup")) / 100) * (M->bElite ? .5 * (1 + S(TEXT("freeze.elite")) / 100) : 1);
	const double Value = FreezeBuild.FindRef(H.Id) + H.Amount / FMath::Max(1., M->MaxHealth) * 100 * Efficiency;
	const double Threshold = 100 * FMath::Max(.1, 1 - S(TEXT("freeze.threshold")) / 100);
	if (Value >= Threshold && R.Status(H.Id, TEXT("freeze")) <= 0)
	{
		FreezeBuild.Add(H.Id, 0);
		R.World.ApplyStatusKey(H.Id, StatusKey(TEXT("freeze")), 1, 1, R.Hero.Owner);
	}
	else
		FreezeBuild.Add(H.Id, FMath::Min(Value, Threshold * 2));
}
void FDWAilments::RemoveFreeze(uint32 Id, bool Consumed)
{
	const bool Existed=R.Status(Id,TEXT("freeze"))>0;
	R.World.ExpireStatus(Id, StatusKey(TEXT("freeze")));
	if (Consumed&&Existed)
		R.Triggers->Ailment(TEXT("consumed"), Id);
}
void FDWAilments::OnKill(const FDWCombatHit &H)
{
	const auto *M = R.Enemy(H.Id);
	if (!M)
		return;
	const FVector3d P = M->Position;
	const int32 N = Layers(H.Id, TEXT("poison"));
	if (N > 0)
	{
		R.Character.Recover(ECharacterPool::Life, S(TEXT("poison.kill_life")));
		R.Character.Recover(ECharacterPool::Mana, S(TEXT("poison.kill_mana")));
		R.Character.Recover(ECharacterPool::Shield, S(TEXT("poison.kill_shield")));
		if (R.Chance(S(TEXT("poison.kill_spread_chance"))))
		{
			int32 Gen = 0;
			for (const auto &L : Poison[H.Id].Layers)
				Gen = FMath::Max(Gen, L.Generation);
			PoisonSpread(H.Id, FMath::CeilToInt(N / 2.), Gen);
		}
		for (const auto *T : R.T(TEXT("poison_death_copy")))
			SpreadLayers(H.Id, TEXT("poison"), FMath::Min(int32(T->Param(TEXT("n"))), N),
						 3 + S(TEXT("poison.spread_radius")), int32(2 + S(TEXT("poison.spread_targets"))), 1);
		for (const auto *T : R.T(TEXT("poison_pool")))
		{
			double Rate = 1;
			for (const auto *T2 : R.T(TEXT("poison_pool_rate")))
				Rate = FMath::Min(Rate, T2->Param(TEXT("seconds")));
			double Avg = 0;
			for (const auto &L : Poison[H.Id].Layers)
				Avg += L.Dps;
			Avg /= N;
			FDWAilmentField F;
			F.Type = TEXT("poison");
			F.Position = P;
			F.Radius = 2 * FMath::Sqrt(1 + S(TEXT("poison.pool_area")) / 100);
			F.Left = T->Param(TEXT("seconds")) * (1 + S(TEXT("poison.pool_duration")) / 100);
			F.Interval = FMath::Max(.1, Rate);
			F.Dps = Avg;
			Fields.Add(F);
		}
	}
	if (IsBurning(H.Id))
	{
		if (R.Chance(S(TEXT("burn.explode_chance"))))
		{
			FDWStrikeOptions O;
			O.Effect = TEXT("fire_explosion");
			O.Depth = 2;
			O.Exclude = {H.Id};
			R.Strike(P, 2.5 * FMath::Sqrt(1 + S(TEXT("burn.explode_area")) / 100) * R.AreaScale(0),
					 R.Hero.Power * (1 + S(TEXT("burn.explode_damage")) / 100) +
						 M->MaxHealth * S(TEXT("burn.explode_life")) / 100 * (M->bElite ? .5 : 1),
					 0, O);
		}
		for (const auto *T : R.T(TEXT("burn_ground")))
		{
			FDWAilmentField F;
			F.Type = TEXT("burn");
			F.Position = P;
			F.Radius = 2;
			F.Left = T->Param(TEXT("seconds")) * (1 + S(TEXT("burn.ground_duration")) / 100);
			F.Dps = R.Hero.Power * .3 * (1 + S(TEXT("burn.ground_damage")) / 100);
			Fields.Add(F);
		}
	}
	if (Layers(H.Id, TEXT("ignite")) && S(TEXT("ignite.kill_spread")))
	{
		const auto List = Ignite[H.Id];
		const auto Near = R.EnemiesNear(P, 3 + S(TEXT("ignite.spread_radius")), {H.Id});
		for (int32 I = 0; I < Near.Num() && I < 2 + S(TEXT("ignite.spread_targets")); ++I)
			for (const auto &L : List)
				AddIgnite(Near[I],
						  L.Dps / FMath::Max(.1, 1 + S(TEXT("ignite.speed")) / 100) /
							  (1 + S(TEXT("ignite.damage")) / 100),
						  4);
	}
	if (R.Status(H.Id, TEXT("shock")) > 0)
	{
		if (R.Chance(S(TEXT("shock.kill_wave_chance"))))
		{
			FDWStrikeOptions O;
			O.Effect = TEXT("shock_wave");
			O.Depth = 2;
			O.Exclude = {H.Id};
			R.Strike(P, 3 * R.AreaScale(2), R.Hero.Power, 2, O);
		}
		for (const auto *T : R.T(TEXT("shock_kill_spread")))
		{
			const auto *List = Shock.Find(H.Id);
			const double Value = List && List->Num() ? (*List)[0].Dps : 20;
			const auto Near = R.EnemiesNear(P, 3 + S(TEXT("shock.spread_radius")), {H.Id});
			for (int32 I = 0; I < Near.Num() && I < T->Param(TEXT("n")); ++I)
				R.World.ApplyStatusKey(Near[I], StatusKey(TEXT("shock")),
									   float(Value / (1 + S(TEXT("shock.effect")) / 100)), 3, R.Hero.Owner);
		}
	}
	if (R.Status(H.Id, TEXT("freeze")) > 0 && S(TEXT("shatter.on_kill")) && !Frozen.FindRef(H.Id))
	{
		FDWStrikeOptions O;
		O.Effect = TEXT("shatter");
		O.Depth = 2;
		O.Exclude = {H.Id};
		O.Tags = {TEXT("cold"), TEXT("frost")};
		const auto Hits = R.Strike(P, 2.5 * FMath::Sqrt(1 + S(TEXT("shatter.area")) / 100) * R.AreaScale(1),
								   M->MaxHealth * .2 * (1 + S(TEXT("shatter.damage")) / 100), 1, O);
		TGuardValue<bool> Guard(bShatterFreeze, true);
		for (const auto &Hit : Hits)
			if (!Hit.bKilled && R.Chance(S(TEXT("shatter.freeze_chance"))))
			{
				R.World.ApplyStatusKey(Hit.Id, StatusKey(TEXT("freeze")), 1, 1, R.Hero.Owner);
				Frozen.Add(Hit.Id, true);
			}
	}
	Forget(H.Id);
}
void FDWAilments::Forget(uint32 Id)
{
	Poison.Remove(Id);
	Erosion.Remove(Id);
	Shock.Remove(Id);
	Ignite.Remove(Id);
	BurnTime.Remove(Id);
	FreezeBuild.Remove(Id);
	Frozen.Remove(Id);
	LastToVoid.Remove(Id);
}
} // namespace DWRules
