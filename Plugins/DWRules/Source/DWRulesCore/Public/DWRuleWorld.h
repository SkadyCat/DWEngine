#pragma once

#include "DWRulePlan.h"
#include "DWRuleCollection.h"
#include "DWRuleWorldExtension.h"
#include "Containers/ChunkedArray.h"

// Runtime: SoA carriers, host-filled targets, wave-based rule VM. Single owner thread;
// only the motion/sweep phase fans out with ParallelFor. Deterministic for equal inputs.
namespace DWRules
{
	struct FDWWorldConfig
	{
		int32 MaxProjectiles = 4096;
		// Rule actions executed per world frame before remaining events are deferred.
		int32 WorkPerFrame = 20000;
		int32 RootWorkCap = 16384;
		int32 MaxWaves = 8;
		int32 MaxDeferredEvents = 65536;
		float FixedStep = 1.f / 60.f;
		int32 MaxSubsteps = 2;
		float CellSize = 3.f;
		bool bParallel = true;
		int32 ParallelChunk = 128;
	};

	// Host writes targets every frame (meters). Health is a mirror: the world decrements it
	// and reports FDWDamageRecord rows that the host commits to its authoritative store.
	struct DWRULESCORE_API FDWTargets
	{
		TArray<FVector3f> Position;
		TArray<float> Radius;
		TArray<float> Health;
		TArray<uint8> Team;
		TArray<uint32> ExternalId;
		// Four values per target: fire, frost, lightning, poison.
		TArray<float> Resistance;

		void Reset();
		int32 Add(const FVector3f& InPosition, float InRadius, float InHealth, uint8 InTeam, uint32 InExternalId,
			const float* InResistance = nullptr);

		int32 Num() const
		{
			return Position.Num();
		}
	};

	struct FDWOwner
	{
		FVector3f Position = FVector3f::ZeroVector;
		FVector3f Aim = FVector3f(1.f, 0.f, 0.f);
		TOptional<FVector3f> TargetPoint;
		uint32 ExternalId = InvalidId;
		// Id under which statuses applied to / read from this owner live (its own target id when
		// it is also a target). Defaults to ExternalId.
		uint32 StatusId = InvalidId;
		uint8 Team = 0;
		int32 LiveProjectiles = 0;
	};

	struct FDWDamageRecord
	{
		uint32 TargetExternalId = InvalidId;
		uint32 OwnerExternalId = InvalidId;
		float Amount = 0.f;
		uint8 Element = 0;
		bool bCritical = false;
		bool bKilled = false;
		bool bDot = false;
		FDWDamageInfo Info;
	};

	struct FDWStatus
	{
		uint32 TargetId = InvalidId;
		uint64 Key = 0;
		float Value = 0.f;
		float Expire = 0.f;
		int32 Stacks = 0;
		int32 Owner = INDEX_NONE;
		float NextTick = 0.f;
	};

	struct FDWStatusEffectDef
	{
		EStatusEffect Type = EStatusEffect::DamageTaken;
		float Scale = 1.f;
		int8 Element = -1;
		float Interval = 0.f;
		EStatusControl Control = EStatusControl::None;
	};

	// One Content/Rules/Statuses/<id>.json. Statuses without a definition keep the default
	// behaviour: strongest magnitude, longest duration, no runtime effect.
	struct FDWStatusDef
	{
		FName Id;
		FString DisplayName, Description;
		FName Category = TEXT("debuff");
		uint64 Key = 0;
		EStatusStacking Stacking = EStatusStacking::Strongest;
		int32 MaxStacks = 1;
		// 0 = uncapped.
		float Cap = 0.f;
		TArray<FDWStatusEffectDef> Effects;
		// Traits the holder has while the status is live (timed buffs, debuffs, temporary states).
		// Resolved by the host: casts of the holder and its defense plan include them.
		TArray<FName> Traits;
		float TickInterval = 0.f;
		int8 TickElement = -1;
		EStatusControl Control = EStatusControl::None;
	};

	struct FDWFrameStats
	{
		int32 Substeps = 0;
		int32 Projectiles = 0;
		int32 Contacts = 0;
		int32 Hits = 0;
		int32 Kills = 0;
		int32 Events = 0;
		int32 Actions = 0;
		int32 Waves = 0;
		int32 Deferred = 0;
		int32 DroppedEvents = 0;
		int32 BudgetStops = 0;
		int32 DepthBlocked = 0;
		int32 ChainBlocked = 0;
		int32 CapacityDrops = 0;
		int32 StatusApplied = 0;
		int32 Statuses = 0;
		double MotionMs = 0.0;
		double RulesMs = 0.0;
		double TotalMs = 0.0;
	};

