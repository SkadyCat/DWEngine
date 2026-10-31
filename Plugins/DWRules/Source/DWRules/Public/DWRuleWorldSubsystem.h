#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityHandle.h"
#include "DWRuleWorld.h"
#include "DWRuleProgram.h"
#include "DWRuleProgramReader.h"
#include "DWRuleHost.h"
#include "DWRuleAssets.h"
#include "DWRuleSession.h"
#include "DWRuleProfile.h"
#include "DWRuleFloorRewards.h"
#include "DWRuleHUDDps.h"
#include "DWRuleWorldSubsystem.generated.h"

class UDWRuleWorldConfig;
class UDWTraitDefinition;
class UDWRuleTargetComponent;
class UStaticMesh;
class UMaterialInterface;
class ADWRuleVisualHost;
struct FMassEntityManager;
struct FDWRuleTargetFragment;

USTRUCT(BlueprintType)
struct DWRULES_API FDWRuleDamageRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rules")
	int32 TargetId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rules")
	int32 OwnerId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rules")
	float Amount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Rules")
	int32 Element = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rules")
	bool bCritical = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rules")
	bool bKilled = false;
	// Character resources were resolved natively. Script consumers mirror these pools.
	UPROPERTY(BlueprintReadOnly, Category = "Rules") bool bCharacterResolved = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float Health = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float Shield = 0;
};

USTRUCT(BlueprintType)
struct DWRULES_API FDWRulePlayerPools
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Rules") bool bActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 TargetId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") bool bDead = false;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float Life = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float MaxLife = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float Shield = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float MaxShield = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float Mana = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float MaxMana = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float MoveScale = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float ActionScale = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float Cooldown = 0;
};

USTRUCT(BlueprintType)
struct DWRULES_API FDWRuleFrameReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Projectiles = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Targets = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Contacts = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Hits = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Events = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Actions = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Deferred = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 DroppedEvents = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 BudgetStops = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 CapacityDrops = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 StatusApplied = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 Statuses = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float SimulationMs = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float MotionMs = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float RulesMs = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") float RenderMs = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Rules") int32 MonsterCasts = 0;
};

// Presentation-only receipt of successful runtime actions (newest first).
struct FDWRuleHUDLog
{
 double Time = 0;
 FString Text;
};

// Bounded, non-consuming presentation receipts from real committed events. No profile/RNG ownership.
struct FDWRuleHUDReceipt
{
 uint64 Serial = 0;
 double Time = 0, Amount = 0;
 FVector Position = FVector::ZeroVector; // world centimeters; XY captured at the event
 uint8 Element = 0;
 bool bCritical = false, bHero = false, bStones = false, bPlayerDamage = false;
 uint32 TargetId = 0;
};

