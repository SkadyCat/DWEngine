#pragma once
#include "DWRuleCharacter.h"
#include "DWRuleHost.h"

namespace DWRules
{
class FDWAilments;
class FDWZones;
class FDWProjectileAffixes;
class FDWMinions;
class FDWCombatTriggers;
class FDWModernCombat;

struct FDWCombatEnemy
{
	uint32 Id = InvalidId;
	int32 Owner = INDEX_NONE;
	FVector3d Position = FVector3d::ZeroVector;
	double Health = 0, MaxHealth = 0, Shield = 0, MaxShield = 0, Radius = .4, SinceHit = 0;
	FName Rarity;
	bool bElite = false;
	float Resistance[NumResistances] = {};
};
struct FDWCombatHero
{
	int32 Owner = INDEX_NONE, Plan = INDEX_NONE;
	FName Skill;
	double Power = 30;
};
struct FDWCombatRoot
{
	uint64 Identity = 0;
	FName Kind = TEXT("cast"), Skill;
	double More = 1, Increased = 0, CriticalBonus = 0;
	int32 Depth = 0, HitCount = 0;
	bool bManual = false, bTriggered = false, bMinion = false;
	uint32 MinionId = 0;
	FString TriggerKey;
	bool bMultiFired = false;
	TMap<uint32, int32> Batch;
    double VolleyTime=-1;
    TSharedPtr<TMap<uint32,int32>> VolleyBatch;
    TMap<int32,TSharedPtr<TMap<uint32,int32>>> RepeatBatches;
	TMap<uint32, int32> Hits;
	TSet<uint32> Victims;
};
struct FDWCombatHit
{
	uint32 Id = InvalidId;
	double Amount = 0;
	uint8 Element = 0;
	bool bCritical = false, bKilled = false, bDot = false;
	bool bProjectile = false, bArea = false, bMinion = false, bTriggered = false, bSilent = false;
	int32 Depth = 0;
	FDWDamageInfo Info;
	FDWCombatRoot *Root = nullptr;
};
struct FDWStrikeOptions
{
	int32 Depth = 1, Plan = INDEX_NONE;
    uint32 MinionId=0;
	bool bSilent = false, bArea = true, bTriggered = false;
	FName Effect = TEXT("strike"), Kind = TEXT("derived");
	TArray<FName> Tags;
	TSet<uint32> Exclude;
	uint32 Only = InvalidId;
	FDWDamageInfo Info;
};
struct FDWStrikeHit : FDWDamageResult
{
	uint32 Id = InvalidId;
};
struct FDWCombatEffect
{
	FName Kind;
	FVector3d Position = FVector3d::ZeroVector;
	double Radius = 0, Amount = 0;
	uint8 Element = 0;
	uint32 From = InvalidId, To = InvalidId;
	FName Shape = TEXT("circle");
	FVector3d End = FVector3d::ZeroVector;
	double Direction = 0, Length = 0, Width = 0, Inner = 0, Angle = 0;
};
struct FDWSkillFamily
{
	bool bArea = false, bProjectile = false, bSummon = false, bFrost = false, bFire = false, bVoid = false,
		 bLightning = false, bPoison = false;
};

struct FDWTemporaryMonsterRequest
{
    FName Kind;
    int32 Count=0;
    double Duration=0,Radius=0,Angle=0;
    FVector3d Origin=FVector3d::ZeroVector;
    bool bInvulnerable=false;
};
// One native combat host per simulated hero. No UObject, JSON, rendering or browser dependency.
class DWRULESCORE_API FDWCombatRuntime : public IDWRuleWorldExtension
{
  public:
	FDWCombatRuntime(FDWRuleHost &Host, FDWRuleCharacter &Character, uint32 Seed = 7);
	~FDWCombatRuntime();
	FDWCombatRuntime(const FDWCombatRuntime &) = delete;
	FDWCombatRuntime &operator=(const FDWCombatRuntime &) = delete;
	FDWRuleHost &Host;
	FDWRuleWorld &World;
	FDWRuleCharacter &Character;
	FDWCombatHero Hero;
	TArray<FDWCombatEnemy> Enemies;
	TArray<FDWCombatEffect> Effects;
	TUniquePtr<FDWAilments> Ailments;
	TUniquePtr<FDWZones> Zones;
	TUniquePtr<FDWProjectileAffixes> Projectiles;
	TUniquePtr<FDWMinions> Minions;
	TUniquePtr<FDWCombatTriggers> Triggers;
    TUniquePtr<FDWModernCombat> Modern;
    TArray<FDWTemporaryMonsterRequest> TemporaryRequests;
    int32 TemporaryMonsters=0;
	TMap<uint32, bool> LastCritical;
	TMap<uint32, double> Attackers;
	uint32 Marked = InvalidId;
	double MarkedUntil = 0, Time = 0, Cooldown = 0;
    TMap<FName,double> SkillReadyAt;
	// Read-only battle HUD totals, counted at the same committed row boundary as the web arena.
	double DamageDealt = 0, DamageTaken = 0;
	int64 Casts = 0;
	int32 StrikeBudget = 400;
	// Optional native presentation observer; called after resolution and cannot modify it.
	TFunction<void(const FDWCharacterHit&, const FDWTakenHit&)> OnHeroHurt;
    // Optional host policies. No bound policy preserves the ordinary runtime.
    TFunction<bool()> CanReceiveHeroDamage;
    TFunction<void(int32,float)> OnProjectileMotion;
    void SetProjectileVelocity(int32 Index,const FVector3f& Velocity);
	double S(FName Path) const
	{
		return Character.S(Path);
	}
	TArray<FDWSheetTrigger *> T(FName Name);
	bool Has(FName Name) const;
	double Random();
	bool Chance(double Percent);
	FDWCombatRoot *RootInfo(int32 Index, bool Create = false);
	FDWCombatEnemy *Enemy(uint32 Id);
	const FDWCombatEnemy *Enemy(uint32 Id) const;
	FDWCombatEnemy *EnemyOfOwner(int32 Owner);
	FVector3d HeroPosition() const;
	bool TargetPosition(uint32 Id, FVector3d &Out) const;
	TArray<uint32> EnemiesNear(const FVector3d &Center, double Radius, const TSet<uint32> &Exclude = {}) const;
	double LifeFraction(uint32 Id) const;
	double Status(uint32 Id, FName Name) const;
	bool HasAilment(uint32 Id) const;
	bool HasStatusChoice(uint32 Id, int32 Choice) const;
	uint64 TagMask(const FDWCompiledPlan &Plan, const TArray<FName> &Names) const;
	bool HasTag(const FDWCompiledPlan &Plan, uint64 Tags, FName Name) const;
	double AreaScale(int32 Element = -1, bool Triggered = false, bool Minion = false, bool Primary = false) const;
	int32 HeroPlan() const;
	int32 OpenRoot(int32 Plan = INDEX_NONE);
    void RuleEvent(FName Name,uint32 Target,double Amount);
	TArray<FDWStrikeHit> Strike(const FVector3d &Position, double Radius, double Amount, uint8 Element,
								const FDWStrikeOptions &Options = {});
	FDWDamageResult Hit(uint32 Id, double Amount, uint8 Element, FDWStrikeOptions Options = {});
	void BeforeStep(double Dt);
	void OnEnemySpawn(FDWCombatEnemy &Enemy);
	double EnemyPace(uint32 Id) const;
	void ScaleHeroAilments(TMap<uint64, TPair<float, int32>> &Seen);
	void ConsumeRows();
	void OnRow(const FDWDamageRecord &Row);
	void DamageEnemy(FDWCombatEnemy &Enemy, const FDWDamageRecord &Row);
	void OnHeroHit(const FDWCombatHit &Hit);
	void OnKill(const FDWCombatHit &Hit);
	FDWTakenHit HeroDamage(FDWCharacterHit Hit, bool Transferred = false);
	void Counter(uint32 Attacker, FName Type, const FDWCharacterEvent &Event);
	FDWSkillFamily SkillFamily(int32 Plan = INDEX_NONE) const;
	bool IsAttack(int32 Plan = INDEX_NONE) const;
	double CooldownFor(int32 Plan = INDEX_NONE) const;
    virtual bool Sacrifice(int32 Root,int32 Count,FName Kind) override;
    void DerivedRoot(int32 Root,int32 Source,bool Repeat) override;
    bool Intercept(int32 Index,int32 Target,const FVector3f& At) override;
    void AfterMotion() override;
    float WallRadius(int32 Index,float Radius) override;
    void ProjectileHit(int32 Index,const FVector3f& At) override;
    bool SpawnMonster(FName Kind,int32 Count,float Duration,float Radius,bool Invulnerable,int32 Owner) override;
	double CostScale(int32 Plan = INDEX_NONE) const;
	double CastSpeed(int32 Plan = INDEX_NONE) const;
	bool PlanSource(FDWTraitSource &Out,bool Minion=false,bool ProjectileOnly=false) const;
	void OnHeroCast(int32 Root, const FDWCastPayment &Payment);
	double ConvertTotal(uint8 Element);
	static bool SourceMatches(int32 Source, uint8 Element);
	FDWDamageMod ModifyDamage(const FDWRoot &, int32, float, uint8, FDWDamageInfo &) override;
	bool ModifyStatus(uint32, uint64, float &, float &, int32) override;
    void StatusConsumed(uint32, uint64, int32) override;
	bool Area(const FDWAreaRequest &) override;
	void Spawn(int32, int32, const FDWSpawnInfo &) override;
	void Motion(int32, float) override;
	bool Contact(int32, int32, const FDWDamageResult &) override;
	bool Wall(int32) override;
	bool End(int32, EEndReason) override;
	int32 Ricochet(int32, const FDWEvent &, float) override;
	void Redirect(int32, ERedirect) override;
	int32 SummonCap(int32, uint16, int32) override;
	void Summon(FDWSummon &, int32, const FDWSummon *) override;
	int32 SummonTarget(FDWSummon &, int32) override;
	void SummonEnd(const FDWSummon &) override;

  private:
	TMap<int32, TUniquePtr<FDWCombatRoot>> Roots;
	uint32 RandomState;
	uint64 RootSeed = 1;
	int32 NeutralPlan = INDEX_NONE, RowCursor = 0;
};
} // namespace DWRules