	struct FDWHitSet
	{
		static constexpr int32 Inline = 6;
		uint32 Ids[Inline];
		uint8 Num = 0;
		bool bOverflow = false;
	};

	struct DWRULESCORE_API FDWProjectiles
	{
		enum EFlag : uint8
		{
			Dead = 1 << 0,
			PendingImpact = 1 << 1,
			Explicit = 1 << 2,
			Parabolic = 1 << 3
		};

		TArray<uint32> Id;
		TArray<uint32> Root;
		TArray<uint32> Owner;
		TArray<FVector3f> Position;
		TArray<FVector3f> Velocity;
		TArray<float> Gravity;
		TArray<float> Age;
		TArray<float> Life;
		TArray<float> Radius;
		TArray<float> Damage;
		TArray<uint16> Pierce;
		TArray<uint16> Ricochet;
		TArray<uint16> Reflect;
		TArray<uint16> Return;
		TArray<uint16> Refract;
		TArray<uint16> Kind;
		TArray<uint16> EndEvent;
		TArray<uint8> Generation;
		TArray<uint32> Lineage;
		TArray<uint8> Element;
		TArray<uint8> Flags;
		TArray<uint64> Tags;
		TArray<FDWHitSet> Hits;
		int32 Num = 0;

		void Reserve(int32 Capacity);
		int32 Allocate(uint32 NewId);
		void MoveSlot(int32 From, int32 To);
	};

	struct FDWEvent
	{
		uint16 Type = 0;
		uint8 Generation = 0;
		uint8 Element = 1;
		uint32 Root = 0;
		uint32 Sequence = 0;
		uint32 Lineage = 0;
		int32 SourceIndex = INDEX_NONE;
		uint32 SourceId = InvalidId;
		int32 TargetIndex = INDEX_NONE;
		uint32 TargetId = InvalidId;
		FVector3f Position = FVector3f::ZeroVector;
		FVector3f Direction = FVector3f::ZeroVector;
		float Amount = 0.f;
		float Damage = 0.f;
		float Stored = 0.f;
		uint64 Tags = 0;
	};

	struct FDWRoot
	{
		int32 Plan = INDEX_NONE;
		int32 Owner = INDEX_NONE;
		uint64 Seed = 0;
		uint32 NextSequence = 1;
		int32 Live = 0;
		int32 Pending = 0;
		int32 Work = 0;
		bool bActive = false;
		uint64 Identity = 0;
        float DamageMultiplier = 1.f;
		TOptional<FVector3f> TargetPoint;
		TArray<uint16> ChainCount;
		TMap<uint64, float> CastState;
		TSet<uint64> OverflowHits;
	};

	// A stationary timed emitter, independent of projectile collision and hit history.
	struct FDWSummon
	{
		uint32 Id = 0;
		FDWPawnEquipment Equipment{{{FName(TEXT("default")), NAME_None}}, {}};
		int32 Root = INDEX_NONE;
		uint16 Kind = 0;
		uint16 Event = 0;
		FVector3f Position = FVector3f::ZeroVector;
		FVector3f Direction = FVector3f(1.f, 0.f, 0.f);
		float Range = 0.f;
		float Interval = 1.f;
		float Expire = 0.f;
		float NextFire = 0.f;
		float Power = 0.f;
		uint64 Tags = 0;
		bool bDead = false;
        bool bStorm = false;
		bool bHold = false, bReplaced = false;
	};

    // Immutable presentation snapshot; querying it never advances or changes the VM.
    struct FDWMaintainedView { uint64 Key=0; int32 Root=INDEX_NONE,Stacks=0,Cap=0; double Expire=0,DecayInterval=0; };
	class DWRULESCORE_API FDWRuleWorld
	{
	public:
		explicit FDWRuleWorld(const FDWWorldConfig& InConfig = FDWWorldConfig());
		// Native host callback. The VM emits typed requests; lifetime/composition stays outside the VM.
		TFunction<void(uint32, int32, float, const FDWGrantDef&)> OnGrantTraits;
		IDWRuleWorldExtension* Extension = nullptr;
		// Optional host geometry sweep. Called serially before worker target sweeps; times are 0..1.
		TFunction<bool(const FVector3f&, const FVector3f&, float, float&)> SweepGeometry;
		// Optional host surface projection. Core-only simulations retain a flat Z=0 floor.
		TFunction<bool(const FVector3f&, FVector3f&, FVector3f&)> ProjectGround;

