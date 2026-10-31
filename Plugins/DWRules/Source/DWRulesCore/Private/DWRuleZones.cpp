#include "DWRuleZones.h"

namespace DWRules
{
namespace
{
double Wrap(double A)
{
	while (A > UE_DOUBLE_PI)
		A -= 2 * UE_DOUBLE_PI;
	while (A < -UE_DOUBLE_PI)
		A += 2 * UE_DOUBLE_PI;
	return A;
}
double ZoneDistance(const FVector3d &A, const FVector3d &B)
{
	return FVector2d::Distance({A.X, A.Y}, {B.X, B.Y});
}
} // namespace
bool FDWZones::OnArea(const FDWAreaRequest &Q)
{
	if (!R.World.IsRootValid(Q.RootIndex))
		return false;
	const auto &Root = R.World.GetRoot(Q.RootIndex);
	if (Root.Owner != R.Hero.Owner)
		return false;
	const auto &Plan = R.World.GetPlan(Root.Plan);
	const auto *RI = R.RootInfo(Q.RootIndex);
	const bool Minion = (RI && RI->bMinion) || R.HasTag(Plan, Q.Tags, TEXT("summon")), Triggered = RI && RI->bTriggered;
	const bool Primary =
		Q.Event->Type == uint16(EBuiltinEvent::Cast) && !Minion && (Triggered || (RI && RI->Kind == TEXT("cast")));
	const bool Own = Plan.TraitIds.Contains(TEXT("__combat_plan"));
	const double Scale = R.AreaScale(Q.Element, Triggered, Minion, Primary) / (Own ? R.AreaScale() : 1);
	const double Radius = Q.Radius * Scale + (Own ? 0 : S(TEXT("area.base_radius")));
	if (!Primary)
	{
		if (FMath::Abs(Scale - 1) < 1.e-6 && Own)
			return false;
		Instant(Q, Radius);
		return true;
	}
	Cast(Q, Radius, Triggered, Own);
	return true;
}
void FDWZones::Instant(const FDWAreaRequest &Q, double Radius)
{
	FString Effect = Q.Trait.ToString();
	int32 Suffix;
	if (Effect.FindChar('~', Suffix))
		Effect.LeftInline(Suffix);
	R.Effects.Add({FName(*Effect), FVector3d(Q.Event->Position), Radius, Q.Amount, Q.Element});
	FDWDamageInfo Info;
	Info.Tags = Q.Tags;
	Info.Position = Info.Center = Q.Event->Position;
	Info.Radius = float(Radius);
	Info.bHasCenter = true;
	Info.Kind = TEXT("host");
	for (uint32 Id : R.EnemiesNear(FVector3d(Q.Event->Position), Radius))
		R.World.HostHit(Q.RootIndex, Id, Q.Amount, Q.Element, Info, false, 1, Q.EventType, Q.Base);
}
void FDWZones::Cast(const FDWAreaRequest &Q, double Radius, bool Triggered, bool Own)
{
	const auto &Plan = R.World.GetPlan(R.World.GetRoot(Q.RootIndex).Plan);
	const FVector3d Hero = R.HeroPosition();
	const auto *Owner = R.World.GetOwner(R.Hero.Owner);
	const FVector3d Aim = Owner ? FVector3d(Owner->Aim) : FVector3d(1, 0, 0);
	const double Dir = FMath::Atan2(Aim.Y, Aim.X);
	const bool Melee = Plan.TagNames.Contains(TEXT("melee")), Line = Plan.TagNames.Contains(TEXT("line"));
	const double CastRange = 6 * (1 + S(TEXT("area.cast_range")) / 100),
				 Seek = 8 * (1 + S(TEXT("area.seek_range")) / 100);
	FVector3d P(Q.Event->Position);
	const auto Near = R.EnemiesNear(Hero, Seek);
	const auto *Nearest = Near.Num() ? R.Enemy(Near[0]) : nullptr;
	if (S(TEXT("area.at_self")))
		P = Hero;
	else if (S(TEXT("area.at_target")) && Nearest)
		P = Nearest->Position;
	else if (S(TEXT("area.at_point")) || (!Melee && !Line))
	{
		P = Hero + Aim * CastRange;
		const FDWCombatEnemy *Pick = nullptr;
		double Best = 1.e30;
		if (!S(TEXT("area.at_point")))
			for (uint32 Id : Near)
			{
				const auto *M = R.Enemy(Id);
				const double Off = FMath::Abs(Wrap(FMath::Atan2(M->Position.Y - Hero.Y, M->Position.X - Hero.X) - Dir));
				if (Off <= UE_DOUBLE_PI / 3 && Off < Best)
				{
					Pick = M;
					Best = Off;
				}
			}
		if (Pick)
		{
			FVector3d Delta = Pick->Position - Hero;
			Delta.Z = 0;
			const double D = Delta.Size();
			P = Hero + Delta / (D ? D : 1) * FMath::Min(D, CastRange * 1.5);
		}
	}
	if (Line)
		P = Hero;
	if (Triggered && !Line)
		P = FVector3d(Q.Event->Position);
	FDWZoneTemplate Z;
	Z.Root = Q.RootIndex;
	Z.Base = *Q.Base;
	Z.Tags = Q.Tags;
	Z.EventType = Q.EventType;
	Z.Generation = Q.Base->Generation;
	Z.Element = Q.Element;
	Z.Amount = Q.Amount;
	Z.Shape = Line ? TEXT("line") : TEXT("circle");
	if (S(TEXT("area.follow_self")))
		Z.bFollowSelf = true;
	else if (S(TEXT("area.follow_target")) && Nearest)
		Z.FollowTarget = Nearest->Id;
	if (Z.Shape == TEXT("circle") && S(TEXT("area.to_ring")))
		Z.Shape = TEXT("ring");
	if (Z.Shape == TEXT("circle") && S(TEXT("area.to_sector")))
		Z.Shape = TEXT("sector");
	if (Z.Shape == TEXT("sector") && !Z.bFollowSelf && Z.FollowTarget == InvalidId)
		P = Hero;
	double Duration = 1 + S(TEXT("area.duration")) / 100 + (Q.Element == 1 ? S(TEXT("frost.duration")) / 100 : 0) +
					  (Triggered ? S(TEXT("trigger.duration")) / 100 : 0);
	Z.bPersistent =
		Z.bFollowSelf || Z.FollowTarget != InvalidId || S(TEXT("area.move_forward")) || S(TEXT("area.pulse"));
    double BaseLife=2;const auto Roamers=R.T(TEXT("area_roam"));
    if(Roamers.Num())
    {
        Z.bPersistent=true;Z.RoamInterval=1.e30;BaseLife=.05;
        for(const auto* T:Roamers){Z.RoamSpeed=FMath::Max(Z.RoamSpeed,T->Param(TEXT("speed")));Z.RoamRange=FMath::Max(Z.RoamRange,T->Param(TEXT("range")));Z.RoamInterval=FMath::Min(Z.RoamInterval,T->Param(TEXT("interval")));BaseLife=FMath::Max(BaseLife,T->Param(TEXT("duration")));}
        Z.RoamSpeed*=FMath::Max(0.,1+S(TEXT("area.move_speed"))/100);Z.RoamRange=FMath::Max(.1,Z.RoamRange);Z.RoamInterval=FMath::Max(.05,Z.RoamInterval);
    }
	FString Effect = Q.Trait.ToString();
	int32 Suffix;
	if (Effect.FindChar('~', Suffix))
		Effect.LeftInline(Suffix);
	Z.Effect = FName(*Effect);
	Z.Radius = FMath::Max(.2, Radius);
	Z.Inner = FMath::Max(0., Radius * .5 - S(TEXT("area.ring_inner")));
	Z.Outer = Radius + S(TEXT("area.ring_outer"));
	Z.Angle = (90 + S(TEXT("area.sector_angle"))) * UE_DOUBLE_PI / 180;
	Z.SectorRadius = Radius * 2.5 + S(TEXT("area.sector_radius"));
	Z.Length = 7 * (Radius / FMath::Max(.1, Q.Radius != 0 ? double(Q.Radius) : Radius)) + S(TEXT("area.line_length"));
	Z.Width = 1.2 + S(TEXT("area.line_width"));
	Z.Direction = Dir;
	Z.bPrimary = true;
	Z.Delay = Melee || Line ? 0 : .25 * FMath::Max(0., 1 - S(TEXT("area.delay")) / 100);
	Z.Life = Z.bPersistent ? BaseLife * Duration : 0;
	Z.bOwnPlan = Own;
	struct FCenter
	{
		FVector3d P;
		double Delay, Dir;
	};
	TArray<FCenter> Centers{{P, 0, Dir}};
	const double Step = Radius * 1.5, Reach = FMath::Max(1., ZoneDistance(P, Hero));
	if (S(TEXT("area.multi_direction")))
		for (int32 K = 1; K < 4; ++K)
		{
			const double A = Dir + K * UE_DOUBLE_PI / 2;
			Centers.Add({Hero + FVector3d(FMath::Cos(A), FMath::Sin(A), 0) * Reach, 0, A});
		}
	if (S(TEXT("area.sequence_line")))
		for (int32 K = 1; K < 4; ++K)
			Centers.Add({P + Aim * Step * K, .15 * K, Dir});
	const int32 Extra = FMath::FloorToInt(S(TEXT("area.extra_zones")));
	const double Distance = Radius * 1.5 * (1 + S(TEXT("area.extra_distance")) / 100),
				 Jitter = FMath::Max(0., 1 - S(TEXT("area.extra_spread")) / 100);
	for (int32 K = 0; K < Extra; ++K)
	{
		const double A = double(K) / FMath::Max(1, Extra) * 2 * UE_DOUBLE_PI +
						 (R.Random() - .5) * Jitter * (2 * UE_DOUBLE_PI / FMath::Max(1, Extra));
		Centers.Add({P + FVector3d(FMath::Cos(A), FMath::Sin(A), 0) * Distance, 0, Dir});
	}
	const int32 Sets = R.Chance(S(TEXT("area.extra_cast_chance"))) ? 2 : 1;
	for (int32 Set = 0; Set < Sets; ++Set)
		for (const auto &Center : Centers)
		{
			FDWZoneTemplate Child = Z;
			Child.Delay += Center.Delay + Set * .3;
			if (S(TEXT("area.ring_around_target")))
				for (int32 K = 0; K < 6; ++K)
				{
					const double A = K / 6. * 2 * UE_DOUBLE_PI;
					Child.Radius = Z.Radius * .5;
					Child.Position = Center.P + FVector3d(FMath::Cos(A), FMath::Sin(A), 0) * Radius;
					Spawn(Child);
				}
			else
			{
				Child.Position = Center.P;
				Child.Direction = Center.Dir;
				Spawn(Child);
			}
		}
}
FDWZone *FDWZones::Spawn(const FDWZoneTemplate &Template)
{
	if (!R.World.IsRootValid(Template.Root))
		return nullptr;
	++R.World.Roots[Template.Root].Live;
	auto Z = MakeUnique<FDWZone>();
	static_cast<FDWZoneTemplate &>(*Z) = Template;
	Z->Id = NextId++;
	Z->RepeatsLeft = Template.bRepeat ? FMath::FloorToInt(S(TEXT("area.repeats"))) : 0;
	Z->SpreadsLeft = Template.bSpread ? FMath::FloorToInt(S(TEXT("area.spreads"))) : 0;
	Z->SpreadRadius = Template.Radius;
	Z->NextAt = Template.Delay;
	Z->Interval = .5 / FMath::Max(.1, 1 + S(TEXT("area.tick_rate")) / 100);
	Z->MaxRadius = Z->Radius * (S(TEXT("area.pulse")) ? FMath::Sqrt(1 + S(TEXT("area.expand_max")) / 100) : 1);
    Z->bRoaming=Z->RoamRange>0&&Z->bPersistent&&Z->bPrimary&&!Z->bGround&&!Z->bSecondary;
    if(Z->bRoaming){Z->RoamOrigin=Z->Position;Z->RoamTurnLeft=Z->RoamInterval;Z->RoamSeed=uint32(R.Random()*4294967296.);if(!Z->RoamSeed)Z->RoamSeed=1;}
	auto *Out = Z.Get();
	Zones.Add(MoveTemp(Z));
	return Out;
}
TArray<TPair<uint32, double>> FDWZones::Victims(const FDWZone &Z, double Radius) const
{
	if (Radius < 0)
		Radius = Z.Radius;
	const double Reach = Z.Shape == TEXT("line")	 ? Z.Length
						 : Z.Shape == TEXT("ring")	 ? Z.Outer * Radius / Z.Radius
						 : Z.Shape == TEXT("sector") ? Z.SectorRadius * Radius / Z.Radius
													 : Radius;
	TArray<TPair<uint32, double>> Out;
	for (uint32 Id : R.EnemiesNear(Z.Position, Reach + .6))
	{
		const auto *M = R.Enemy(Id);
		const double X = M->Position.X - Z.Position.X, Y = M->Position.Y - Z.Position.Y, D = FMath::Sqrt(X * X + Y * Y),
					 MR = M->Radius;
		if (Z.Shape == TEXT("circle") && D > Radius + MR)
			continue;
		if (Z.Shape == TEXT("ring") && (D > Reach + MR || D < Z.Inner * Radius / Z.Radius - MR))
			continue;
		if (Z.Shape == TEXT("sector") && (D > Reach + MR || FMath::Abs(Wrap(FMath::Atan2(Y, X) - Z.Direction)) >
																Z.Angle / 2 + FMath::Atan2(MR, FMath::Max(.1, D))))
			continue;
		if (Z.Shape == TEXT("line"))
		{
			const double Along = X * FMath::Cos(Z.Direction) + Y * FMath::Sin(Z.Direction),
						 Across = FMath::Abs(-X * FMath::Sin(Z.Direction) + Y * FMath::Cos(Z.Direction));
			if (Along < -MR || Along > Z.Length + MR || Across > Z.Width / 2 + MR)
				continue;
		}
		Out.Emplace(Id, D);
	}
	return Out;
}
int32 FDWZones::Resolve(FDWZone &Z, double Scale, double Radius, TOptional<FVector2d> Ring, double Tick)
{
	if (Radius < 0)
		Radius = Z.Radius;
	auto List = Victims(Z, Radius);
	List.RemoveAll([&](const auto &V) {
		if (Ring.IsSet() && (V.Value < Ring->X || V.Value > Ring->Y + R.Enemy(V.Key)->Radius))
			return true;
		if (S(TEXT("area.no_rehit_spread")) && Z.HitIds->Contains(V.Key))
			return true;
		if (Z.bPersistent || Z.bGround)
		{
			const double Last = Z.LastHit.Contains(V.Key) ? Z.LastHit[V.Key] : -1.e30;
			if (Z.HitCounts.FindRef(V.Key) >= 3 + FMath::FloorToInt(S(TEXT("area.rehit_cap"))) ||
				R.Time - Last < .5 * FMath::Max(0., 1 - S(TEXT("area.rehit_interval")) / 100) - 1.e-6)
				return true;
		}
		return false;
	});
	R.Effects.Add({Z.Effect, Z.Position, Z.Shape == TEXT("line") ? Z.Width / 2 : Radius, Z.Amount, Z.Element});
	auto &Visual = R.Effects.Last();
	Visual.Shape = Z.Shape;
	Visual.Direction = Z.Direction;
	Visual.Length = Z.Length;
	Visual.Width = Z.Width;
	Visual.Inner = Z.Inner * Radius / Z.Radius;
	Visual.Angle = Z.Angle;
	if (Z.Shape == TEXT("sector"))
		Visual.Radius = Z.SectorRadius * Radius / Z.Radius;
	if (Z.Shape == TEXT("ring"))
		Visual.Radius = Z.Outer * Radius / Z.Radius;
	double CountBonus = List.Num() == 1 ? S(TEXT("area.single_target")) : 0;
	for (const auto *T : R.T(TEXT("area_per_enemy")))
		CountBonus += FMath::Min(T->Param(TEXT("max")), T->Param(TEXT("n")) * List.Num());
	const double CenterShare = FMath::Min(1., (30 + S(TEXT("area.center_share"))) / 100),
				 Falloff = .25 * FMath::Max(0., 1 - S(TEXT("area.falloff")) / 100);
	const double CenterBonus = S(TEXT("area.center_damage")) + (Z.Element == 0 ? S(TEXT("fire.center_damage")) : 0);
	TArray<uint32> Ids;
	for (const auto &V : List)
	{
		const double Ratio = FMath::Min(1., V.Value / FMath::Max(.1, Z.Shape == TEXT("line") ? Z.Length : Radius));
		double More = Scale * (1 + CountBonus / 100) * (Z.bPrimary ? 1 - Falloff * Ratio : 1);
		More *= 1 + (Ratio <= CenterShare ? CenterBonus : S(TEXT("area.edge_damage"))) / 100;
		const int32 Hits = Z.HitCounts.FindRef(V.Key);
		More *= 1 + Hits * S(TEXT("area.consecutive")) / 100;
		for (const auto *T : R.T(TEXT("area_stay")))
			More *= 1 + FMath::FloorToDouble(Z.Stay.FindRef(V.Key) / FMath::Max(.1, T->Param(TEXT("seconds")))) *
							T->Param(TEXT("n")) / 100;
		const uint64 Key = (uint64(uint32(Z.Root)) << 32) | V.Key;
		if (const auto *Recent = RecentHits.Find(Key))
			if (Recent->Zone != Z.Id && R.Time - Recent->Time < .25)
				More *= FMath::Min(1., .5 * (1 + S(TEXT("area.overlap")) / 100));
		RecentHits.Add(Key, {Z.Id, R.Time});
		if (Z.bPrimary && S(TEXT("area.focus")))
			More *= 1.4;
		if (Z.bPrimary && S(TEXT("area.widen")))
			More *= .75;
		Z.HitCounts.Add(V.Key, Hits + 1);
		Z.LastHit.Add(V.Key, R.Time);
		Z.HitIds->Add(V.Key);
		Ids.Add(V.Key);
		FDWDamageInfo Info;
		Info.Position = Info.Center = FVector3f(Z.Position);
		Info.Tags = Z.Tags;
		Info.Radius = float(Radius);
		Info.bHasCenter = true;
		Info.ZoneId = Z.Id;
		Info.ZoneMore = More;
		Info.Kind = TEXT("host");
		R.World.HostHit(Z.Root, V.Key, float(Z.Amount * Tick), Z.Element, Info, false, Z.Generation, Z.EventType,
						&Z.Base);
	}
	if (!Z.bPersistent)
		Pull(Z, Ids, .25);
	return List.Num();
}
void FDWZones::Pull(const FDWZone &Z, const TArray<uint32> &Ids, double Seconds)
{
	const int32 Sign = S(TEXT("area.pull")) ? -1 : S(TEXT("area.push")) ? 1 : 0;
	if (!Sign)
		return;
	const double Speed = 2 * (1 + S(TEXT("area.pull_speed")) / 100) * Seconds;
	for (uint32 Id : Ids)
		if (auto *M = R.Enemy(Id))
		{
			FVector3d Delta = M->Position - Z.Position;
			Delta.Z = 0;
			const double D = Delta.Size();
			const double Move = Sign < 0 ? FMath::Min(Speed, D ? D : 1) : Speed;
			M->Position += Delta / (D ? D : 1) * (Sign * Move);
		}
}
void FDWZones::OnHit(const FDWCombatHit &H)
{
	const auto *P = Zones.FindByPredicate([&](const auto &Z) { return Z->Id == H.Info.ZoneId; });
	if (!P || (*P)->bSecondary || !R.Chance(S(TEXT("area.secondary_chance"))) || !R.Enemy(H.Id))
		return;
	FDWZoneTemplate Child = **P;
	Child.Position = R.Enemy(H.Id)->Position;
	Child.Radius *= .5 * FMath::Sqrt(1 + S(TEXT("area.secondary_size")) / 100);
	Child.Amount *= .5 * (1 + S(TEXT("area.secondary_damage")) / 100);
	Child.Shape = TEXT("circle");
	Child.bPersistent = false;
	Child.bFollowSelf = false;
	Child.FollowTarget = InvalidId;
	Child.Life = Child.Delay = 0;
	Child.bSecondary = true;
	Child.bPrimary = Child.bRepeat = Child.bSpread = false;
	Spawn(Child);
}
void FDWZones::OnKill(const FDWCombatHit &H)
{
	if (!H.Info.ZoneId || !R.Enemy(H.Id))
		return;
	R.Cooldown = FMath::Max(0., R.Cooldown - S(TEXT("area.kill_cooldown")));
	auto *P = Zones.FindByPredicate([&](const auto &Z) { return Z->Id == H.Info.ZoneId; });
	if (!P)
		return;
	auto &Z = **P;
	FDWZoneTemplate Child = Z;
	Child.Position = R.Enemy(H.Id)->Position;
	Child.bRecast = true;
	Child.bPersistent = false;
	Child.bFollowSelf = false;
	Child.FollowTarget = InvalidId;
	Child.Life = 0;
	Child.bRepeat = false;
	if (!Z.bRecast && R.Chance(S(TEXT("area.kill_recast_chance"))))
	{
		Child.Delay = .1;
		Spawn(Child);
	}
	if (S(TEXT("area.no_rehit_spread")) && Z.KillSpreads < 6)
	{
		++Z.KillSpreads;
		Child.Delay = .15;
		Child.bSpread = false;
		if (auto *Next = Spawn(Child))
		{
			Next->HitIds = Z.HitIds;
			Next->KillSpreads = Z.KillSpreads;
		}
	}
}
void FDWZones::Tick(double Dt)
{
	for (int32 I = 0; I < Zones.Num(); ++I)
	{
		auto &Z = *Zones[I];
		Z.Time += Dt;
		if (Z.bFollowSelf)
			Z.Position = R.HeroPosition();
		else if (Z.FollowTarget != InvalidId)
		{
			FVector3d P;
			if (R.TargetPosition(Z.FollowTarget, P))
				Z.Position = P;
		}
		if(Z.bRoaming)
        {
            auto Random=[&](){Z.RoamSeed=Z.RoamSeed*1664525u+1013904223u;return double(Z.RoamSeed)/4294967296.;};
            double Left=FMath::Clamp(Z.Time-Z.Delay,0.,Z.Life)-FMath::Clamp(Z.Time-Dt-Z.Delay,0.,Z.Life);
            while(Left>1.e-9)
            {
                if(Z.RoamTurnLeft<=1.e-9){Z.Direction+=(Random()<.5?-1:1)*(PI/3+Random()*PI/3);Z.RoamTurnLeft=Z.RoamInterval*(.75+Random()*.5);}
                const double Step=FMath::Min3(Left,Z.RoamTurnLeft,.5/FMath::Max(.01,Z.RoamSpeed));FVector3d Dir(FMath::Cos(Z.Direction),FMath::Sin(Z.Direction),0),Next=Z.Position+Dir*Z.RoamSpeed*Step;
                FVector3d Away=Next-Z.RoamOrigin;Away.Z=0;const double Reach=Away.Size();if(Reach>Z.RoamRange){const auto N=Away/Reach;Next=Z.RoamOrigin+N*Z.RoamRange;Dir-=N*(2*FVector3d::DotProduct(Dir,N));Z.Direction=FMath::Atan2(Dir.Y,Dir.X);}
                const float Radius=float(FMath::Min(.5,Z.Radius));const FVector3f Start=FVector3f(Z.Position+FVector3d(0,0,.6)),Finish=FVector3f(Next+FVector3d(0,0,.6));float At=1;
                if(R.World.SweepGeometry&&R.World.SweepGeometry(Start,Finish,Radius,At)){Next=Z.Position+(Next-Z.Position)*FMath::Max(0.f,At-.001f);float TX=1,TY=1;const bool BX=R.World.SweepGeometry(Start,FVector3f(Finish.X,Start.Y,Start.Z),Radius,TX),BY=R.World.SweepGeometry(Start,FVector3f(Start.X,Finish.Y,Start.Z),Radius,TY);Z.Direction=FMath::Atan2(BY||!BX?-Dir.Y:Dir.Y,BX||!BY?-Dir.X:Dir.X);Z.RoamTurnLeft=FMath::Min(Z.RoamTurnLeft,Z.RoamInterval*.25);}
                Z.Position=Next;Z.RoamTurnLeft-=Step;Left-=Step;
            }
        }
        else if (Z.bPersistent && S(TEXT("area.move_forward")) && Z.bPrimary)
			Z.Position += FVector3d(FMath::Cos(Z.Direction), FMath::Sin(Z.Direction), 0) *
						  (3 * (1 + S(TEXT("area.move_speed")) / 100) * Dt);
		if (Z.bPersistent && Z.Shape == TEXT("sector"))
			Z.Direction += UE_DOUBLE_PI / 2 * (1 + S(TEXT("area.rotate_speed")) / 100) * Dt;
		if (Z.Time < Z.NextAt)
			continue;
		if (Z.bPersistent || Z.bGround)
		{
			TickPersistent(Z, Dt);
			continue;
		}
		if (!Z.bFired)
		{
			Z.bFired = true;
			Resolve(Z);
			Z.NextAt = Z.Time + .35 * FMath::Max(.05, 1 - S(TEXT("area.repeat_interval")) / 100);
			Z.NextSpread = Z.Time + .25 * FMath::Max(.05, 1 - S(TEXT("area.spread_interval")) / 100);
			continue;
		}
		if (Z.SpreadsLeft > 0 && Z.Time >= Z.NextSpread)
		{
			const double Before = Z.SpreadRadius;
			Z.SpreadRadius += Z.Radius * .5 + S(TEXT("area.spread_radius"));
			--Z.SpreadsLeft;
			Resolve(Z, 1, Z.SpreadRadius, FVector2d(Before, Z.SpreadRadius));
			Z.NextSpread = Z.Time + .25 * FMath::Max(.05, 1 - S(TEXT("area.spread_interval")) / 100);
		}
		if (Z.RepeatsLeft > 0 && Z.Time >= Z.NextAt)
		{
			--Z.RepeatsLeft;
			const int32 K = ++Z.RepeatIndex;
			Resolve(Z, 1 + K * S(TEXT("area.repeat_damage")) / 100,
					Z.Radius * FMath::Sqrt(1 + K * S(TEXT("area.repeat_size")) / 100));
			Z.NextAt = Z.Time + .35 * FMath::Max(.05, 1 - S(TEXT("area.repeat_interval")) / 100);
		}
		if (Z.RepeatsLeft <= 0 && Z.SpreadsLeft <= 0)
			End(Z);
	}
	Zones.RemoveAll([](const auto &Z) { return Z->bEnded; });
	if (RecentHits.Num() > 2000)
		RecentHits.Reset();
}
void FDWZones::TickPersistent(FDWZone &Z, double Dt)
{
	if (Z.Time - Z.NextAt >= Z.Life)
	{
		End(Z);
		return;
	}
	TArray<uint32> Ids;
	for (const auto &V : Victims(Z, Z.CurrentRadius != 0 ? Z.CurrentRadius : Z.Radius))
	{
		Z.Stay.FindOrAdd(V.Key) += Dt;
		Ids.Add(V.Key);
	}
	Pull(Z, Ids, Dt);
	if (S(TEXT("area.pulse")) && Z.bPrimary && !Z.bGround)
	{
		const double Expand = .4 / FMath::Max(.1, 1 + S(TEXT("area.expand_speed")) / 100),
					 Contract = .4 / FMath::Max(.1, 1 + S(TEXT("area.contract_speed")) / 100);
		const double T = FMath::Fmod(Z.Time - Z.NextAt, Expand + Contract);
		const bool WasExpanding = Z.bExpand;
		Z.bExpand = T < Expand;
		Z.CurrentRadius = Z.bExpand ? Z.MaxRadius * T / Expand : Z.MaxRadius * (1 - (T - Expand) / Contract);
		if (WasExpanding && !Z.bExpand)
		{
			Z.HitCounts.Reset();
			Z.LastHit.Reset();
			Resolve(Z, 1, Z.MaxRadius);
		}
		if (!WasExpanding && Z.bExpand && S(TEXT("area.pulse_both")))
		{
			Z.HitCounts.Reset();
			Z.LastHit.Reset();
			Resolve(Z, 1, FMath::Max(.5, Z.MaxRadius * .4));
		}
		return;
	}
	Z.Tick += Dt;
	if (Z.Tick < Z.Interval && Z.bFired)
		return;
	Z.Tick = 0;
	Z.bFired = true;
	const double Per = Z.bGround ? .2 * Z.Interval : .4 * Z.Interval / .5;
	if (S(TEXT("area.consume_remaining")) && !Z.bGround)
	{
		const double Ticks = FMath::Max(1., FMath::FloorToDouble((Z.Life - (Z.Time - Z.NextAt)) / Z.Interval + .5));
		Resolve(Z, 1, -1, {}, Per * Ticks);
		End(Z);
		return;
	}
	Resolve(Z, 1, -1, {}, Per);
}
void FDWZones::End(FDWZone &Z)
{
	if (Z.bEnded)
		return;
	Z.bEnded = true;
	if (Z.bPrimary && !Z.bSecondary)
	{
		FDWZoneTemplate Child = Z;
		Child.Shape = TEXT("circle");
		Child.bPersistent = Child.bFollowSelf = false;
		Child.FollowTarget = InvalidId;
		Child.Life = Child.Delay = 0;
		Child.bPrimary = false;
		Child.bSecondary = true;
		Child.bRepeat = Child.bSpread = false;
		if (S(TEXT("area.end_explosion")))
		{
			auto Blast = Child;
			Blast.Radius *= .6;
			Blast.Amount *= .5;
			Spawn(Blast);
		}
		if (S(TEXT("area.leave_ground")))
		{
			auto Ground = Child;
			Ground.bGround = true;
			Ground.Life = 3 * (1 + S(TEXT("area.duration")) / 100);
			Spawn(Ground);
		}
		if (!Z.bRecast && R.Chance(S(TEXT("area.recast_chance"))))
		{
			FDWZoneTemplate Recast = Z;
			Recast.Delay = .2;
			Recast.bRecast = true;
			Spawn(Recast);
		}
	}
	R.World.ReleaseHeldRoot(Z.Root);
}
} // namespace DWRules
