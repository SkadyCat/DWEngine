#include "DWRuleWorld.h"

namespace DWRules
{
	namespace
	{
		FORCEINLINE uint8 ChannelDepth(uint32 Lineage, int32 Channel)
		{
			return uint8((Lineage >> (Channel * 8)) & 0xFF);
		}

		FORCEINLINE uint32 BumpChannel(uint32 Lineage, int32 Channel)
		{
			if (Channel < 0)
			{
				return Lineage;
			}
			const uint32 Depth = FMath::Min<uint32>(ChannelDepth(Lineage, Channel) + 1, 255);
			const uint32 Mask = 0xFFu << (Channel * 8);
			return (Lineage & ~Mask) | (Depth << (Channel * 8));
		}

		FORCEINLINE FVector3f FlatDirection(const FVector3f& Value, const FVector3f& Fallback)
		{
			const FVector3f Flat(Value.X, Value.Y, 0.f);
			return Flat.IsNearlyZero() ? Fallback : Flat.GetSafeNormal();
		}
	}

	void FDWRuleWorld::PushEvent(const FDWEvent& Event)
	{
		++Roots[Event.Root].Pending;
		Next.Add(Event);
	}

	void FDWRuleWorld::DrainWaves()
	{
		int32 Wave = 0;
		while (Current.Num() > 0 || Next.Num() > 0)
		{
			if (Current.Num() == 0)
			{
				Swap(Current, Next);
			}
			if (Wave >= Config.MaxWaves || FrameWork >= Config.WorkPerFrame)
			{
				// Out of budget: keep causal order, finish next frame.
				for (const FDWEvent& Event : Current)
				{
					if (Deferred.Num() < Config.MaxDeferredEvents)
					{
						Deferred.Add(Event);
					}
					else
					{
						--Roots[Event.Root].Pending;
						++FrameStats.DroppedEvents;
					}
				}
				Current.Reset();
				continue;
			}
			++Wave;
			++FrameStats.Waves;
			// Index loop: Current is stable while processing; children go to Next.
			int32 Index = 0;
			for (; Index < Current.Num() && FrameWork < Config.WorkPerFrame; ++Index)
			{
				const FDWEvent Event = Current[Index];
				ProcessEvent(Event);
				--Roots[Event.Root].Pending;
				++FrameStats.Events;
			}
			if (Index < Current.Num())
			{
				// Frame budget reached mid-wave: unprocessed events keep their order for next frame.
				++FrameStats.BudgetStops;
				Current.RemoveAt(0, Index, EAllowShrinking::No);
				continue;
			}
			Current.Reset();
		}
	}

	void FDWRuleWorld::ProcessEvent(const FDWEvent& Event)
	{
		FDWRoot& Root = Roots[Event.Root];
		const FDWCompiledPlan& RootPlan = *Plans[Root.Plan];
		if (!RootPlan.HasRules(Event.Type))
		{
			return;
		}
		const int32 First = RootPlan.EventRuleFirst[Event.Type];
		const int32 Last = First + RootPlan.EventRuleCount[Event.Type];
		for (int32 RuleIndex = First; RuleIndex < Last; ++RuleIndex)
		{
			const FDWRuleDef& Rule = RootPlan.Rules[RuleIndex];
			const bool bTooDeep = Rule.Channel >= 0
				? ChannelDepth(Event.Lineage, Rule.Channel) >= Rule.MaxDepth
				: Event.Generation > Rule.MaxDepth;
			if (bTooDeep)
			{
				++FrameStats.DepthBlocked;
				continue;
			}
			if ((Event.Tags & Rule.RequireAll) != Rule.RequireAll || (Event.Tags & Rule.Forbid) != 0 ||
				(Rule.RequireAny != 0 && (Event.Tags & Rule.RequireAny) == 0))
			{
				continue;
			}
			uint16& Chain = Root.ChainCount[RuleIndex];
			if (Chain >= Rule.MaxEvents)
			{
				++FrameStats.ChainBlocked;
				continue;
			}
			FContext Context{ Event, RootPlan, Root, int32(Event.Root), Rule.Channel, Event.Stored, RootPlan.TraitIds[Rule.Trait] };
			if (!CheckConditions(Rule, Context))
			{
				continue;
			}
			++Chain;
			RunActions(Rule.ActionFirst, Rule.ActionCount, Context, 0);
		}
	}

	bool FDWRuleWorld::CheckConditions(const FDWRuleDef& Rule, const FContext& Context) const
	{
		for (int32 Index = 0; Index < Rule.ConditionCount; ++Index)
		{
			const FDWConditionDef& Condition = Context.Plan.Conditions[Rule.ConditionFirst + Index];
			const float Value = Eval(Condition.Value, Context);
			bool bPass = true;
			switch (Condition.Kind)
			{
			case ECond::Generation:
				bPass = float(Context.Event.Generation) <= Value;
				break;
			case ECond::State:
				bPass = Compare(Condition.Compare, ReadState(Condition.Scope, Condition.Key, Context), Value);
				break;
			case ECond::Chance:
				bPass = Random01(Context.Root.Seed, Context.Event.Sequence, uint64(Rule.ActionFirst) + 0x5EED) * 100.f < Value;
				break;
			case ECond::Status:
				bPass = Compare(Condition.Compare, GetStatus(Context.Event.TargetId, Condition.Key), Value);
				break;
			case ECond::Active:
            {
                FString Active=Context.Plan.ActiveId.ToString();int32 Suffix;if(Active.FindChar('~',Suffix))Active.LeftInline(Suffix);
                bPass=Condition.Compare==ECompare::Ne?FName(*Active)!=Condition.Active:FName(*Active)==Condition.Active;
                break;
            }
			case ECond::EntityCount:
			{
				int32 Count = 0;
				if (Condition.EntityKind == MAX_uint16)
				{
					Count = Owners[Context.Root.Owner].LiveProjectiles;
				}
				else
				{
					for (int32 Carrier = 0; Carrier < Carriers.Num; ++Carrier)
					{
						const bool bMine = int32(Carriers.Owner[Carrier]) == Context.Root.Owner;
						const bool bAlive = !(Carriers.Flags[Carrier] & FDWProjectiles::Dead);
						Count += (bMine && bAlive && Carriers.Kind[Carrier] == Condition.EntityKind) ? 1 : 0;
					}
				}
				bPass = Compare(Condition.Compare, float(Count), Value);
				break;
			}
			}
			if (!bPass)
			{
				return false;
			}
		}
		return true;
	}