		int32 AddPlan(const TSharedRef<const FDWCompiledPlan>& Plan);
		int32 AddOwner(uint32 ExternalId, uint8 Team);
		void RetireOwner(int32 Owner);
		void UpdateOwner(int32 Owner, const FVector3f& Position, const FVector3f& Aim);
		void SetOwnerTargetPoint(int32 Owner, const FVector3f &Point);
		// Returns the root index, or INDEX_NONE when the plan/owner is invalid.
		// Power is the cast event's amount (e.g. weapon or monster damage), readable as {"event":"amount"}.
		int32 Cast(int32 Plan, int32 Owner, const FVector3f &Origin, const FVector3f &Direction, uint64 Seed,
				   float Power = 0.f, TOptional<FVector3f> TargetPoint = {});
		int32 OpenRoot(int32 Plan, int32 Owner, uint64 Seed);
		void ReleaseHeldRoot(int32 Root);
		int32 TargetIndexOf(uint32 Id) const;
		FDWDamageResult HostHit(int32 Root, uint32 TargetId, float Amount, uint8 Element,
			const FDWDamageInfo& Info, bool Silent = false, uint8 Generation = 3,
			uint16 EventType = uint16(EBuiltinEvent::Hit), const FDWEvent* Template = nullptr);
		float HostDot(int32 Owner, uint32 TargetId, float Amount, uint8 Element, float Scale = 1);
		void MirrorStatus(uint32 TargetId, uint64 Key, float Value, int32 Stacks, float Remaining, int32 Owner);
		void ExpireStatus(uint32 TargetId, uint64 Key);
		const FDWOwner* GetOwner(int32 Index) const { return Owners.IsValidIndex(Index) ? &Owners[Index] : nullptr; }
		bool IsRootValid(int32 Index) const { return Roots.IsValidIndex(Index) && Roots[Index].bActive; }

		// Bind compiled defense traits to any target external id. INDEX_NONE clears the binding.
		// Bindings survive the per-frame target export and must be cleared on id recycling.
		bool SetTargetDefensePlan(uint32 TargetExternalId, int32 Plan);
		// The HUD reads the exact currently bound defense partition, including temporary grants.
		const FDWStatBlock* TargetDefenseStats(uint32 TargetExternalId) const
		{
			const int32* Plan = TargetDefensePlans.Find(TargetExternalId);
			return Plan && Plans.IsValidIndex(*Plan) ? &Plans[*Plan]->Stats : nullptr;
		}

		FDWTargets& Targets()
		{
			return TargetData;
		}

		void Step(float DeltaSeconds);

		TArray<FDWDamageRecord>& DamageLog()
		{
			return Damage;
		}
		const TArray<FDWDamageRecord>& DamageLog() const { return Damage; }
		const FDWTargets& Targets() const { return TargetData; }

		const FDWProjectiles& Projectiles() const
		{
			return Carriers;
		}

		const FDWFrameStats& GetStats() const
		{
			return FrameStats;
		}

		const TArray<FDWSummon>& GetSummons() const { return Summons; }

		const FDWCompiledPlan& GetPlan(int32 Index) const
		{
			return *Plans[Index];
		}

		const FDWRoot& GetRoot(int32 Index) const
		{
			return Roots[Index];
		}

		int32 ActiveRoots() const;

		// Live magnitude of a status on a target external id (value x stacks, capped), 0 when absent or expired.
		float GetStatus(uint32 TargetExternalId, uint64 Key) const;
		float GetStatusRemaining(uint32 TargetExternalId, uint64 Key) const;
		int32 GetStatusStacks(uint32 TargetExternalId, uint64 Key) const;
		// Host query: summed effect (Control: strongest EStatusControl) over a target's live statuses,
		// and the longest remaining time among them.
		float GetStatusEffect(uint32 TargetExternalId, EStatusEffect Effect, float& OutRemaining) const;
		// Host calls this when an external id is recycled (entity destroyed/respawned).
		void ClearTargetStatuses(uint32 TargetExternalId);
		void SetStatusDefs(TArray<FDWStatusDef> Defs);
		void SetOwnerStatusId(int32 Owner, uint32 StatusId);
		// Host entry (items, scripted effects): apply a timed status by key; same stacking as rules.
		bool ApplyStatusKey(uint32 TargetId, uint64 Key, float Value, float Duration, int32 Owner = INDEX_NONE);
		// Trait ids granted by the live statuses on one id (definition order, duplicates dropped).
		void GetGrantedTraits(uint32 StatusId, TArray<FName>& Out) const;
		// Ids that currently hold at least one trait-granting status.
		void GetIdsWithGrantedTraits(TSet<uint32>& Out) const;
		bool HasTraitStatuses() const
		{
			return bTraitStatuses;
		}
		uint32 GetOwnerStatusId(int32 Owner) const
		{
			return Owners.IsValidIndex(Owner) ? Owners[Owner].StatusId : InvalidId;
		}
		uint32 GetOwnerExternalId(int32 Owner) const
		{
			return Owners.IsValidIndex(Owner) ? Owners[Owner].ExternalId : InvalidId;
		}

