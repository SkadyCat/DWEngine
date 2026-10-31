#include "DWRuleAilments.h"
#include "DWRuleCombatTriggers.h"
#include <limits>

namespace DWRules
{
namespace
{
uint64 AilmentKey(FName Name)
{
	return StatusKey(Name.ToString().ToLower());
}
double SumDps(const TArray<FDWAilmentLayer> &List)
{
	double Value = 0;
	for (const auto &L : List)
		Value += L.Dps;
	return Value;
}
double MaxLeft(const TArray<FDWAilmentLayer> &List)
{
	double Value = 0;
	for (const auto &L : List)
		Value = FMath::Max(Value, FMath::IsFinite(L.Left) ? L.Left : 60.);
	return Value;
}
} // namespace
TMap<uint32, FDWLayeredAilment> &FDWAilments::Store(FName Name)
{
	return Name == TEXT("poison") ? Poison : Erosion;
}
const TMap<uint32, FDWLayeredAilment> &FDWAilments::Store(FName Name) const
{
	return Name == TEXT("poison") ? Poison : Erosion;
}
int32 FDWAilments::Layers(uint32 Id, FName Name) const
{
	if (Name == TEXT("shock"))
		return ShockLayers(Id);
	if (Name == TEXT("ignite"))
	{
		const auto *P = Ignite.Find(Id);
		return P ? P->Num() : 0;
	}
	const auto *P = Store(Name).Find(Id);
	return P ? P->Layers.Num() : 0;
}
int32 FDWAilments::ShockLayers(uint32 Id) const
{
	const auto *P = Shock.Find(Id);
	int32 N = 0;
	if (P)
		for (const auto &L : *P)
			N += L.Left > 0;
	return N;
}
bool FDWAilments::IsBurning(uint32 Id) const
{
	if (R.Status(Id, TEXT("ignite")) > 0)
		return true;
	for (const auto &F : Fields)
		if (F.Type == TEXT("burn") && F.Inside.Contains(Id))
			return true;
	return false;
}
double FDWAilments::SlowOf(uint32 Id) const
{
	float Remaining = 0;
	return FMath::Max(0., -double(R.World.GetStatusEffect(Id, EStatusEffect::MoveSpeed, Remaining)));
}
double FDWAilments::TakenMultiplier(uint32 Id, uint8 Element)
{
	double Taken = 1;
	for (const auto *T : R.T(TEXT("poison_no_damage")))
		Taken *= 1 + FMath::Min(60., T->Param(TEXT("n")) * Layers(Id, TEXT("poison"))) / 100;
	if (Element == uint8(EElement::Fire))
		for (const auto *T : R.T(TEXT("burn_timer")))
			Taken *=
				1 + FMath::Min(T->Param(TEXT("max")),
							   FMath::FloorToDouble(BurnTime.FindRef(Id) / FMath::Max(.1, T->Param(TEXT("seconds")))) *
								   T->Param(TEXT("n"))) /
						100;
	return Taken;
}
bool FDWAilments::OnStatus(uint32 Id, uint64 Key, float &Value, float &Duration, int32 Owner)
{
	if (Owner != R.Hero.Owner)
		return true;
	const double Effect = 1 + PendingEffect / 100;
	if (Key == AilmentKey(TEXT("chill")))
	{
		Value = float(Value * (1 + S(TEXT("chill.effect")) / 100) * Effect);
		Duration = float(Duration * (1 + (S(TEXT("chill.duration")) + S(TEXT("frost.duration"))) / 100));
	}
	else if (Key == AilmentKey(TEXT("freeze")))
	{
		Duration = float(Duration * (1 + (S(TEXT("freeze.duration")) + S(TEXT("frost.duration"))) / 100));
		if (!Frozen.Contains(Id))
			Frozen.Add(Id, bShatterFreeze);
		R.Triggers->Ailment(TEXT("freeze"), Id);
	}
	else if (Key == AilmentKey(TEXT("shock")))
	{
		AddShock(Id, Value * Effect, Duration);
		return false;
	}
	else if (Key == AilmentKey(TEXT("ignite")))
	{
		AddIgnite(Id, Value * Effect, Duration);
		return false;
	}
    if(Key==AilmentKey(TEXT("poison"))){FDWAilmentApplication O;O.Amount=Value;O.Element=uint8(EElement::Poison);O.Duration=Duration;O.Effect=PendingEffect;O.bCritical=R.LastCritical.FindRef(Id);ApplyPoison(Id,O);return false;}
	return true;
}
void FDWAilments::OnStatusConsumed(uint32 Id,uint64 Key,int32 Count)
{
    if(Count<=0)return;
    if(Key==AilmentKey(TEXT("shock"))||Key==AilmentKey(TEXT("ignite")))
    {
        auto& Store=Key==AilmentKey(TEXT("shock"))?Shock:Ignite;
        if(auto* List=Store.Find(Id)){List->RemoveAt(0,FMath::Min(Count,List->Num()));if(List->IsEmpty())Store.Remove(Id);}
        if(Key==AilmentKey(TEXT("shock")))SyncShock(Id);else SyncIgnite(Id);
    }
    else if(Key==AilmentKey(TEXT("poison"))||Key==AilmentKey(TEXT("erosion")))
    {
        const FName Name=Key==AilmentKey(TEXT("poison"))?TEXT("poison"):TEXT("erosion");
        if(auto* State=Store(Name).Find(Id))State->Layers.RemoveAt(0,FMath::Min(Count,State->Layers.Num()));
        SyncLayers(Id,Name);
    }
    else if(Key==AilmentKey(TEXT("freeze")))Frozen.Remove(Id);
    if(R.Enemy(Id))for(const FName Name:{FName(TEXT("chill")),FName(TEXT("freeze")),FName(TEXT("shock")),FName(TEXT("ignite")),FName(TEXT("poison")),FName(TEXT("erosion")),FName(TEXT("bleed")),FName(TEXT("stun"))})
        if(Key==AilmentKey(Name)){R.Triggers->Ailment(TEXT("consumed"),Id);break;}
}
void FDWAilments::AddShock(uint32 Id, double Value, double Duration)
{
	Value *= 1 + (S(TEXT("shock.effect")) + (R.LastCritical.FindRef(Id) ? S(TEXT("shock.crit_effect")) : 0)) / 100;
	Duration *= 1 + S(TEXT("shock.duration")) / 100;
	const double Max = 1 + FMath::Max(0., S(TEXT("shock.max_stacks")));
	auto &List = Shock.FindOrAdd(Id);
	List.RemoveAll([](const auto &L) { return L.Left <= 0; });
	const double Adding = 1 + FMath::Max(0., S(TEXT("shock.extra_layers")));
	for (int32 I = 0; I < Adding; ++I)
	{
		FDWAilmentLayer L;
		L.Dps = Value;
		L.Left = Duration;
		if (List.Num() < Max)
			List.Add(L);
		else
		{
			List.StableSort(
				[](const auto &A, const auto &B) { return A.Dps != B.Dps ? A.Dps < B.Dps : A.Left < B.Left; });
			if (List[0].Dps <= Value)
				List[0] = L;
		}
	}
	SyncShock(Id);
	R.Triggers->Ailment(TEXT("shock"), Id, List.Num());
	ShockApplied += int32(Adding);
	for (auto *T : R.T(TEXT("shock_layers_strike")))
	{
		T->Accumulator += Adding;
		while (T->Accumulator >= FMath::Max(1., T->Param(TEXT("per"))))
		{
			T->Accumulator -= FMath::Max(1., T->Param(TEXT("per")));
			FVector3d P;
			if (R.TargetPosition(Id, P))
			{
				FDWStrikeOptions O;
				O.Effect = TEXT("lightning_strike");
				O.Depth = 2;
				R.Strike(P, 1.5 * R.AreaScale(2), R.Hero.Power * 2, 2, O);
			}
		}
	}
}
void FDWAilments::SyncShock(uint32 Id)
{
	const auto *List = Shock.Find(Id);
	const int32 N = List ? List->Num() : 0;
	R.World.MirrorStatus(Id, AilmentKey(TEXT("shock")), N ? float(SumDps(*List) / N) : 0, N,
						 List ? float(MaxLeft(*List)) : 0, R.Hero.Owner);
}
void FDWAilments::ConsumeShock(uint32 Id)
{
	if (!ShockLayers(Id))
		return;
	Shock.Remove(Id);
	SyncShock(Id);
	R.Triggers->Ailment(TEXT("consumed"), Id);
}
void FDWAilments::AddIgnite(uint32 Id, double Dps, double Duration)
{
	const double Speed = FMath::Max(.1, 1 + S(TEXT("ignite.speed")) / 100);
	Dps = (Dps + S(TEXT("ignite.base"))) *
		  (1 + (S(TEXT("ignite.damage")) + (R.LastCritical.FindRef(Id) ? S(TEXT("ignite.crit")) : 0)) / 100) * Speed;
	Duration = Duration * (1 + S(TEXT("ignite.duration")) / 100) / Speed;
	auto &List = Ignite.FindOrAdd(Id);
	List.RemoveAll([](const auto &L) { return L.Left <= 0; });
	FDWAilmentLayer L;
	L.Dps = Dps;
	L.Left = Duration;
	if (List.Num() < 1 + FMath::Max(0., S(TEXT("ignite.max_stacks"))))
		List.Add(L);
	else
	{
		List.StableSort([](const auto &A, const auto &B) { return A.Dps < B.Dps; });
		if (List[0].Dps <= Dps)
			List[0] = L;
	}
	SyncIgnite(Id);
	R.Triggers->Ailment(TEXT("ignite"), Id, List.Num());
}
void FDWAilments::SyncIgnite(uint32 Id)
{
	const auto *List = Ignite.Find(Id);
	const int32 N = List ? List->Num() : 0;
	R.World.MirrorStatus(Id, AilmentKey(TEXT("ignite")), N ? float(SumDps(*List) / N) : 0, N,
						 List ? float(MaxLeft(*List)) : 0, R.Hero.Owner);
}
double FDWAilments::IgniteRemaining(uint32 Id) const
{
	double N = 0;
	if (const auto *P = Ignite.Find(Id))
		for (const auto &L : *P)
			N += L.Dps * FMath::Max(0., L.Left);
	return N;
}
int32 FDWAilments::RemoveLayers(uint32 Id, FName Name, int32 Count)
{
	auto *St = Store(Name).Find(Id);
	if (!St || !St->Layers.Num())
		return 0;
	St->Layers.StableSort([](const auto &A, const auto &B) { return A.Left < B.Left; });
	const int32 Removed = FMath::Clamp(Count, 0, St->Layers.Num());
	St->Layers.RemoveAt(0, Removed);
	SyncLayers(Id, Name);
	if (Removed)
		R.Triggers->Ailment(TEXT("consumed"), Id);
	return Removed;
}
double FDWAilments::Remaining(uint32 Id, FName Name) const
{
	double N = 0;
	if (const auto *P = Store(Name).Find(Id))
		for (const auto &L : P->Layers)
			N += L.Dps * (FMath::IsFinite(L.Left) ? FMath::Max(0., L.Left) : 4);
	return N;
}
void FDWAilments::SyncLayers(uint32 Id, FName Name)
{
	auto *St = Store(Name).Find(Id);
	const int32 N = St ? St->Layers.Num() : 0;
	R.World.MirrorStatus(Id, AilmentKey(Name), 1, N, St ? float(MaxLeft(St->Layers)) : 0, R.Hero.Owner);
	if (St && !N)
		Store(Name).Remove(Id);
}
void FDWAilments::ApplyPoison(uint32 Id, const FDWAilmentApplication &O)
{
	if (!R.Enemy(Id))
		return;
	auto St = Poison.FindRef(Id);
	const int32 Already = St.Layers.Num();
	const double Share = O.Element == 3	  ? S(TEXT("poison.from_physical"))
						 : O.Element == 5 ? S(TEXT("poison.from_void"))
						 : O.Element <= 2 ? S(TEXT("poison.from_elemental"))
										  : 0;
	double Inc = S(TEXT("poison.damage")) + (O.bCritical ? S(TEXT("poison.crit_damage")) : 0) + O.Effect;
	const double Life = R.LifeFraction(Id);
	if (Life >= .999)
		Inc += S(TEXT("poison.vs_full"));
	if (Life <= .35)
		Inc += S(TEXT("poison.vs_low"));
	if (R.Status(Id, TEXT("bleed")) > 0)
		Inc += S(TEXT("poison.vs_bleed"));
	if (R.Status(Id, TEXT("shock")) > 0)
		Inc += S(TEXT("poison.vs_shock"));
	for (const auto *T : R.T(TEXT("poison_per_layer")))
		Inc += FMath::Min(T->Param(TEXT("max")), T->Param(TEXT("n")) * Already);
	if (O.bSpread)
		Inc += S(TEXT("poison.spread_damage"));
	if (O.bMinion)
		Inc += S(TEXT("minion.dot"));
	double More = S(TEXT("poison.more"));
	for (const auto *T : R.T(TEXT("poison_threshold_more")))
		if (Already >= T->Param(TEXT("stacks")))
			More *= 1 + T->Param(TEXT("n")) / 100;
	if (S(TEXT("poison.single_layer")))
		More *= 1 + S(TEXT("poison.single_layer")) / 100;
	if (S(TEXT("poison.unlimited")))
		More *= FMath::Max(0., 1 - S(TEXT("poison.unlimited")) / 100);
	double Speed = 1 + S(TEXT("poison.speed")) / 100;
	double DurationScale = 1 + S(TEXT("poison.duration")) / 100 +
						   (R.Status(Id, TEXT("chill")) > 0 ? S(TEXT("poison.vs_chill_duration")) / 100 : 0) +
						   (O.bSpread ? S(TEXT("poison.spread_duration")) / 100 : 0);
	for (const auto *T : R.T(TEXT("poison_threshold_duration")))
		if (Already >= T->Param(TEXT("stacks")))
			DurationScale += T->Param(TEXT("n")) / 100;
	for (const auto *T : R.T(TEXT("poison_quick")))
	{
		DurationScale *= FMath::Max(.05, 1 - T->Param(TEXT("duration")) / 100);
		Speed *= 1 + T->Param(TEXT("speed")) / 100;
	}
	Speed = FMath::Max(.1, Speed);
	const double Dps =
		FMath::Max(0., (S(TEXT("poison.base")) + O.Amount * (20 + Share) / 100) * (1 + Inc / 100) * More) * Speed;
	const auto Eternal = R.T(TEXT("poison_eternal"));
	const double Duration =
		Eternal.Num() ? std::numeric_limits<double>::infinity() : 2 * FMath::Max(.05, DurationScale) / Speed;
	if (!(Dps > 0))
		return;
	double Count = O.Layers != INDEX_NONE
					   ? O.Layers
					   : 1 + S(TEXT("poison.extra_layers")) + (Already > 0 ? S(TEXT("poison.extra_on_poisoned")) : 0);
	if (!O.bSpread && R.Chance(S(TEXT("poison.double_chance"))))
		Count *= 2;
	if (O.bSpread)
		Count += S(TEXT("poison.spread_layers"));
	double EternalCap = 200;
	for (const auto *T : Eternal)
		EternalCap = FMath::Min(EternalCap, T->Param(TEXT("n")));
	const double Max = S(TEXT("poison.single_layer")) ? 1
					   : Eternal.Num()				  ? EternalCap
					   : S(TEXT("poison.unlimited"))  ? 200
													  : 10 + S(TEXT("poison.max_stacks"));
	if (S(TEXT("poison.refresh")))
		for (auto &L : St.Layers)
			L.Left = FMath::Max(L.Left, Duration);
	if (S(TEXT("poison.extend")) > 0)
		for (auto &L : St.Layers)
			L.Left = FMath::Min(L.Left + S(TEXT("poison.extend")), 6 / Speed);
	if (S(TEXT("poison.merge")) && St.Layers.Num())
	{
		double Total = Dps * Duration * Count;
		for (const auto &L : St.Layers)
			Total += L.Dps * (FMath::IsFinite(L.Left) ? L.Left : Duration);
		St.Layers = {{Total / Duration, Duration, Total, 0, O.Generation}};
	}
	else
		for (int32 I = 0; I < Count; ++I)
		{
			FDWAilmentLayer L{Dps, Duration, Dps * (FMath::IsFinite(Duration) ? Duration : 4), 0, O.Generation};
			if (St.Layers.Num() < Max)
			{
				St.Layers.Add(L);
				continue;
			}
			if (S(TEXT("poison.single_layer")))
			{
				if (St.Layers[0].Dps <= Dps)
					St.Layers[0] = L;
				continue;
			}
			int32 Index = 0;
			for (int32 K = 1; K < St.Layers.Num(); ++K)
				if (S(TEXT("poison.replace_lowest")) ? St.Layers[K].Dps < St.Layers[Index].Dps
													 : St.Layers[K].Left < St.Layers[Index].Left)
					Index = K;
			St.Layers[Index] = L;
		}
	Poison.Add(Id, MoveTemp(St));
	SyncLayers(Id, TEXT("poison"));
	if (S(TEXT("poison.self")) && R.Character.SelfPoison.Num() < 10)
		R.Character.SelfPoison.Add(2);
	PoisonApplied += int32(Count);
	for (auto *T : R.T(TEXT("poison_layers_empower")))
	{
		T->Accumulator += Count;
		const double Per = FMath::Max(1., T->Param(TEXT("per")));
		while (T->Accumulator >= Per)
		{
			T->Accumulator -= Per;
			R.Character.PendingEmpower += T->Param(TEXT("n"));
		}
	}
	R.Triggers->Ailment(TEXT("poison"), Id, Layers(Id, TEXT("poison")));
	for (const auto *T : R.T(TEXT("poison_threshold_detonate")))
		if (Layers(Id, TEXT("poison")) >= T->Param(TEXT("stacks")))
			Detonate(Id);
}
void FDWAilments::ApplyErosion(uint32 Id, const FDWAilmentApplication &O)
{
	auto St = Erosion.FindRef(Id);
	const double Dps = (S(TEXT("erosion.base")) + O.Amount * .15) *
					   (1 + (S(TEXT("erosion.damage")) + O.Effect + (O.bMinion ? S(TEXT("minion.dot")) : 0)) / 100);
	const double Duration = 3 * (1 + S(TEXT("erosion.duration")) / 100);
	if (!(Dps > 0))
		return;
	const double Count = O.Layers != INDEX_NONE ? O.Layers : 1 + S(TEXT("erosion.extra_layers"));
	const double Max = 5 + S(TEXT("erosion.max_stacks"));
	for (int32 I = 0; I < Count; ++I)
	{
		FDWAilmentLayer L{Dps, Duration, Dps * Duration, 0, 0};
		if (St.Layers.Num() < Max)
			St.Layers.Add(L);
		else
		{
			int32 Index = 0;
			for (int32 K = 1; K < St.Layers.Num(); ++K)
				if (St.Layers[K].Left < St.Layers[Index].Left)
					Index = K;
			St.Layers[Index] = L;
		}
	}
	Erosion.Add(Id, MoveTemp(St));
	SyncLayers(Id, TEXT("erosion"));
	R.Triggers->Ailment(TEXT("erosion"), Id, Layers(Id, TEXT("erosion")));
	for (const auto *T : R.T(TEXT("erosion_collapse")))
		if (Layers(Id, TEXT("erosion")) >= T->Param(TEXT("stacks")))
			Collapse(Id, false);
}
void FDWAilments::Collapse(uint32 Id, bool FromExecute)
{
	FVector3d P;
	if (!R.TargetPosition(Id, P))
		return;
	if (!FromExecute)
		RemoveLayers(Id, TEXT("erosion"));
	FDWStrikeOptions O;
	O.Effect = TEXT("void_collapse");
	O.Depth = 2;
	R.Strike(P, 2.5 * FMath::Sqrt(1 + S(TEXT("collapse.area")) / 100) * R.AreaScale(5),
			 R.Hero.Power * 1.5 * (1 + S(TEXT("collapse.damage")) / 100), 5, O);
}
void FDWAilments::Detonate(uint32 Id, int32 Count, bool Force)
{
	auto *Original = Poison.Find(Id);
	if (!Original || !Original->Layers.Num() || bDetonating || (!Force && Original->Cooldown > 0))
		return;
	TGuardValue<bool> Guard(bDetonating, true);
	auto St = *Original;
	St.Cooldown = 1 / FMath::Max(.1, 1 + S(TEXT("poison.detonate_cooldown")) / 100);
	St.Layers.StableSort([](const auto &A, const auto &B) { return A.Dps * A.Left > B.Dps * B.Left; });
	const int32 N = FMath::Clamp(Count, 0, St.Layers.Num());
	double Rest = 0;
	for (int32 I = 0; I < N; ++I)
		Rest += St.Layers[I].Dps * (FMath::IsFinite(St.Layers[I].Left) ? St.Layers[I].Left : 4);
    const bool Kept=R.Chance(S(TEXT("poison.detonate_keep_chance")));
	if (!Kept)
		St.Layers.RemoveAt(0, N);
	Poison.Add(Id, MoveTemp(St));
	SyncLayers(Id, TEXT("poison"));
	FVector3d P;
	if (!R.TargetPosition(Id, P))
		return;
	const double Burning = IsBurning(Id) ? S(TEXT("poison.detonate_vs_burning")) : 0;
	const double Amount = Rest * (60 + S(TEXT("poison.detonate_ratio"))) / 100 *
						  (1 + (S(TEXT("poison.detonate_damage")) + Burning) / 100);
	FDWStrikeOptions O;
	O.Effect = TEXT("poison_detonate");
	O.Depth = 2;
	O.Tags = {TEXT("poison")};
	const auto Hits =
		R.Strike(P, 2 * FMath::Sqrt(1 + S(TEXT("poison.detonate_area")) / 100) * R.AreaScale(), Amount, 4, O);
    if(N>0&&!Kept)R.Triggers->Ailment(TEXT("consumed"), Id);
	for (const auto &H : Hits)
		if (H.bKilled)
			for (const auto *T : R.T(TEXT("poison_detonate_spread")))
				SpreadLayers(H.Id, TEXT("poison"), int32(T->Param(TEXT("n"))), 3);
}
void FDWAilments::SpreadLayers(uint32 Id, FName Name, int32 Count, double Radius, int32 Targets, int32 Generation,
							   const FVector3d *Origin, double Average)
{
	FVector3d P;
	if (Origin)
		P = *Origin;
	else if (!R.TargetPosition(Id, P))
		return;
	const auto *Source = Store(Name).Find(Id);
	if (Average < 0)
		Average = Source && Source->Layers.Num() ? SumDps(Source->Layers) / Source->Layers.Num() : R.Hero.Power * .2;
	const auto Nearby = R.EnemiesNear(P, Radius, {Id});
	for (int32 I = 0; I < FMath::Min(Targets, Nearby.Num()); ++I)
	{
		FDWAilmentApplication O;
		O.Layers = Count;
		O.Generation = Generation;
		O.bSpread = Name == TEXT("poison");
		O.Amount = Name == TEXT("poison") ? Average * 5 : Average / .15;
		if (Name == TEXT("poison"))
			ApplyPoison(Nearby[I], O);
		else
			ApplyErosion(Nearby[I], O);
	}
}
void FDWAilments::PoisonSpread(uint32 Id, int32 Count, int32 Generation)
{
	if (Generation > S(TEXT("poison.spread_times")))
		return;
	FVector3d P;
	if (!R.TargetPosition(Id, P))
		return;
	const auto *St = Poison.Find(Id);
	const double Average = St && St->Layers.Num() ? SumDps(St->Layers) / St->Layers.Num() : R.Hero.Power * .2;
	Scheduled.Add({R.Time + .3 * FMath::Max(0., 1 - S(TEXT("poison.spread_interval")) / 100), Average, P, Id,
				   FMath::Max(1, Count), Generation + 1});
}
} // namespace DWRules
