#include "DWRuleAilments.h"

namespace DWRules
{
void FDWAilments::Tick(double Dt)
{
	const auto Jobs = Scheduled.FilterByPredicate([&](const auto &J) { return J.At <= R.Time; });
	for (const auto &J : Jobs)
		SpreadLayers(J.Id, TEXT("poison"), J.Count, 3 + S(TEXT("poison.spread_radius")),
					 int32(2 + S(TEXT("poison.spread_targets"))), J.Generation, &J.Origin, J.Average);
	Scheduled.RemoveAll([&](const auto &J) { return J.At <= R.Time; });
	for (FName Name : {FName(TEXT("poison")), FName(TEXT("erosion"))})
	{
		auto &Map = Store(Name);
		TArray<uint32> Ids;
		Map.GetKeys(Ids);
		const uint8 Element = Name == TEXT("poison") ? 4 : 5;
		const double Interval =
			Name == TEXT("poison") ? .5 / FMath::Max(.1, 1 + S(TEXT("poison.frequency")) / 100) : .5;
		for (uint32 Id : Ids)
		{
			if (!R.Enemy(Id))
			{
				Map.Remove(Id);
				continue;
			}
			auto St = Map[Id];
			St.Cooldown = FMath::Max(0., St.Cooldown - Dt);
			St.Tick += Dt;
			const bool Silent =
				Name == TEXT("poison") && (S(TEXT("poison.burst_end")) || R.Has(TEXT("poison_no_damage")));
			while (St.Tick >= Interval)
			{
				St.Tick -= Interval;
				double Amount = 0;
				for (const auto &L : St.Layers)
					if (L.Left > 0)
						Amount += L.Dps;
				Amount *= Interval;
				if (!Silent && Amount > 0)
					R.World.HostDot(R.Hero.Owner, Id, float(Amount), Element);
			}
			for (auto &L : St.Layers)
				L.Left -= Dt;
			const auto Ended = St.Layers.FilterByPredicate([](const auto &L) { return L.Left <= 0; });
			St.Layers.RemoveAll([](const auto &L) { return L.Left <= 0; });
			for (auto L : Ended)
				if (Name == TEXT("poison"))
				{
					if (L.Keeps < 3 && R.Chance(S(TEXT("poison.keep_chance"))))
					{
						++L.Keeps;
						L.Left = L.Total / FMath::Max(1.e-9, L.Dps);
						St.Layers.Add(L);
						continue;
					}
					if (S(TEXT("poison.burst_end")))
						R.World.HostDot(R.Hero.Owner, Id, float(L.Total), Element);
					if (S(TEXT("poison.end_burst")) > 0)
						R.World.HostDot(R.Hero.Owner, Id, float(L.Total * S(TEXT("poison.end_burst")) / 100), Element);
				}
			Map.Add(Id, MoveTemp(St));
			if (Ended.Num())
				SyncLayers(Id, Name);
		}
	}
	for (int32 Which = 0; Which < 2; ++Which)
	{
		auto &Map = Which == 0 ? Shock : Ignite;
		TArray<uint32> Ids;
		Map.GetKeys(Ids);
		for (uint32 Id : Ids)
		{
			auto &List = Map[Id];
			for (auto &L : List)
				L.Left -= Dt;
			const int32 Removed = List.RemoveAll([](const auto &L) { return L.Left <= 0; });
			if (Removed)
			{
				if (Which == 0)
					SyncShock(Id);
				else
					SyncIgnite(Id);
			}
			if (!List.Num())
				Map.Remove(Id);
		}
	}
	TArray<uint32> FrozenIds;
	Frozen.GetKeys(FrozenIds);
	for (uint32 Id : FrozenIds)
	{
		if (R.Status(Id, TEXT("freeze")) > 0)
			continue;
		Frozen.Remove(Id);
		FVector3d P;
		if (!R.TargetPosition(Id, P) || !R.Enemy(Id))
			continue;
		if (S(TEXT("freeze.residual_chill")) > 0)
			R.World.ApplyStatusKey(Id, StatusKey(TEXT("chill")), 30, float(S(TEXT("freeze.residual_chill"))),
								   R.Hero.Owner);
		for (const auto *T : R.T(TEXT("freeze_end_zone")))
		{
			FDWAilmentField F;
			F.Type = TEXT("chill");
			F.Position = P;
			F.Radius = 2;
			F.Left = T->Param(TEXT("seconds"));
			Fields.Add(F);
		}
	}
	for (auto &F : Fields)
	{
		F.Left -= Dt;
		F.Tick += Dt;
		F.Inside.Reset();
		for (uint32 Id : R.EnemiesNear(F.Position, F.Radius))
			F.Inside.Add(Id);
		if (F.Tick < F.Interval)
			continue;
		F.Tick -= F.Interval;
		for (uint32 Id : F.Inside)
		{
			if (F.Type == TEXT("poison"))
			{
				FDWAilmentApplication O;
				O.Amount = F.Dps * 5;
				O.Layers = 1;
				ApplyPoison(Id, O);
			}
			else if (F.Type == TEXT("burn"))
				R.World.HostDot(R.Hero.Owner, Id, float(F.Dps * F.Interval), 0);
			else
				R.World.ApplyStatusKey(Id, StatusKey(TEXT("chill")), 30, 1, R.Hero.Owner);
		}
	}
	Fields.RemoveAll([](const auto &F) { return F.Left <= 0; });
	for (const auto &M : R.Enemies)
	{
		if (IsBurning(M.Id))
			BurnTime.FindOrAdd(M.Id) += Dt;
		else
			BurnTime.Remove(M.Id);
	}
}
} // namespace DWRules