		float GetClock() const
		{
			return Clock;
		}
		uint64 StateHash() const;
        void SetInvulnerable(uint32 Id,bool Enabled){if(Enabled)InvulnerableTargets.Add(Id);else InvulnerableTargets.Remove(Id);}
        float MaintainedLayers(int32 Owner,uint64 Key) const;
        void LiveStatusSnapshot(uint32 Target,TArray<FDWStatus>& Out) const;
        void MaintainedSnapshot(int32 Owner,TArray<FDWMaintainedView>& Out) const;

	private:
		friend class FDWCombatRuntime;
		friend class FDWProjectileAffixes;
		friend class FDWMinions;
		friend class FDWZones;
        friend class FDWModernCombat;
		struct FContact
		{
			uint32 ProjectileId;
			int32 Projectile;
			int32 Target;
			float Time;
		};

		struct FContext
		{
			const FDWEvent& Event;
			const FDWCompiledPlan& Plan;
			FDWRoot& Root;
			int32 RootIndex;
			int8 Channel;
			float Stored;
			FName Trait;
		};

		// DWRuleWorld.cpp: frame pipeline, spatial queries, carriers.
		void Substep(float Dt);
		void BuildGrid();
		void MoveAndSweep(float Dt);
		void ResolveContacts();
		void EndPendingImpacts();
		void ResolveWalls();
		void ExpireProjectiles();
		void Compact();
		void ReleaseRoots();
		void ReleaseOwners();
		void TickSummons();
		template <typename FVisit>
		void QueryTargets(const FVector3f& Min, const FVector3f& Max, FVisit&& Visit) const;
		void ForEachTargetInRadius(const FVector3f& Center, float Radius, TFunctionRef<void(int32)> Visit) const;
		int32 NearestTarget(const FVector3f& Center, float Radius, int32 Projectile, const FDWRoot& Root, uint8 Team) const;
		bool IsHostile(int32 Owner, int32 Target) const;
		bool HitContains(int32 Projectile, const FDWRoot& Root, uint32 TargetId) const;
		void HitAdd(int32 Projectile, FDWRoot& Root, uint32 TargetId);
		int32 SpawnProjectile(FDWRoot& Root, int32 RootIndex, const FVector3f& Origin, const FVector3f& Direction,
			float Speed, float Life, float Radius, float Damage, uint8 Element, uint16 Kind, uint64 Tags,
			uint8 Generation, uint32 Lineage);
		void EndProjectile(int32 Index, EEndReason Reason,bool Silent=false);
		void ApplyRefraction(int32 Index, const FVector3f& At);
		float ResolveDamage(FDWRoot& Root, int32 Target, float Amount, uint8 Element, uint32 Sequence,
			bool& bOutCritical, bool& bOutKilled, FDWDamageInfo Info = {}, bool* Missed = nullptr);