	bool FDWRuleWorld::RunActions(int32 First, int32 Count, FContext& Context, int32 Depth)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!RunAction(Context.Plan.Actions[First + Index], Context, Depth))
			{
				return false;
			}
		}
		return true;
	}

	bool FDWRuleWorld::RunAction(const FDWActionDef& Action, FContext& Context, int32 Depth)
	{
		// Per-cast hard cap: a single root can never monopolise the world.
		if (Context.Root.Work >= Config.RootWorkCap)
		{
			++FrameStats.BudgetStops;
			return false;
		}
		++FrameWork;
		++Context.Root.Work;
		++FrameStats.Actions;
		switch (Action.Op)
		{
		case EOp::Emit:
			return RunEmit(Action, Context);
		case EOp::Copy:
			return RunCopy(Action, Context);
		case EOp::DamageArea:
			return RunDamageArea(Action, Context);
		case EOp::DamageTarget:
			return RunDamageTarget(Action, Context);
		case EOp::Counter:
		case EOp::Accumulate:
			return RunAccumulate(Action, Context, Depth);
		case EOp::Consume:
			return RunConsume(Action, Context, Depth);
        case EOp::MultiplyDamage:
        {const float Factor=SlotOr(Action,ESlot::Value,Context,1);if(!FMath::IsFinite(Factor)||Factor<0)return false;Context.Root.DamageMultiplier=FMath::Clamp(Context.Root.DamageMultiplier*Factor,0.f,1.e6f);return true;}
        case EOp::ConsumeStatus:return RunConsumeStatus(Action,Context,Depth);
        case EOp::RepeatSkill:return RunRepeatSkill(Context);
        case EOp::MaintainEmitter:return RunMaintainEmitter(Action,Context);
        case EOp::SpawnMonster:
            return Extension&&Extension->SpawnMonster(Context.Plan.KindNames[Action.Kind],FMath::Clamp(FMath::FloorToInt(SlotOr(Action,ESlot::Count,Context,1)),0,128),SlotOr(Action,ESlot::Duration,Context,5),SlotOr(Action,ESlot::Radius,Context,5),Action.bInvulnerable,Context.Root.Owner);
		case EOp::EmitEvent:
		{
			FDWEvent Child = MakeChild(Context, Action.Event, Action.AddTags);
			if (Action.Slot(ESlot::Amount).bSet)
			{
				Child.Amount = Eval(Action.Slot(ESlot::Amount), Context);
			}
			PushEvent(Child);
			return true;
		}
		case EOp::Redirect:
			return RunRedirect(Action, Context);
		case EOp::ApplyStatus:
			return RunApplyStatus(Action, Context);
		case EOp::Sacrifice:
        {
            if(Extension)Extension->Sacrifice(Context.RootIndex,FMath::Clamp(FMath::FloorToInt(SlotOr(Action,ESlot::Count,Context,1)),0,32),Context.Plan.KindNames.IsValidIndex(Action.Kind)?Context.Plan.KindNames[Action.Kind]:NAME_None);
            return true;
        }
        case EOp::GrantTraits:
		{
			const uint32 Target = Action.Origin == EOrigin::Owner ? Owners[Context.Root.Owner].StatusId : Context.Event.TargetId;
			const float Duration = SlotOr(Action, ESlot::Duration, Context, 0.f);
			if (Target != InvalidId && Duration > 0.f && Context.Plan.Grants.IsValidIndex(Action.Grant) && OnGrantTraits)
				OnGrantTraits(Target, Context.Root.Owner, Duration, Context.Plan.Grants[Action.Grant]);
			return true;
		}
        case EOp::Storm:
        case EOp::Summon:
			return RunSummon(Action, Context);
		}
		return true;
	}

	float FDWRuleWorld::Eval(const FDWExprRef& Ref, const FContext& Context) const
	{
		if (Ref.Count == 0)
		{
			return Ref.Const;
		}
		return EvaluateExpr(&Context.Plan.Expr[Ref.First], Ref.Count, [this, &Context](const FDWExprInstr& In) -> float
		{
			switch (In.Op)
			{
			case EExprOp::EventAmount: return Context.Event.Amount;
			case EExprOp::EventDamage: return Context.Event.Damage;
			case EExprOp::EventStored: return Context.Stored;
			case EExprOp::EventGeneration: return float(Context.Event.Generation);
			case EExprOp::EventSequence: return float(Context.Event.Sequence);
			case EExprOp::State: return ReadState(In.Scope, In.Key, Context);
			default: return 0.f;
			}
		});
	}

	float FDWRuleWorld::SlotOr(const FDWActionDef& Action, ESlot Slot, const FContext& Context, float Default) const
	{
		const FDWExprRef& Ref = Action.Slot(Slot);
		return Ref.bSet ? Eval(Ref, Context) : Default;
	}

	float FDWRuleWorld::ReadState(EScope Scope, uint64 Key, const FContext& Context) const
	{
		const float* Found = nullptr;
		switch (Scope)
		{
		case EScope::Owner:
			Found = OwnerState.Find(Mix64(Key ^ (uint64(Context.Root.Owner) << 40)));
			break;
		case EScope::Target:
			if (Context.Event.TargetId != InvalidId)
			{
				Found = TargetState.Find(Mix64(Key ^ (uint64(Context.Event.TargetId) << 24)));
			}
			break;
		case EScope::Cast:
			Found = Context.Root.CastState.Find(Key);
			break;
		}
		return Found ? *Found : 0.f;
	}

	void FDWRuleWorld::WriteState(EScope Scope, uint64 Key, const FContext& Context, float Value)
	{
		const float Clean = FMath::IsFinite(Value) ? FMath::Max(0.f, Value) : 0.f;
		switch (Scope)
		{
		case EScope::Owner:
			OwnerState.Add(Mix64(Key ^ (uint64(Context.Root.Owner) << 40)), Clean);
			OwnerStateKeys.FindOrAdd(Context.Root.Owner).Add(Mix64(Key ^ (uint64(Context.Root.Owner) << 40)));
			break;
		case EScope::Target:
			if (Context.Event.TargetId != InvalidId)
			{
				TargetState.Add(Mix64(Key ^ (uint64(Context.Event.TargetId) << 24)), Clean);
				TargetStateKeys.FindOrAdd(Context.Event.TargetId).Add(Mix64(Key ^ (uint64(Context.Event.TargetId) << 24)));
			}
			break;
		case EScope::Cast:
			Context.Root.CastState.Add(Key, Clean);
			break;
		}
	}

	bool FDWRuleWorld::ResolveSource(const FDWEvent& Event) const
	{
		const int32 Index = Event.SourceIndex;
		return Index >= 0 && Index < Carriers.Num && Carriers.Id[Index] == Event.SourceId &&
			!(Carriers.Flags[Index] & FDWProjectiles::Dead);
	}

	FDWEvent FDWRuleWorld::MakeChild(const FContext& Context, uint16 Type, uint64 AddTags) const
	{
		FDWEvent Child = Context.Event;
		Child.Type = Type;
		Child.Generation = uint8(FMath::Min(int32(Context.Event.Generation) + 1, MaxGeneration));
		Child.Lineage = BumpChannel(Context.Event.Lineage, Context.Channel);
		Child.Sequence = Context.Root.NextSequence++;
		Child.Tags = (Context.Event.Tags & ~NonInheritedTags) | AddTags;
		Child.Stored = Context.Stored;
		return Child;
	}

	void FDWRuleWorld::PushHitEvents(FDWRoot& Root, int32 RootIndex, const FDWEvent& Base, int32 Target, float Applied,
		float Requested, bool bCritical, bool bKilled, uint16 HitType)
	{
		const FDWCompiledPlan& RootPlan = *Plans[Root.Plan];
		FDWEvent Event = Base;
		Event.Root = uint32(RootIndex);
		Event.TargetIndex = Target;
		Event.TargetId = TargetData.ExternalId[Target];
		Event.Amount = Applied;
		Event.Damage = Requested;
		if (bCritical)
		{
			Event.Tags |= TagBit(Tags::Critical);
		}
		if (RootPlan.HasRules(HitType))
		{
			Event.Type = HitType;
			Event.Sequence = Root.NextSequence++;
			PushEvent(Event);
		}
		if (bCritical && RootPlan.HasRules(uint16(EBuiltinEvent::Critical)))
		{
			Event.Type = uint16(EBuiltinEvent::Critical);
			Event.Sequence = Root.NextSequence++;
			PushEvent(Event);
		}
		if (bKilled && RootPlan.HasRules(uint16(EBuiltinEvent::Kill)))
		{
			Event.Type = uint16(EBuiltinEvent::Kill);
			Event.Sequence = Root.NextSequence++;
			Event.Tags |= TagBit(Tags::OwnerKill);
			Event.Position = TargetData.Position[Target];
			PushEvent(Event);
		}
	}

	bool FDWRuleWorld::RunEmit(const FDWActionDef& Action, FContext& Context)
	{
		const FDWEvent& Event = Context.Event;
		const FDWStatBlock& Stats = Context.Plan.Stats;
		const bool bInherit = (Action.Flags & FDWActionDef::Inherit) != 0;
		// Inherit: the action field (or the stat default) is the base of the build's partition.
		auto Value = [&](EStat Stat, ESlot Slot)
		{
			const float Base = SlotOr(Action, Slot, Context, GetStatInfo(StatId(Stat)).Default);
			return bInherit ? Stats.From(Stat, Base) : Base;
		};
		const int32 Count = FMath::Clamp(FMath::RoundToInt32(Value(EStat::ProjectileCount, ESlot::Count)), 0, 512);
		const float Spread = Value(EStat::ProjectileSpread, ESlot::Spread);
		const float Speed = Value(EStat::ProjectileSpeed, ESlot::Speed);
		const float Life = Value(EStat::ProjectileLifetime, ESlot::Lifetime);
		const float Radius = Value(EStat::ProjectileRadius, ESlot::Radius);
		const float DamageValue = Value(EStat::ProjectileDamage, ESlot::Damage);
		const int32 Pierce = FMath::RoundToInt32(Value(EStat::ProjectilePierce, ESlot::Pierce));
		const uint8 Element = Action.Element >= 0 ? uint8(Action.Element) : Event.Element;
		const FDWOwner& Owner = Owners[Context.Root.Owner];
		FVector3f Origin = Event.Position;
		if (Action.Origin == EOrigin::Owner)
		{
			Origin = Owner.Position;
		}
		else if (Action.Origin == EOrigin::Source && ResolveSource(Event))
		{
			Origin = Carriers.Position[Event.SourceIndex];
		}
		else if (Action.Origin == EOrigin::Target && TargetData.ExternalId.IsValidIndex(Event.TargetIndex) &&
			TargetData.ExternalId[Event.TargetIndex] == Event.TargetId)
		{
			Origin = TargetData.Position[Event.TargetIndex];
		}
		const FVector3f OffsetAim=Action.Origin==EOrigin::Owner?Owner.Aim:FlatDirection(Event.Direction,Owner.Aim);
        Origin+=OffsetAim*SlotOr(Action,ESlot::OffsetForward,Context,0)+FVector3f::UpVector*SlotOr(Action,ESlot::OffsetUp,Context,0);
        const bool Arc = (Action.Flags & FDWActionDef::Parabolic) != 0;
		const auto Target = Arc ? Context.Root.TargetPoint : TOptional<FVector3f>();
		const FVector3f Forward =
			FlatDirection(Target.IsSet() ? (Target.GetValue() - Origin) : Event.Direction, Owner.Aim);
		const uint64 TagMask = (Event.Tags & ~NonInheritedTags) | Action.AddTags | TagBit(Tags::Projectile);
		const uint8 Generation = uint8(FMath::Min(int32(Event.Generation) + 1, MaxGeneration));
		const uint32 Lineage = BumpChannel(Event.Lineage, Context.Channel);
		const bool bSpawnRules = Context.Plan.HasRules(uint16(EBuiltinEvent::Spawn));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			float Angle = 0.f;
			if (Action.Layout == ELayout::Radial)
			{
				Angle = 360.f * float(Index) / float(Count);
			}
			else if (Action.Layout == ELayout::Fan && Count > 1)
			{
				Angle = Spread * (float(Index) / float(Count - 1) - 0.5f);
			}
            FVector3f ShotOrigin=Origin;
            if(Action.bHemisphere)
            {
                const float R=FMath::Max(0.f,SlotOr(Action,ESlot::ScatterRadius,Context,0));
                const float Z=Random01(Context.Root.Seed,Event.Sequence,uint64(Index)*3+0x4D01);
                const float A=2*PI*Random01(Context.Root.Seed,Event.Sequence,uint64(Index)*3+0x4D02);
                const float XY=FMath::Sqrt(FMath::Max(0.f,1-Z*Z));
                ShotOrigin+=FVector3f(FMath::Cos(A)*XY,FMath::Sin(A)*XY,Z)*R;
            }
			FVector3f Direction = Forward.RotateAngleAxis(Angle, FVector3f::UpVector);
            if(Action.bAimTarget&&TargetData.ExternalId.IsValidIndex(Event.TargetIndex)&&TargetData.ExternalId[Event.TargetIndex]==Event.TargetId)
                Direction=(TargetData.Position[Event.TargetIndex]-ShotOrigin).GetSafeNormal();
			const int32 Spawned = SpawnProjectile(Context.Root, Context.RootIndex, ShotOrigin, Direction, Speed, Life, Radius,
				DamageValue, Element, Action.Kind, TagMask, Generation, Lineage);
			if (Spawned == INDEX_NONE)
			{
				break;
			}
			if (Arc)
			{
				const float BaseSpeed = SlotOr(Action, ESlot::Speed, Context,
											   GetStatInfo(StatId(EStat::ProjectileSpeed)).Default);
				const float Duration = FMath::Max(1.f / 60.f, Target.IsSet() && Speed > 0 && BaseSpeed > 0
																  ? Life * BaseSpeed / Speed
																  : Life);
				const float Distance = Target.IsSet() ? FVector2f::Distance(FVector2f(Target->X, Target->Y),
																			FVector2f(ShotOrigin.X, ShotOrigin.Y))
													  : FMath::Max(0.f, Speed) * Duration;
				const float EndZ = FMath::Max(Radius, Target.IsSet() ? Target->Z : 0.f);
				const float Height = FMath::Max(0.f, SlotOr(Action, ESlot::ArcHeight, Context, 1.f));
				Carriers.Velocity[Spawned] =
					FVector3f(Direction.X * Distance / Duration, Direction.Y * Distance / Duration,
							  (EndZ - ShotOrigin.Z + 4 * Height) / Duration);
				Carriers.Gravity[Spawned] = -8 * Height / (Duration * Duration);
				Carriers.Life[Spawned] = Duration;
				Carriers.Flags[Spawned] |= FDWProjectiles::Parabolic;
			}
			Carriers.Pierce[Spawned] = uint16(FMath::Clamp(Pierce, 0, 1000));
			Carriers.EndEvent[Spawned] = Action.EndEvent;
			if (bInherit)
			{
				Carriers.Ricochet[Spawned] = uint16(Stats.Get(EStat::ProjectileRicochetCount));
				Carriers.Reflect[Spawned] = uint16(Stats.Get(EStat::ProjectileReflectionCount));
				Carriers.Return[Spawned] = uint16(Stats.Get(EStat::ProjectileReturnCount));
				Carriers.Refract[Spawned] = uint16(Stats.Get(EStat::ProjectileRefractionCount));
			}
			else
			{
				Carriers.Flags[Spawned] |= FDWProjectiles::Explicit;
			}
			if (Extension) Extension->Spawn(Spawned, Context.RootIndex, {Index, Count, true, Direction, ShotOrigin});
			if (bSpawnRules)
			{
				FDWEvent Spawn = MakeChild(Context, uint16(EBuiltinEvent::Spawn), Action.AddTags);
				Spawn.SourceIndex = Spawned;
				Spawn.SourceId = Carriers.Id[Spawned];
				Spawn.Position = ShotOrigin;
				Spawn.Direction = Direction;
				PushEvent(Spawn);
			}
		}
		return true;
	}

	bool FDWRuleWorld::RunCopy(const FDWActionDef& Action, FContext& Context)
	{
		const FDWEvent& Event = Context.Event;
		if (!ResolveSource(Event))
		{
			return true;
		}
		const int32 Source = Event.SourceIndex;
		const int32 Count = FMath::Clamp(FMath::RoundToInt32(SlotOr(Action, ESlot::Count, Context, 1.f)), 0, 512);
		const float Spread = SlotOr(Action, ESlot::Spread, Context, 30.f);
		const FVector3f Velocity = Carriers.Velocity[Source];
		const FVector3f Forward = Velocity.GetSafeNormal();
		const float Speed = Velocity.Size();
		const bool bInherit = (Action.Flags & FDWActionDef::Inherit) != 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = Count > 1 ? Spread * (float(Index) / float(Count - 1) - 0.5f) : Spread * 0.5f;
			const uint64 TagMask = (Carriers.Tags[Source] & ~NonInheritedTags) | Action.AddTags;
			const int32 Spawned = SpawnProjectile(Context.Root, Context.RootIndex, Event.Position,
				Forward.RotateAngleAxis(Angle, FVector3f::UpVector), Speed,
				FMath::Max(0.05f, Carriers.Life[Source] - Carriers.Age[Source]), Carriers.Radius[Source],
				Carriers.Damage[Source], Carriers.Element[Source], Carriers.Kind[Source], TagMask,
				uint8(FMath::Min(int32(Event.Generation) + 1, MaxGeneration)), BumpChannel(Event.Lineage, Context.Channel));
			if (Spawned == INDEX_NONE)
			{
				break;
			}
			Carriers.Hits[Spawned] = Carriers.Hits[Source];
			Carriers.Gravity[Spawned] = Carriers.Gravity[Source];
			Carriers.Flags[Spawned] |= Carriers.Flags[Source] & FDWProjectiles::Parabolic;
			Carriers.Velocity[Spawned].Z = Carriers.Velocity[Source].Z;
			Carriers.Pierce[Spawned] = Carriers.Pierce[Source];
			if (bInherit)
			{
				Carriers.Ricochet[Spawned] = Carriers.Ricochet[Source];
				Carriers.Reflect[Spawned] = Carriers.Reflect[Source];
				Carriers.Return[Spawned] = Carriers.Return[Source];
			}
			else
			{
				Carriers.Flags[Spawned] |= FDWProjectiles::Explicit;
			}
		}
		return true;
	}

	bool FDWRuleWorld::RunDamageArea(const FDWActionDef& Action, FContext& Context)
	{
		const FDWEvent& Event = Context.Event;
		const FDWStatBlock& Stats = Context.Plan.Stats;
		const float Radius = Stats.From(EStat::AreaRadius, SlotOr(Action, ESlot::Radius, Context, 0.f));
		const float Amount = SlotOr(Action, ESlot::Amount, Context, 0.f);
		if (Radius <= 0.f || Amount <= 0.f)
		{
			return true;
		}
		const uint8 Element = Action.Element >= 0 ? uint8(Action.Element) : Event.Element;
		TArray<int32, TInlineAllocator<64>> Victims;
		FDWAreaRequest Request;
		Request.RootIndex = Context.RootIndex; Request.Trait = Context.Trait;
		Request.Event = &Event;
		FDWEvent AreaBase = MakeChild(Context, Action.Event, Action.AddTags | TagBit(Tags::Area));
		AreaBase.Tags &= ~TagBit(Tags::Projectile); AreaBase.Element = Element;
		AreaBase.SourceIndex = INDEX_NONE; AreaBase.SourceId = InvalidId;
		Request.Base = &AreaBase; Request.EventType = Action.Event;
		Request.Radius = Radius; Request.Amount = Amount; Request.Element = Element;
		Request.Tags = AreaBase.Tags;
		if (Extension && Extension->Area(Request)) return true;
		ForEachTargetInRadius(Event.Position, Radius, [&](int32 Target)
		{
			if (IsHostile(Context.Root.Owner, Target))
			{
				Victims.Add(Target);
			}
		});
		Victims.Sort();
		FDWEvent Base = AreaBase;
		// Area damage is not a projectile delivery: drop carrier identity and projectile fact.
		Base.Tags &= ~TagBit(Tags::Projectile);
		Base.SourceIndex = INDEX_NONE;
		Base.SourceId = InvalidId;
		Base.Element = Element;
		for (int32 Target : Victims)
		{
			bool bCritical = false;
			bool bKilled = false;
			const uint32 Sequence = Context.Root.NextSequence++;
			FDWDamageInfo Info;
			Info.RootIndex = Context.RootIndex; Info.Tags = Base.Tags;
			Info.Position = Info.Center = Event.Position; Info.bHasCenter = true;
			Info.Radius = Radius; Info.Kind = TEXT("area");
			bool Missed = false;
			const float Applied = ResolveDamage(Context.Root, Target, Amount, Element, Sequence, bCritical, bKilled, Info, &Missed);
			if (Missed) continue;
			PushHitEvents(Context.Root, Context.RootIndex, Base, Target, Applied, Amount, bCritical, bKilled, Action.Event);
		}
		return true;
	}

	bool FDWRuleWorld::RunDamageTarget(const FDWActionDef& Action, FContext& Context)
	{
		const FDWEvent& Event = Context.Event;
		const int32 Target = Event.TargetIndex;
		if (!TargetData.ExternalId.IsValidIndex(Target) || TargetData.ExternalId[Target] != Event.TargetId)
		{
			return true;
		}
		const float Amount = SlotOr(Action, ESlot::Amount, Context, 0.f);
		const uint8 Element = Action.Element >= 0 ? uint8(Action.Element) : Event.Element;
		FDWEvent Base = MakeChild(Context, Action.Event, Action.AddTags);
		Base.Element = Element;
		bool bCritical = false;
		bool bKilled = false;
		const uint32 Sequence = Context.Root.NextSequence++;
		FDWDamageInfo Info;
		Info.RootIndex = Context.RootIndex; Info.Tags = Base.Tags;
		Info.Position = TargetData.Position[Target]; Info.Kind = TEXT("target");
		bool Missed = false;
		const float Applied = ResolveDamage(Context.Root, Target, Amount, Element, Sequence, bCritical, bKilled, Info, &Missed);
		if (Missed) return true;
		PushHitEvents(Context.Root, Context.RootIndex, Base, Target, Applied, Amount, bCritical, bKilled, Action.Event);
		return true;
	}

	bool FDWRuleWorld::RunAccumulate(const FDWActionDef& Action, FContext& Context, int32 Depth)
	{
		const float Default = Action.Op == EOp::Counter ? 1.f : 0.f;
		float Stock = ReadState(Action.Scope, Action.StateKey, Context) + SlotOr(Action, ESlot::Value, Context, Default);
		if (Action.Slot(ESlot::Cap).bSet)
		{
			Stock = FMath::Min(Stock, Eval(Action.Slot(ESlot::Cap), Context));
		}
		const float Threshold = SlotOr(Action, ESlot::Threshold, Context, 0.f);
		if (Threshold <= 0.f || Stock < Threshold)
		{
			WriteState(Action.Scope, Action.StateKey, Context, Stock);
			return true;
		}
		const int32 Fires = FMath::Min(FMath::FloorToInt32(Stock / Threshold), 64);
		WriteState(Action.Scope, Action.StateKey, Context, Stock - float(Fires) * Threshold);
		const float Saved = Context.Stored;
		for (int32 Fire = 0; Fire < Fires; ++Fire)
		{
			Context.Stored = Threshold;
			if (!RunActions(Action.ThenFirst, Action.ThenCount, Context, Depth + 1))
			{
				break;
			}
		}
		Context.Stored = Saved;
		return true;
	}

	bool FDWRuleWorld::RunConsume(const FDWActionDef& Action, FContext& Context, int32 Depth)
	{
		const float Stock = ReadState(Action.Scope, Action.StateKey, Context);
		const bool bAll = (Action.Flags & FDWActionDef::ConsumeAll) != 0;
		const float Need = bAll ? Stock : SlotOr(Action, ESlot::Value, Context, 0.f);
		if (Need <= 0.f || Stock < Need)
		{
			// Insufficient stock stops the remaining actions of this list.
			return false;
		}
		WriteState(Action.Scope, Action.StateKey, Context, Stock - Need);
		const float Saved = Context.Stored;
		Context.Stored = Need;
		RunActions(Action.ThenFirst, Action.ThenCount, Context, Depth + 1);
		Context.Stored = Saved;
		return true;
	}

	bool FDWRuleWorld::RunRedirect(const FDWActionDef& Action, FContext& Context)
	{
		const FDWEvent& Event = Context.Event;
		if (!ResolveSource(Event))
		{
			return true;
		}
		const int32 Index = Event.SourceIndex;
		const float Speed = Carriers.Velocity[Index].Size();
		FVector3f Direction = FVector3f::ZeroVector;
		uint32 Tag = 0;
		switch (Action.Mode)
		{
		case ERedirect::Ricochet:
		{
			if (Carriers.Ricochet[Index] == 0)
			{
				return true;
			}
			const float Radius = SlotOr(Action, ESlot::Radius, Context, Context.Plan.Stats.Get(EStat::ProjectileRicochetRadius));
			int32 Target = NearestTarget(Event.Position, Radius, Index, Context.Root, Owners[Context.Root.Owner].Team);
			if (Target == INDEX_NONE && Extension) Target = Extension->Ricochet(Index, Event, Radius);
			if (Target == INDEX_NONE)
			{
				return true;
			}
			Direction = (TargetData.Position[Target] - Event.Position).GetSafeNormal();
			--Carriers.Ricochet[Index];
			Tag = Tags::Ricochet;
			break;
		}
		case ERedirect::Reflect:
			if (Carriers.Reflect[Index] == 0)
			{
				return true;
			}
			Direction = -Carriers.Velocity[Index].GetSafeNormal();
			--Carriers.Reflect[Index];
			Tag = Tags::Reflect;
			break;
		case ERedirect::Return:
			if (Carriers.Return[Index] == 0)
			{
				return true;
			}
			Direction = (Owners[Context.Root.Owner].Position - Event.Position).GetSafeNormal();
			--Carriers.Return[Index];
			Tag = Tags::Return;
			break;
		}
		if (Direction.IsNearlyZero())
		{
			return true;
		}
		// Same carrier continues: position at the contact, fresh lifetime, impact cancelled.
		Carriers.Position[Index] = Event.Position;
		Carriers.Velocity[Index] = Direction * Speed;
		Carriers.Age[Index] = 0.f;
		Carriers.Flags[Index] &= ~(FDWProjectiles::PendingImpact | FDWProjectiles::Parabolic);
		Carriers.Gravity[Index] = 0;
		Carriers.Tags[Index] |= TagBit(Tag);
		if (Extension) Extension->Redirect(Index, Action.Mode);
		return true;
	}

	namespace
	{
		FORCEINLINE uint64 StatusSlot(uint32 TargetId, uint64 Key)
		{
			return Mix64(Key ^ (uint64(TargetId) << 24));
		}
	}

	void FDWRuleWorld::SetStatusDefs(TArray<FDWStatusDef> Defs)
	{
		// Definition order is the native key order on both runtimes (web parity).
		Defs.Sort([](const FDWStatusDef& A, const FDWStatusDef& B) { return A.Key < B.Key; });
		StatusDefs = MoveTemp(Defs);
		StatusDefByKey.Reset();
		bTraitStatuses = false;
		for (TArray<FEffectRef>& List : EffectDefs)
		{
			List.Reset();
		}
		for (int32 Index = 0; Index < StatusDefs.Num(); ++Index)
		{
			StatusDefByKey.Add(StatusDefs[Index].Key, Index);
			bTraitStatuses |= StatusDefs[Index].Traits.Num() > 0;
			for (const FDWStatusEffectDef& Effect : StatusDefs[Index].Effects)
			{
				EffectDefs[int32(Effect.Type)].Add({ Index, Effect.Scale, Effect.Control });
			}
		}
	}

	void FDWRuleWorld::SetOwnerStatusId(int32 Owner, uint32 StatusId)
	{
		if (Owners.IsValidIndex(Owner))
		{
			Owners[Owner].StatusId = StatusId;
		}
	}

	float FDWRuleWorld::StatusMagnitude(const FDWStatus& Status) const
	{
		float Value = Status.Value * float(Status.Stacks);
		const int32* Def = StatusDefByKey.Find(Status.Key);
		if (Def && StatusDefs[*Def].Cap > 0.f)
		{
			Value = FMath::Min(Value, StatusDefs[*Def].Cap);
		}
		return Value;
	}

	float FDWRuleWorld::GetStatus(uint32 TargetExternalId, uint64 Key) const
	{
		if (TargetExternalId == InvalidId)
		{
			return 0.f;
		}
		const FDWStatus* Found = Statuses.Find(StatusSlot(TargetExternalId, Key));
		return Found && Found->TargetId == TargetExternalId && Found->Expire > Clock ? StatusMagnitude(*Found) : 0.f;
	}

	float FDWRuleWorld::GetStatusRemaining(uint32 TargetExternalId, uint64 Key) const
	{
		const FDWStatus* Found = Statuses.Find(StatusSlot(TargetExternalId, Key));
		return Found && Found->TargetId == TargetExternalId ? FMath::Max(0.f, Found->Expire - Clock) : 0.f;
	}

	int32 FDWRuleWorld::GetStatusStacks(uint32 TargetExternalId, uint64 Key) const
	{
		const FDWStatus* Found = Statuses.Find(StatusSlot(TargetExternalId, Key));
		return Found && Found->TargetId == TargetExternalId && Found->Expire > Clock ? Found->Stacks : 0;
	}

	float FDWRuleWorld::EffectSum(uint32 StatusId, EStatusEffect Effect) const
	{
		float Sum = 0.f;
		for (const FEffectRef& Ref : EffectDefs[int32(Effect)])
		{
			const float Value = GetStatus(StatusId, StatusDefs[Ref.Def].Key);
			if (Value > 0.f)
			{
				Sum = Sum + Ref.Scale * Value;
			}
		}
		return Sum;
	}

	float FDWRuleWorld::GetStatusEffect(uint32 TargetExternalId, EStatusEffect Effect, float& OutRemaining) const
	{
		OutRemaining = 0.f;
		if (int32(Effect) >= int32(EStatusEffect::Num))
		{
			return 0.f;
		}
		float Value = 0.f;
		for (const FEffectRef& Ref : EffectDefs[int32(Effect)])
		{
			const uint64 Key = StatusDefs[Ref.Def].Key;
			const float Magnitude = GetStatus(TargetExternalId, Key);
			if (!(Magnitude > 0.f))
			{
				continue;
			}
			Value = Effect == EStatusEffect::Control ? FMath::Max(Value, float(Ref.Control)) : Value + Ref.Scale * Magnitude;
			OutRemaining = FMath::Max(OutRemaining, GetStatusRemaining(TargetExternalId, Key));
		}
		return Value;
	}

	float FDWRuleWorld::EffectiveResistance(int32 Target, uint8 Element, float Penetration) const
	{
		const int32 ResistIndex = ResistanceIndex(EElement(Element));
		if (ResistIndex == INDEX_NONE)
		{
			return 0.f;
		}
		const uint16 ResistanceStat = ElementStatId(Element, EElementStat::Resistance);
		const uint16 MaxResistanceStat = ElementStatId(Element, EElementStat::MaxResistance);
		float Resist = TargetData.Resistance[Target * NumResistances + ResistIndex];
		float Cap = GetStatInfo(MaxResistanceStat).Default;
		if (const int32* DefensePlan = TargetDefensePlans.Find(TargetData.ExternalId[Target]))
		{
			const FDWStatBlock& Defense = Plans[*DefensePlan]->Stats;
			Resist = Defense.From(ResistanceStat, Resist);
			Cap = Defense.Get(MaxResistanceStat);
		}
		Resist = FMath::Min(Resist, Cap);
		return Resist > 0.f ? FMath::Max(0.f, Resist - Penetration) : Resist;
	}

	float FDWRuleWorld::ApplyDamageTaken(int32 Target, float Dealt) const
	{
		if (EffectDefs[int32(EStatusEffect::DamageTaken)].Num() == 0)
		{
			return Dealt;
		}
		const float Taken = EffectSum(TargetData.ExternalId[Target], EStatusEffect::DamageTaken);
		return Taken != 0.f ? FMath::Max(0.f, Dealt * FMath::Max(0.f, 1.f + Taken / 100.f)) : Dealt;
	}

	void FDWRuleWorld::TickStatuses()
	{
		// Damage over time: magnitude is damage per second, ticking every interval up to expiry.
		// Ticks bypass crit, damage bonuses and rules (no hit/kill events): they can never recurse.
		if (EffectDefs[int32(EStatusEffect::DamageOverTime)].Num() == 0 || Statuses.Num() == 0)
		{
			return;
		}
		TArray<FDWStatus*> Due;
		for (TPair<uint64, FDWStatus>& Pair : Statuses)
		{
			FDWStatus& Status = Pair.Value;
			const int32* Def = StatusDefByKey.Find(Status.Key);
			if (Def && StatusDefs[*Def].TickInterval > 0.f && Status.TargetId != InvalidId && Status.NextTick <= Clock && Status.NextTick <= Status.Expire)
			{
				Due.Add(&Status);
			}
		}
		if (Due.Num() == 0)
		{
			return;
		}
		Due.Sort([](const FDWStatus& A, const FDWStatus& B)
		{
			return A.TargetId != B.TargetId ? A.TargetId < B.TargetId : A.Key < B.Key;
		});
		TMap<uint32, int32> TargetIndex;
		for (int32 Index = 0; Index < TargetData.Num(); ++Index)
		{
			TargetIndex.FindOrAdd(TargetData.ExternalId[Index], Index);
		}
		for (FDWStatus* Status : Due)
		{
			const FDWStatusDef& Def = StatusDefs[StatusDefByKey.FindChecked(Status->Key)];
			for (int32 Count = 0; Count < MaxTicksPerSubstep && Status->NextTick <= Clock && Status->NextTick <= Status->Expire; ++Count)
			{
				Status->NextTick = Status->NextTick + Def.TickInterval;
				const int32* Target = TargetIndex.Find(Status->TargetId);
				if (!Target || TargetData.Health[*Target] <= 0.f)
				{
					continue;
				}
				const float Amount = StatusMagnitude(*Status) * Def.TickInterval;
				if (!(Amount > 0.f))
				{
					continue;
				}
				const uint8 Element = uint8(Def.TickElement);
				const float Effective = EffectiveResistance(*Target, Element, 0.f);
				const float Dealt = ApplyDamageTaken(*Target, FMath::Max(0.f, Amount * (1.f - Effective / 100.f)));
				float& Health = TargetData.Health[*Target];
				const float Applied = FMath::Min(Health, Dealt);
				Health -= Dealt;
				const bool bKilled = Health <= 0.f;
				FDWDamageRecord Record;
				Record.TargetExternalId = Status->TargetId;
				Record.OwnerExternalId = Owners.IsValidIndex(Status->Owner) ? Owners[Status->Owner].ExternalId : InvalidId;
				Record.Amount = Applied;
				Record.Element = Element;
				Record.bDot = true;
				Record.bCritical = false;
				Record.bKilled = bKilled;
				Damage.Add(Record);
				++FrameStats.Hits;
				FrameStats.Kills += bKilled ? 1 : 0;
			}
		}
	}

	void FDWRuleWorld::ClearTargetStatuses(uint32 TargetExternalId)
	{
		if (const auto* Keys=TargetStateKeys.Find(TargetExternalId)) for (uint64 Key : *Keys) TargetState.Remove(Key);
		TargetStateKeys.Remove(TargetExternalId);
		for (auto It = Statuses.CreateIterator(); It; ++It)
		{
			if (It.Value().TargetId == TargetExternalId)
			{
				It.RemoveCurrent();
			}
		}
	}

	void FDWRuleWorld::PruneStatuses()
	{
		for (auto It = Statuses.CreateIterator(); It; ++It)
		{
			if (It.Value().Expire <= Clock)
			{
				It.RemoveCurrent();
			}
		}
	}

	bool FDWRuleWorld::RunApplyStatus(const FDWActionDef& Action, FContext& Context)
	{
		// origin "owner": a buff on the caster itself (potions, on-kill frenzy); otherwise the event target.
		const uint32 TargetId = Action.Origin == EOrigin::Owner ? Owners[Context.Root.Owner].StatusId : Context.Event.TargetId;
		if (TargetId == InvalidId)
		{
			return true;
		}
		float Value = SlotOr(Action, ESlot::Value, Context, 0.f);
		if (Action.Slot(ESlot::Cap).bSet)
		{
			Value = FMath::Min(Value, Eval(Action.Slot(ESlot::Cap), Context));
		}
		const float Duration = SlotOr(Action, ESlot::Duration, Context, 0.f);
		ApplyStatusKey(TargetId, Action.StateKey, Value, Duration, Context.Root.Owner);
		return true;
	}

	bool FDWRuleWorld::RunSummon(const FDWActionDef& Action, FContext& Context)
	{
		const float Duration = SlotOr(Action, ESlot::Duration, Context, 0.f);
		const float Interval = SlotOr(Action, ESlot::Interval, Context, 0.f);
		const float Range = SlotOr(Action, ESlot::Radius, Context, 0.f);
		if (!FMath::IsFinite(Duration) || !FMath::IsFinite(Interval) || !FMath::IsFinite(Range) || Duration <= 0.f || Interval <= 0.f || Range <= 0.f) return true;
		const FName Kind = Context.Plan.KindNames[Action.Kind];
		int32 Cap = FMath::Clamp(FMath::TruncToInt(SlotOr(Action, ESlot::Cap, Context, 1.f)), 1, 32);
		if (Extension) Cap = Extension->SummonCap(Cap, Action.Kind, Context.RootIndex);
		if (Cap <= 0) return true;
		TOptional<FDWSummon> Replaced;
		int32 Owned = 0;
		float NextFire = Clock;
		bool bInheritedFire = false;
		for (const FDWSummon& Summon : Summons)
		{
            if (!Summon.bDead && Roots[Summon.Root].Owner == Context.Root.Owner && Plans[Roots[Summon.Root].Plan]->KindNames[Summon.Kind] == Kind)
            {
                ++Owned;
            }
		}
		for (FDWSummon& Summon : Summons)
		{
			if (Owned < Cap) break;
			if (!Summon.bDead && Roots[Summon.Root].Owner == Context.Root.Owner && Plans[Roots[Summon.Root].Plan]->KindNames[Summon.Kind] == Kind)
			{
				if (!bInheritedFire && Summon.Expire > Clock)
				{
					NextFire = FMath::Max(Clock, Summon.NextFire);
					bInheritedFire = true;
				}
				Summon.bDead = true;
				Summon.bReplaced = true;
				if (!Replaced.IsSet()) Replaced = Summon;
				if (Extension) Extension->SummonEnd(Summon);
				--Roots[Summon.Root].Live;
				--Owned;
			}
		}
		Summons.RemoveAll([](const FDWSummon& Summon) { return Summon.bDead; });
		if (Summons.Num() >= 128)
		{
			++FrameStats.CapacityDrops;
			return true;
		}
		const FDWOwner& Owner = Owners[Context.Root.Owner];
		FDWSummon Summon;
		Summon.Id = NextSummonId++;
		Summon.Root = Context.RootIndex;
		Summon.Kind = Action.Kind;
		Summon.Event = Action.Event;
		Summon.Position = Action.Origin == EOrigin::Owner ? Owner.Position : Context.Event.Position;
		Summon.Direction = FlatDirection(Context.Event.Direction, Owner.Aim);
		Summon.Range = FMath::Min(1000.f, Range);
		Summon.Interval = FMath::Max(0.05f, Interval);
		Summon.Expire = Clock + FMath::Min(60.f, Duration);
        Summon.NextFire = NextFire;
		Summon.Power = Context.Event.Amount;
        Summon.bStorm = Action.Op == EOp::Storm;
        Summon.Tags = Summon.bStorm ? Action.AddTags : (Context.Event.Tags & ~NonInheritedTags) | Action.AddTags;
		const int32 SummonIndex = Summons.Add(Summon);
		++Context.Root.Live;
		if (Extension) Extension->Summon(Summons[SummonIndex], Context.RootIndex, Replaced.IsSet() ? &Replaced.GetValue() : nullptr);
		return true;
	}

	bool FDWRuleWorld::ApplyStatusKey(uint32 TargetId, uint64 Key, float Value, float Duration, int32 Owner)
	{
		if (Extension && !Extension->ModifyStatus(TargetId, Key, Value, Duration, Owner)) return true;
		if (TargetId == InvalidId || !(Value > 0.f) || !(Duration > 0.f))
		{
			return false;
		}
		FDWStatus& Status = Statuses.FindOrAdd(StatusSlot(TargetId, Key));
		const int32* DefIndex = StatusDefByKey.Find(Key);
		const FDWStatusDef* Def = DefIndex ? &StatusDefs[*DefIndex] : nullptr;
		const bool bLive = Status.TargetId == TargetId && Status.Expire > Clock;
		const float Until = Clock + Duration;
		if (!bLive)
		{
			Status.Value = Value;
			Status.Expire = Until;
			Status.Stacks = 1;
			Status.NextTick = Def && Def->TickInterval > 0.f ? Clock + Def->TickInterval : 0.f;
		}
		else
		{
			switch (Def ? Def->Stacking : EStatusStacking::Strongest)
			{
			case EStatusStacking::Refresh:
				Status.Value = Value;
				Status.Expire = Until;
				break;
			case EStatusStacking::Stack:
				Status.Stacks = FMath::Min(Def->MaxStacks, Status.Stacks + 1);
				Status.Value = FMath::Max(Status.Value, Value);
				Status.Expire = FMath::Max(Status.Expire, Until);
				break;
			default:
				// Same-name statuses do not stack: strongest magnitude, longest remaining duration.
				Status.Value = FMath::Max(Status.Value, Value);
				Status.Expire = FMath::Max(Status.Expire, Until);
				break;
			}
		}
		Status.TargetId = TargetId;
		Status.Key = Key;
		Status.Owner = Owner;
		++FrameStats.StatusApplied;
		return true;
	}

	void FDWRuleWorld::GetGrantedTraits(uint32 StatusId, TArray<FName>& Out) const
	{
		Out.Reset();
		if (!bTraitStatuses || StatusId == InvalidId)
		{
			return;
		}
		for (const FDWStatusDef& Def : StatusDefs)
		{
			if (Def.Traits.Num() == 0 || !(GetStatus(StatusId, Def.Key) > 0.f))
			{
				continue;
			}
			for (FName Id : Def.Traits)
			{
				Out.AddUnique(Id);
			}
		}
	}

	void FDWRuleWorld::GetIdsWithGrantedTraits(TSet<uint32>& Out) const
	{
		Out.Reset();
		if (!bTraitStatuses)
		{
			return;
		}
		for (const TPair<uint64, FDWStatus>& Pair : Statuses)
		{
			const FDWStatus& Status = Pair.Value;
			const int32* Def = StatusDefByKey.Find(Status.Key);
			if (Def && StatusDefs[*Def].Traits.Num() > 0 && Status.TargetId != InvalidId && Status.Expire > Clock)
			{
				Out.Add(Status.TargetId);
			}
		}
	}
}