// Per-world owner of the rule runtime. Driven once per frame by UDWRuleFrameProcessor (Mass,
// PostPhysics). Lua/Blueprint only compile builds, register owners, cast and drain summaries;
// no per-projectile or per-hit callback ever leaves native code.
UCLASS()
class DWRULES_API UDWRuleWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UFUNCTION(BlueprintCallable, Category = "Rules", meta = (WorldContext = "Context"))
	static UDWRuleWorldSubsystem* ForContext(const UObject* Context);

	// Applies budgets/visuals. Recreates the runtime, so call before casting.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void Configure(UDWRuleWorldConfig* InConfig);

	// Returns a plan index (cached by build hash) or -1 with Error set.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	int32 CompileBuild(FName ActiveId, const TArray<UDWTraitDefinition*>& Traits, FString& Error);

	UFUNCTION(BlueprintCallable, Category = "Rules")
	int32 CompileSampleBuild(FString& Error);

	// Loads every *.json under Dir/Statuses, Dir/Traits and Dir/Builds (Dir relative to the project Content
	// directory, e.g. "Rules"). Replaces the library; compiled plans stay valid.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	bool LoadRuleLibrary(const FString& Dir, FString& Error);

	// Composes library traits into one plan; the first id is the active trait unless ActiveId is set.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	int32 CompileTraits(FName ActiveId, const TArray<FName>& TraitIds, FString& Error);

	// Compiles a library build (skill) plus extra trait ids (supports, affixes, talents).
	UFUNCTION(BlueprintCallable, Category = "Rules")
	int32 CompileNamedBuild(FName BuildId, const TArray<FName>& ExtraTraits, FString& Error);

	// Compile resistance traits and bind them to an Actor, Mass or static target id.
	// An empty list removes the defense traits. The rule library must already be loaded.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	bool SetTargetDefenseTraits(int32 TargetId, const TArray<FName>& TraitIds, FString& Error);

	// Resolved build stat, e.g. "cast.speed_percent" or "element.cold.damage_percent".
	UFUNCTION(BlueprintPure, Category = "Rules")
	float GetPlanStat(int32 Plan, const FString& Stat) const;

	UFUNCTION(BlueprintPure, Category = "Rules")
	FString GetTraitDisplayName(FName TraitId) const;

	UFUNCTION(BlueprintPure, Category = "Rules")
	FString GetBuildDisplayName(FName BuildId) const;

	const FDWRuleBuildDef* FindBuild(FName BuildId) const
	{
		return Host ? Host->Library.Builds.Find(BuildId) : nullptr;
	}

	// Live status magnitude (e.g. chill slow percent) on a rule target id; 0 when absent.
	UFUNCTION(BlueprintPure, Category = "Rules")
	float GetStatus(int32 TargetId, FName Status) const;

	float GetStatusByKey(uint32 TargetId, uint64 Key) const;

	// Accepts a game entity id (or an internal id) and returns the internal target id.
	uint32 ResolveTargetId(int32 Id) const;

	// Runtime visual for a carrier kind (no config asset needed).
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void SetVisual(FName Kind, UStaticMesh* Mesh, UMaterialInterface* Material, FVector ScalePerMeter);

	void RegisterActorTarget(UDWRuleTargetComponent* Target);
	void UnregisterActorTarget(UDWRuleTargetComponent* Target);

	static uint32 MassTargetId(const FMassEntityHandle& Entity)
	{
		return uint32(Entity.Index) + 1u;
	}

	// A Mass entity was destroyed or spawned: forget statuses keyed by its recycled id.
	void ForgetTarget(uint32 TargetId);
	UFUNCTION(BlueprintCallable, Category = "Rules") void ForgetGameTarget(int32 TargetId);

	UFUNCTION(BlueprintCallable, Category = "Rules")
	int32 RegisterOwner(int32 ExternalId, int32 Team);

	UFUNCTION(BlueprintCallable, Category = "Rules")
	void UpdateOwner(int32 Owner, FVector Location, FVector Aim);
	void SetOwnerTargetPoint(int32 Owner, FVector Point);

	// World-space centimeters. Returns the cast root or -1.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	int32 CastBuild(int32 Plan, int32 Owner, FVector Origin, FVector Direction);

	// Cast with a power value (weapon/monster damage) exposed to rules as {"event":"amount"}.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	int32 CastBuildPower(int32 Plan, int32 Owner, FVector Origin, FVector Direction, float Power);

	UFUNCTION(BlueprintCallable, Category = "Rules")
	TArray<FDWRuleDamageRow> DrainDamage();

	// Seconds left on a status for a game entity id; 0 when absent.
	UFUNCTION(BlueprintPure, Category = "Rules")
	float GetStatusRemaining(int32 TargetId, FName Status) const;

	// Summed status effect on a target (move_speed, action_speed, damage_taken, damage_dealt,
	// critical_chance, damage_over_time; control returns 1 freeze / 2 stun) and its longest remaining time.
	UFUNCTION(BlueprintPure, Category = "Rules")
	float GetStatusEffect(int32 TargetId, FName Effect, float& Remaining) const;

	UFUNCTION(BlueprintPure, Category = "Rules")
	int32 GetStatusStacks(int32 TargetId, FName Status) const;

	// Timed status on any target (item use, scripted effects). Statuses with "traits" grant those
	// traits while live: the holder's casts and defense include them, removed on expiry.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	bool ApplyStatus(int32 TargetId, FName Status, float Value, float Duration);

	// Makes any Actor hittable (players, mercenaries, actor monsters). Creates the target component
	// on first use; call whenever authoritative health changes. Health <= 0 makes it untargetable.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	void UpdateActorTarget(AActor* Actor, int32 ExternalId, int32 Team, float Health, float MaxHealth, float RadiusCm);

	UFUNCTION(BlueprintCallable, Category = "Rules")
	FDWRuleFrameReport GetFrameReport() const
	{
		return Report;
	}

	// Non-Mass targets (tests, dummies). Ids are returned as external ids in damage rows.
	UFUNCTION(BlueprintCallable, Category = "Rules")
	int32 AddStaticTarget(FVector Location, float RadiusCm, float Health, int32 Team);

	UFUNCTION(BlueprintCallable, Category = "Rules")
	void ClearStaticTargets();

	int32 CompileSource(const DWRules::FDWBuildSource& Source, FString& Error);
	// One local hero, matching RuleStudio Battle. Empty active slots stay empty.
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool EnablePlayer(int32 Owner, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool ImportPlayerState(const FString& Json, FString& Error);
	UFUNCTION(BlueprintPure, Category = "Rules|Player") FString ExportPlayerState() const;
	UFUNCTION(BlueprintPure, Category = "Rules|Player") FDWRulePlayerPools GetPlayerPools() const;
	UFUNCTION(BlueprintPure, Category = "Rules") FDWRulePlayerPools GetTargetPools(int32 TargetId) const;
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool ChangePlayerSkill(int32 Slot, FName Skill, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool ChangePlayerGem(FName Skill, int32 Slot, FName Gem, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool EquipPlayerItem(FName Slot, FName Item, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool BuyPlayerListing(const FString& Key, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool IdentifyPlayerEquipment(FName Item, FString& Uid, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool CraftPlayerEquipment(const FString& Uid, FName Currency, FName Omen, int32 Index, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Events") bool LockPlayerEventItem(const FString& Uid,const FString& Token,FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Events") bool UnlockPlayerEventItem(const FString& Uid,const FString& Token,FString& Error);
	UFUNCTION(BlueprintPure, Category = "Rules|Events") bool IsPlayerItemLocked(const FString& Uid) const;
	UFUNCTION(BlueprintCallable, Category = "Rules|Events") bool CraftPlayerEventItem(const FString& Uid,const FString& Token,FName Currency,int32 Index,FString& Error);
	bool CommitPlayerEventItems(const TArray<FString>& Consumed,const TArray<DWRules::FDWEquipmentInstance>& Granted,const FString& Token,FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool DiscardPlayerEquipment(const FString& Uid, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool SelectPlayerSkill(int32 Slot, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool UsePlayerItem(FName Item, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool StartPlayerProgression(FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") void RespawnPlayer();
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") void SetPlayerGod(bool Enabled);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") void SetPlayerCasting(bool Casting);
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool DodgePlayer();
	UFUNCTION(BlueprintCallable, Category = "Rules|Player") bool SacrificePlayerMinion();
	UFUNCTION(BlueprintPure, Category = "Rules|Monsters") TArray<FName> GetMonsterTypes() const;
	UFUNCTION(BlueprintCallable, Category = "Rules|Monsters") int32 RegisterMonster(int32 TargetId, FName Type, FVector Location, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Monsters") bool SetMonsterTraits(int32 TargetId, const FString& EntriesJson, FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Rules|Monsters") bool TickMonsterSkill(int32 TargetId, FVector Location, FVector TargetLocation, float DeltaSeconds, bool TargetAlive);
	UFUNCTION(BlueprintPure, Category = "Rules|Monsters") float GetMonsterMoveScale(int32 TargetId) const;
	// Meters. Legacy definitions without a range trait use the caller's configured default.
	UFUNCTION(BlueprintPure, Category="Rules|Monsters")
	float GetMonsterSeekRange(int32 TargetId, float DefaultMeters = 8.f) const;
 UFUNCTION(BlueprintCallable, Category="Rules|Player") bool SetPlayerBeltSlot(int32 Slot, FName Id, FString& Error);
 UFUNCTION(BlueprintCallable, Category="Rules|Player") bool SellPlayerEquipment(const FString& Id, int64& Price, FString& Error);
 UFUNCTION(BlueprintPure, Category="Rules|Player") bool QuotePlayerEquipmentSale(const FString& Id, int64& Price, FString& Error) const;
 UFUNCTION(BlueprintCallable, Category="Rules|Player") bool DropPlayerInventory(FName Kind,const FString& Id,int32 Count,FVector Position,FString& Key,FString& Error);
 void BeginPlayerFloorRewards(int32 Seed,int32 Floor,int32 Visit=1,int32 MaxTier=1);
 // Host-supplied catalogue view. The rule engine and its RNG remain unchanged.
 bool SetPlayerFloorDropPool(const TArray<FName>& Equipment,const TArray<FName>& Items,FString& Error);
 void CompletePlayerFloorRewards();
 const DWRules::FDWRuleFloorRewards* PlayerFloorRewards() const { return FloorRewards.Get(); }
 const TArray<FDWRuleHUDLog>& PlayerHUDLog() const { return HUDLog; }
 const TArray<FDWRuleHUDReceipt>& PlayerHUDReceipts() const { return HUDReceipts; }
 void PlayerHUDDps(double AdditionalTime, double& Value, int32& Targets) const;
 double GetStatusValue(int32 TargetId, FName Status) const;
 bool GetMonsterHUD(int32 TargetId, FString& Name, FString& Skill, double& LastCastAt) const;
 bool PickupPlayerLoot(const FString& Key,FVector Position,FString& Error);
 bool BuyPlayerMerchant(const FString& Key,FString& Error);
	// Game hosts may supply encounter variants without changing the shared authored definition.
	int32 RegisterMonsterDefinition(int32 TargetId, const DWRules::FDWMonsterDefinition& Definition, FVector Location, FString& Error, float RadiusCm = 0);
	bool ApplyPlayerPreset(const FString &Json, FString &Error);
	bool SetMonsterHitPower(int32 TargetId, float Power, FString& Error);
	const DWRules::FDWRuleProfile& PlayerInventory() const { return PlayerProfile; }
 // UI placement metadata; ownership remains exclusively managed by the existing transactions.
 DWRules::FDWInventoryLayout& PlayerInventoryLayout() { return PlayerProfile.BagLayout; }
	void SetSimulationEnabled(bool Enabled) { bSimulationEnabled = Enabled; }
	const DWRules::FDWRuleLibrary& Library() const { return Host->Library; }
	bool SetPlayerPassiveParts(const TArray<DWRules::FDWCollectionPart>& Parts, FString& Error);
	DWRules::FDWRuleSession* PlayerSession() { return Player.Get(); }
	const DWRules::FDWCombatEnemy* CombatEnemy(uint32 Id) const;

	// Mass frame driver interface.
	void BeginTargets();
	void AddMassTarget(const FMassEntityHandle& Entity, const FVector& Location, const FDWRuleTargetFragment& Target);
	void StepFrame(float DeltaSeconds, FMassEntityManager* EntityManager);

	DWRules::FDWRuleWorld& RuleWorld()
	{
		return *Runtime;
	}

private:
	struct FStaticTarget
	{
		FVector3f Position;
		float Radius;
		float Health;
		float MaxHealth;
		uint8 Team;
	};

	void CreateRuntime();
	void SyncVisuals();
	bool ApplyPlayerProfile(DWRules::FDWRuleProfile Next, FString& Error);
	int32 RegisterCompiledMonster(int32 TargetId, const DWRules::FDWCompiledMonster& Compiled, FVector Location, FString& Error, float RadiusCm = 0);
	void ExportCombatTarget(uint32 Id, const FVector3f& Position, float Radius, float Health, float MaxHealth, float Shield, float MaxShield, uint8 Team, bool Elite, const float* Resistance);
	void SyncPlayerTarget();

	TUniquePtr<DWRules::FDWRuleHost> Host;
	TUniquePtr<DWRules::FDWRuleSession> Player;
	DWRules::FDWRuleProfile PlayerProfile;
	TMap<FString,FString> PlayerEventLocks;
	TUniquePtr<DWRules::FDWRuleFloorRewards> FloorRewards;
	TUniquePtr<DWRules::FDWRuleLibrary> FloorDropLibrary;
	struct FMonsterState
	{
		DWRules::FDWCompiledMonster Compiled;
		DWRules::FDWMonsterCooldowns Cooldowns;
		DWRules::FDWRuleCharacter Character;
		int32 Owner = INDEX_NONE;
		double BaseHealth = 0, BaseShield = 0;
		FString LastSkill;
		double LastCastAt = -1.e30;
	};
	TMap<uint32, FMonsterState> MonsterStates;
	TMap<uint32, DWRules::FDWCombatEnemy> FrameEnemies;
	TArray<uint32> FrameEnemyIds;
	TMap<FName, DWRules::FDWCompiledMonster> MonsterPlans;
	DWRules::FDWRuleWorld* Runtime = nullptr;

	UPROPERTY()
	TObjectPtr<UDWRuleWorldConfig> Config;

	UPROPERTY()
	TObjectPtr<ADWRuleVisualHost> VisualHost;

	TMap<uint32, FMassEntityHandle> MassTargets;
	// Internal target id <-> game entity id (Lua EntityID), rebuilt every frame.
	TMap<uint32, int32> GameIdByTarget;
	TMap<int32, uint32> TargetByGameId;
	void MapGameId(uint32 Internal, int32 GameId);
	TArray<FStaticTarget> StaticTargets;
	TArray<TWeakObjectPtr<UDWRuleTargetComponent>> ActorTargets;
	int32 PlanWithGrantedTraits(int32 Plan, int32 Owner);
	void RefreshGrantedDefense();
	TArray<FDWRuleDamageRow> PendingDamage;
	FDWRuleFrameReport Report;
	uint64 CastCounter = 0;
	int32 MonsterCastCounter = 0;
	uint64 LastFrame = MAX_uint64;
	bool bSimulationEnabled = true;
	TArray<FDWRuleHUDLog> HUDLog;
 TArray<FDWRuleHUDReceipt> HUDReceipts;
 FDWRuleHUDDps HUDDamageMeter;
 uint64 HUDReceiptSerial = 0;
 void AddHUDReceipt(FDWRuleHUDReceipt Receipt);
	double LastMonsterHUDLogAt = -1.e30;
	void AddHUDLog(const FString& Text);
};