		// DWRuleVM.cpp: events, conditions, actions.
		void DrainWaves();
		void ProcessEvent(const FDWEvent& Event);
		bool CheckConditions(const FDWRuleDef& Rule, const FContext& Context) const;
		bool RunActions(int32 First, int32 Count, FContext& Context, int32 Depth);
		bool RunAction(const FDWActionDef& Action, FContext& Context, int32 Depth);
		bool RunEmit(const FDWActionDef& Action, FContext& Context);
		bool RunCopy(const FDWActionDef& Action, FContext& Context);
		bool RunDamageArea(const FDWActionDef& Action, FContext& Context);
		bool RunDamageTarget(const FDWActionDef& Action, FContext& Context);
		bool RunAccumulate(const FDWActionDef& Action, FContext& Context, int32 Depth);
		bool RunConsume(const FDWActionDef& Action, FContext& Context, int32 Depth);
        bool RunConsumeStatus(const FDWActionDef& Action,FContext& Context,int32 Depth);
        bool RunRepeatSkill(FContext& Context);
        bool RunMaintainEmitter(const FDWActionDef& Action,FContext& Context);
        void TickMaintainedEmitters();
		bool RunRedirect(const FDWActionDef& Action, FContext& Context);
		bool RunApplyStatus(const FDWActionDef& Action, FContext& Context);
		bool RunSummon(const FDWActionDef& Action, FContext& Context);
		void PruneStatuses();
		float StatusMagnitude(const FDWStatus& Status) const;
		float EffectSum(uint32 StatusId, EStatusEffect Effect) const;
		float EffectiveResistance(int32 Target, uint8 Element, float Penetration) const;
		float ApplyDamageTaken(int32 Target, float Dealt) const;
		void TickStatuses();
		float Eval(const FDWExprRef& Ref, const FContext& Context) const;
		float SlotOr(const FDWActionDef& Action, ESlot Slot, const FContext& Context, float Default) const;
		float ReadState(EScope Scope, uint64 Key, const FContext& Context) const;
		void WriteState(EScope Scope, uint64 Key, const FContext& Context, float Value);
		bool ResolveSource(const FDWEvent& Event) const;
		FDWEvent MakeChild(const FContext& Context, uint16 Type, uint64 AddTags) const;
		void PushEvent(const FDWEvent& Event);
		void PushHitEvents(FDWRoot& Root, int32 RootIndex, const FDWEvent& Base, int32 Target, float Applied,
			float Requested, bool bCritical, bool bKilled, uint16 HitType);

		FDWWorldConfig Config;
		TArray<TSharedRef<const FDWCompiledPlan>> Plans;
		TArray<FDWOwner> Owners;
		TSet<int32> RetiredOwners;
		TArray<int32> FreeOwners;
		TMap<int32,TSet<uint64>> OwnerStateKeys;
		TMap<uint32,TSet<uint64>> TargetStateKeys;
		// Host policies may open derived roots while a parent action is executing.
		// Chunked storage preserves references held by the VM during that recursion.
		TChunkedArray<FDWRoot> Roots;
		TArray<int32> FreeRoots;
		FDWTargets TargetData;
		FDWProjectiles Carriers;
		TArray<FVector3f> SweepStarts, MotionDelta;
		TArray<float> WallTimes;
		TArray<FDWSummon> Summons;
        struct FMaintainedEmitter
        {
            uint64 StateKey=0,Seed=0,ScheduleSeed=0;
            int32 Owner=INDEX_NONE,Root=INDEX_NONE,Level=0,Cap=10;
            uint32 Pulse=0,Sample=0;
            uint16 Event=0;
            double Duration=5,DecayInterval=2,DecayAt=0,RateAt=0,Hazard=0,Interval=1,NextFire=0,Radius=24,Power=0;
            FDWExprRef Rate;
            uint64 Tags=0;
            FName Targeting;
        };
        TArray<FMaintainedEmitter> MaintainedEmitters;
		uint32 NextSummonId = 1;
		TArray<FDWEvent> Current;
		TArray<FDWEvent> Next;
		TArray<FDWEvent> Deferred;
		TArray<FDWDamageRecord> Damage;
		TMap<uint64, float> OwnerState;
		TMap<uint64, float> TargetState;
		TMap<uint64, FDWStatus> Statuses;
		// Sorted by Key; EffectDefs[e] lists (definition index, scale) in that order.
		TArray<FDWStatusDef> StatusDefs;
		TMap<uint64, int32> StatusDefByKey;
		struct FEffectRef
		{
			int32 Def;
			float Scale;
			EStatusControl Control;
		};
		TArray<FEffectRef> EffectDefs[int32(EStatusEffect::Num)];
		bool bTraitStatuses = false;
		TMap<uint32, int32> TargetDefensePlans;
        TSet<uint32> InvulnerableTargets;
		float Clock = 0.f;
		TMap<uint64, TPair<int32, int32>> Cells;
		TArray<int32> CellOrder;
		float MaxTargetRadius = 0.f;
		TArray<TArray<FContact>> ChunkContacts;
		TArray<FContact> Contacts;
		FDWFrameStats FrameStats;
		uint32 NextProjectileId = 1;
		uint64 NextRootIdentity = 1;
		float Accumulator = 0.f;
		int32 FrameWork = 0;
	};
}
