#include "DWRuleCombat.h"
#include "DWRuleAilments.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleMinions.h"
#include "DWRuleProjectileAffixes.h"
#include "DWRuleZones.h"
#include "DWRuleModernCombat.h"

namespace DWRules
{
FDWCombatRuntime::FDWCombatRuntime(FDWRuleHost &InHost, FDWRuleCharacter &InCharacter, uint32 Seed)
	: Host(InHost), World(InHost.World), Character(InCharacter), RandomState(Seed ? Seed : 1)
{
	FDWBuildSource Source;
	Source.ActiveId = TEXT("__combat");
	Source.Traits.AddDefaulted_GetRef().Id = Source.ActiveId;
	FString Error;
	NeutralPlan = Host.CompileSource(Source, Error);
	Ailments = MakeUnique<FDWAilments>(*this);
	Zones = MakeUnique<FDWZones>(*this);
	Projectiles = MakeUnique<FDWProjectileAffixes>(*this);
	Minions = MakeUnique<FDWMinions>(*this);
	Triggers = MakeUnique<FDWCombatTriggers>(*this);
    Modern=MakeUnique<FDWModernCombat>(*this);
	checkf(!World.Extension, TEXT("A rule world has exactly one combat policy owner"));
	World.Extension = this;
}
bool FDWCombatRuntime::Sacrifice(int32 Root,int32 Count,FName Kind)
{
 if(!World.IsRootValid(Root)||World.GetRoot(Root).Owner!=Hero.Owner||Character.bDead)return false;
 return Minions->Sacrifice(Count,Kind);
}
FDWCombatRuntime::~FDWCombatRuntime()
{
	if (World.Extension == this)
		World.Extension = nullptr;
}
TArray<FDWSheetTrigger *> FDWCombatRuntime::T(FName Name)
{
	TArray<FDWSheetTrigger *> Out;
	for (auto &Trigger : Character.Sheet.Triggers)
		if (Trigger.Name == Name)
			Out.Add(&Trigger);
	return Out;
}
bool FDWCombatRuntime::Has(FName Name) const
{
	for (const auto &Trigger : Character.Sheet.Triggers)
		if (Trigger.Name == Name)
			return true;
	return false;
}
double FDWCombatRuntime::Random()
{
	RandomState = RandomState * 1664525u + 1013904223u;
	return double(RandomState) / 4294967296.;
}
bool FDWCombatRuntime::Chance(double Percent)
{
	return Percent > 0 && (Percent >= 100 || Random() * 100 < Percent);
}
FDWCombatRoot *FDWCombatRuntime::RootInfo(int32 Index, bool Create)
{
	if (!World.IsRootValid(Index))
		return nullptr;
	const uint64 Identity = World.GetRoot(Index).Identity;
	auto *Entry = Roots.Find(Index);
	if (Entry && (*Entry)->Identity != Identity)
	{
		Roots.Remove(Index);
		Entry = nullptr;
	}
	if (!Entry && Create)
	{
		auto Value = MakeUnique<FDWCombatRoot>();
		Value->Identity = Identity;
		Roots.Add(Index, MoveTemp(Value));
		Entry = Roots.Find(Index);
	}
	return Entry ? Entry->Get() : nullptr;
}
FDWCombatEnemy *FDWCombatRuntime::Enemy(uint32 Id)
{
	return Enemies.FindByPredicate([&](const auto &M) { return M.Id == Id; });
}
const FDWCombatEnemy *FDWCombatRuntime::Enemy(uint32 Id) const
{
	return Enemies.FindByPredicate([&](const auto &M) { return M.Id == Id; });
}
FDWCombatEnemy *FDWCombatRuntime::EnemyOfOwner(int32 Owner)
{
	return Enemy(World.GetOwnerStatusId(Owner));
}
FVector3d FDWCombatRuntime::HeroPosition() const
{
	const auto *O = World.GetOwner(Hero.Owner);
	return O ? FVector3d(O->Position) : FVector3d::ZeroVector;
}
bool FDWCombatRuntime::TargetPosition(uint32 Id, FVector3d &Out) const
{
	if (const auto *M = Enemy(Id))
	{
		Out = M->Position;
		return true;
	}
	const int32 Index = World.TargetIndexOf(Id);
	if (Index == INDEX_NONE)
		return false;
	Out = FVector3d(World.TargetData.Position[Index]);
	return true;
}
TArray<uint32> FDWCombatRuntime::EnemiesNear(const FVector3d &P, double Radius, const TSet<uint32> &Exclude) const
{
	TArray<TPair<uint32, double>> Found;
	for (const auto &M : Enemies)
		if (M.Health > 0 && !Exclude.Contains(M.Id))
		{
			const double D = FVector2d::Distance(FVector2d(P.X, P.Y), FVector2d(M.Position.X, M.Position.Y));
			if (D <= Radius + M.Radius)
				Found.Emplace(M.Id, D);
		}
	Found.Sort([](const auto &A, const auto &B) { return A.Value != B.Value ? A.Value < B.Value : A.Key < B.Key; });
	TArray<uint32> Out;
	for (const auto &Pair : Found)
		Out.Add(Pair.Key);
	return Out;
}
double FDWCombatRuntime::LifeFraction(uint32 Id) const
{
	const auto *M = Enemy(Id);
	return M ? M->Health / FMath::Max(1., M->MaxHealth) : 1;
}
double FDWCombatRuntime::Status(uint32 Id, FName Name) const
{
	return World.GetStatus(Id, StatusKey(Name.ToString().ToLower()));
}
bool FDWCombatRuntime::HasAilment(uint32 Id) const
{
	for (const TCHAR *Name :
		 {TEXT("poison"), TEXT("shock"), TEXT("chill"), TEXT("freeze"), TEXT("ignite"), TEXT("erosion"), TEXT("bleed")})
		if (Status(Id, Name) > 0)
			return true;
	return Ailments->Layers(Id, TEXT("poison")) > 0;
}
bool FDWCombatRuntime::HasStatusChoice(uint32 Id, int32 Choice) const
{
	if (!Choice)
		return HasAilment(Id);
	static const FName Names[] = {NAME_None,	 TEXT("poison"), TEXT("shock"),
								  TEXT("chill"), TEXT("ignite"), TEXT("erosion")};
	if (Choice < 0 || Choice >= UE_ARRAY_COUNT(Names))
		return false;
	return Choice == 1 || Choice == 5 ? Ailments->Layers(Id, Names[Choice]) > 0 : Status(Id, Names[Choice]) > 0;
}
uint64 FDWCombatRuntime::TagMask(const FDWCompiledPlan &Plan, const TArray<FName> &Names) const
{
	uint64 Mask = 0;
	for (FName Name : Names)
	{
		const int32 Index = Plan.TagNames.IndexOfByKey(Name);
		if (Index >= 0)
			Mask |= TagBit(Index);
	}
	return Mask;
}
bool FDWCombatRuntime::HasTag(const FDWCompiledPlan &Plan, uint64 Tags, FName Name) const
{
	const int32 Index = Plan.TagNames.IndexOfByKey(Name);
	return Index >= 0 && (Tags & TagBit(Index)) != 0;
}
double FDWCombatRuntime::AreaScale(int32 Element, bool Triggered, bool Minion, bool Primary) const
{
	double Area = S(TEXT("area.size"));
	if (Element == 1)
		Area += S(TEXT("frost.area"));
	if (Element == 0)
		Area += S(TEXT("fire.area"));
	if (Element == 5)
		Area += S(TEXT("void.area"));
	if (Triggered)
		Area += S(TEXT("trigger.area"));
	if (Minion)
		Area += S(TEXT("minion.area"));
	double Scale = FMath::Max(0., 1 + Area / 100) * S(TEXT("area.size_more"));
	if (Primary && S(TEXT("area.focus")))
		Scale *= .5;
	if (Primary && S(TEXT("area.widen")))
		Scale *= 1.6;
	return FMath::Sqrt(FMath::Max(.01, Scale));
}
int32 FDWCombatRuntime::HeroPlan() const
{
	return Hero.Plan >= 0 ? Hero.Plan : NeutralPlan;
}
int32 FDWCombatRuntime::OpenRoot(int32 Plan)
{
	RootSeed = RootSeed * 48271 % 2147483647;
	const int32 Root = World.OpenRoot(Plan == INDEX_NONE ? HeroPlan() : Plan, Hero.Owner, RootSeed);
	if (auto *Info = RootInfo(Root, true))
		Info->Kind = TEXT("derived");
	return Root;
}
TArray<FDWStrikeHit> FDWCombatRuntime::Strike(const FVector3d &P, double Radius, double Amount, uint8 Element,
											  const FDWStrikeOptions &O)
{
	TArray<FDWStrikeHit> Results;
	if (!(Amount > 0) || !(Radius > 0) || O.Depth > 4 || StrikeBudget <= 0)
		return Results;
	--StrikeBudget;
	const int32 Root = OpenRoot(O.Plan);
	if (Root == INDEX_NONE)
		return Results;
	auto *RI = RootInfo(Root);
	RI->Kind = O.Kind;
	RI->Depth = O.Depth;
	RI->bTriggered = O.bTriggered;
    RI->bMinion=O.MinionId!=0;RI->MinionId=O.MinionId;
	const auto &Plan = World.GetPlan(World.GetRoot(Root).Plan);
	static const FName Names[] = {TEXT("fire"),		TEXT("frost"),	TEXT("lightning"),
								  TEXT("physical"), TEXT("poison"), TEXT("void")};
	TArray<FName> Tags = O.Tags;
	if (!Tags.Num())
	{
		Tags.Add(Names[FMath::Min<int32>(Element, 5)]);
		if (Element == 1)
			Tags.Add(TEXT("cold"));
	}
	FDWDamageInfo Info = O.Info;
	Info.Tags = TagMask(Plan, Tags) | (O.bArea ? TagBit(DWRules::Tags::Area) : 0);
	Info.Strike = O.Effect;
	Info.Depth = O.Depth;
	Info.Position = Info.Center = FVector3f(P);
	Info.Radius = float(Radius);
	Info.bHasCenter = true;
	Info.Kind = TEXT("host");
	TArray<uint32> Victims;
	if (O.Only != InvalidId)
	{
		if (Enemy(O.Only))
			Victims.Add(O.Only);
	}
	else
		Victims = EnemiesNear(P, Radius, O.Exclude);
	if (O.bArea)
		Effects.Add({O.Effect, P, Radius, Amount, Element});
	for (uint32 Id : Victims)
	{
		const int32 Target = World.TargetIndexOf(Id);
		if (Target == INDEX_NONE || World.TargetData.Health[Target] <= 0)
			continue;
		FDWStrikeHit Hit;
		static_cast<FDWDamageResult &>(Hit) =
			World.HostHit(Root, Id, float(Amount), Element, Info, O.bSilent, uint8(2 + O.Depth));
		Hit.Id = Id;
		Results.Add(Hit);
	}
	World.ReleaseHeldRoot(Root);
	return Results;
}
FDWDamageResult FDWCombatRuntime::Hit(uint32 Id, double Amount, uint8 Element, FDWStrikeOptions O)
{
	const auto *M = Enemy(Id);
	if (!M || !(Amount > 0))
		return {};
	O.Only = Id;
	O.bArea = false;
	O.bSilent = true;
	const auto Results = Strike(M->Position, .01, Amount, Element, O);
	return Results.Num() ? static_cast<const FDWDamageResult &>(Results[0]) : FDWDamageResult();
}
void FDWCombatRuntime::BeforeStep(double Dt)
{
	Time += Dt;
	StrikeBudget = 400;
	if (World.DamageLog().IsEmpty())
		RowCursor = 0;
	if (MarkedUntil < Time)
		Marked = InvalidId;
	Cooldown = FMath::Max(0., Cooldown - Dt);
    if(SkillReadyAt.Contains(Hero.Skill))Cooldown=FMath::Max(0.,SkillReadyAt.FindRef(Hero.Skill)-Time);
	for (auto &M : Enemies)
	{
		M.SinceHit += Dt;
		if (M.SinceHit < 3 || !(M.Health > 0))
			continue;
		const double Poison = Ailments->Layers(M.Id, TEXT("poison")) ? S(TEXT("poison.enemy_regen")) : 0;
		const bool Eroded = Ailments->Layers(M.Id, TEXT("erosion")) > 0;
		const double Erosion = Eroded ? S(TEXT("erosion.enemy_regen")) : 0;
		M.Health =
			FMath::Min(M.MaxHealth, M.Health + M.MaxHealth * .01 * Dt * FMath::Max(0., 1 - (Poison + Erosion) / 100));
		const double Start = Eroded ? 3 / FMath::Max(.1, 1 - S(TEXT("erosion.enemy_recharge")) / 100) : 3;
		if (M.MaxShield > 0 && M.SinceHit >= Start)
			M.Shield = FMath::Min(
				M.MaxShield,
				M.Shield + M.MaxShield * .2 * Dt *
							   FMath::Max(0., 1 - (Eroded ? S(TEXT("erosion.enemy_shield_regen")) : 0) / 100));
	}
	if (const auto *Index = World.StatusDefByKey.Find(StatusKey(TEXT("chill"))))
		World.StatusDefs[*Index].Cap = float(70 + S(TEXT("chill.cap")));
	Character.Aura = {{TEXT("buff.damage_taken_reduction"), Minions->GuardReduction()}};
	Ailments->Tick(Dt);
    Modern->Tick(Dt);
	Zones->Tick(Dt);
	Projectiles->Tick(Dt);
	Minions->Tick(Dt);
	Triggers->Tick(Dt);
}
void FDWCombatRuntime::OnEnemySpawn(FDWCombatEnemy &M)
{
	M.bElite = Random() < .08;
	if (M.bElite)
	{
		M.MaxHealth *= 3;
		M.Health = M.MaxHealth;
		M.Radius = .6;
	}
	M.MaxShield = M.bElite ? M.MaxHealth * .25 : 0;
	M.Shield = M.MaxShield;
	M.SinceHit = 0;
	M.Resistance[0] = M.Resistance[1] = M.Resistance[2] = M.bElite ? 40.f : 20.f;
	M.Resistance[3] = 0;
}
double FDWCombatRuntime::EnemyPace(uint32 Id) const
{
	return Ailments->Layers(Id, TEXT("poison")) > 0 ? FMath::Max(.1, 1 - S(TEXT("poison.enemy_slow")) / 100) : 1;
}
void FDWCombatRuntime::ScaleHeroAilments(TMap<uint64, TPair<float, int32>> &Seen)
{
	const double Scale = Character.AilmentScale();
	for (auto &Pair : World.Statuses)
	{
		auto &Status = Pair.Value;
		if (Status.TargetId != World.GetOwnerStatusId(Hero.Owner))
			continue;
		const auto *DefIndex = World.StatusDefByKey.Find(Status.Key);
		if (!DefIndex || World.StatusDefs[*DefIndex].Category != TEXT("ailment"))
			continue;
		const TPair<float, int32> Stamp{Status.Expire, Status.Stacks};
		if (const auto *Old = Seen.Find(Status.Key); Old && *Old == Stamp)
			continue;
		Seen.Add(Status.Key, Stamp);
		if (Scale < 1)
			Status.Value *= float(Scale);
	}
}
void FDWCombatRuntime::DamageEnemy(FDWCombatEnemy &M, const FDWDamageRecord &Row)
{
	double Amount = Row.Amount;
	M.SinceHit = 0;
	if (M.Shield > 0)
	{
		const double Bypass = Row.Element == 5 && Row.OwnerExternalId == World.GetOwnerExternalId(Hero.Owner)
								  ? FMath::Min(100., S(TEXT("void.bypass_shield"))) / 100
								  : 0;
		const double ToShield = FMath::Min(M.Shield, Amount * (1 - Bypass));
		M.Shield -= ToShield;
		Amount -= ToShield;
	}
	M.Health = FMath::Max(0., M.Health - Amount);
	if (Row.bKilled)
		M.Health = 0;
}
void FDWCombatRuntime::ConsumeRows()
{
	if (RowCursor > World.DamageLog().Num())
		RowCursor = 0;
	while (RowCursor < World.DamageLog().Num())
	{
		const FDWDamageRecord Row = World.DamageLog()[RowCursor++];
		if (auto *DamagedEnemy = Enemy(Row.TargetExternalId))
		{
			DamageEnemy(*DamagedEnemy, Row);
			OnRow(Row);
		}
		else if (Row.TargetExternalId == World.GetOwnerStatusId(Hero.Owner))
		{
			FDWCharacterHit H;
			H.Amount = Row.Amount;
			H.OriginalAmount = Row.Info.OriginalAmount;
			H.Element = EElement(Row.Element);
			H.bDot = Row.bDot;
			H.bCritical = Row.bCritical;
			const FDWCompiledPlan *Plan = World.IsRootValid(Row.Info.RootIndex)
											  ? &World.GetPlan(World.GetRoot(Row.Info.RootIndex).Plan)
											  : nullptr;
			H.Kind.bSpell = Plan && HasTag(*Plan, Row.Info.Tags, TEXT("spell"));
			H.Kind.bAttack = !H.Kind.bSpell;
			H.Kind.bMelee = Plan && HasTag(*Plan, Row.Info.Tags, TEXT("melee"));
			H.Kind.bProjectile = !!(Row.Info.Tags & TagBit(Tags::Projectile));
			H.Kind.bArea = !!(Row.Info.Tags & TagBit(Tags::Area));
			for (const auto &M : Enemies)
				if (World.GetOwnerExternalId(M.Owner) == Row.OwnerExternalId)
				{
					H.Attacker = M.Id;
					Attackers.Add(M.Id, Time);
					break;
				}
			if (!Triggers->bGod)
				HeroDamage(H);
		}
		else if (auto *M = Minions->StateOfTarget(Row.TargetExternalId))
		{
			const FDWCompiledPlan *Plan = World.IsRootValid(Row.Info.RootIndex)
											  ? &World.GetPlan(World.GetRoot(Row.Info.RootIndex).Plan)
											  : nullptr;
			Minions->TakeDamage(*M, Row.Amount, Row.Element, Row.bDot, !!(Row.Info.Tags & TagBit(Tags::Area)),
								Plan && HasTag(*Plan, Row.Info.Tags, TEXT("spell")));
		}
	}
}
FDWTakenHit FDWCombatRuntime::HeroDamage(FDWCharacterHit Hit, bool Transferred)
{
    if(CanReceiveHeroDamage&&!CanReceiveHeroDamage())return {};
	if (!(Hit.OriginalAmount > 0)) Hit.OriginalAmount = Hit.Amount;
	if (!Transferred && !Hit.bDot)
		Hit.Amount -= Minions->AbsorbForHero(Hit.Amount, uint8(Hit.Element));
	const auto Taken=Character.TakeHit(Hit);if(OnHeroHurt)OnHeroHurt(Hit,Taken);return Taken;
}
void FDWCombatRuntime::OnRow(const FDWDamageRecord &Row)
{
	if (Row.Info.bReflected) return;
	if (Row.OwnerExternalId != World.GetOwnerExternalId(Hero.Owner) || !Enemy(Row.TargetExternalId))
		return;
	auto *RI = RootInfo(Row.Info.RootIndex);
	if (!RI || !RI->bMinion)
		Character.OnDealt(Row.Amount, EElement(Row.Element), Row.bCritical, Row.bKilled, Row.bDot);
	FDWCombatHit H;
	H.Id = Row.TargetExternalId;
	H.Amount = Row.Amount;
	H.Element = Row.Element;
	H.bCritical = Row.bCritical;
	H.bKilled = Row.bKilled;
	H.bDot = Row.bDot;
	H.Info = Row.Info;
	H.Root = RI;
	H.bProjectile = H.Info.Kind == TEXT("projectile");
	H.bArea = H.Info.bHasCenter || H.Info.Kind == TEXT("area");
	H.bMinion = RI && RI->bMinion;
	H.bTriggered = RI && RI->bTriggered;
	H.Depth = H.Info.Depth ? H.Info.Depth : RI ? RI->Depth : 0;
	H.bSilent = !H.Info.Strike.IsNone() && H.Info.Strike != TEXT("zone") && H.Info.Strike != TEXT("chain") &&
				H.Info.Strike != TEXT("arc");
	if (!H.bDot && !H.bMinion)
	{
		Marked = H.Id;
		MarkedUntil = Time + 4;
	}
	if (RI)
	{
		RI->Victims.Add(H.Id);
		RI->HitCount += H.bDot ? 0 : 1;
	}
	if (!H.bDot)
	{
		if (H.bMinion)
			Minions->OnHit(H);
		else
			OnHeroHit(H);
	}
	Ailments->OnHit(H);
	if (H.bKilled)
		OnKill(H);
	Triggers->OnDealt(H);
}
void FDWCombatRuntime::OnHeroHit(const FDWCombatHit &H)
{
    Modern->Hit(H);
	if (H.bProjectile)
	{
		Character.Recover(ECharacterPool::Life, S(TEXT("proj.hit_life")));
		Character.Recover(ECharacterPool::Shield, S(TEXT("proj.hit_shield")));
		Character.Recover(ECharacterPool::Mana, S(TEXT("proj.hit_mana")));
	}
	if (H.Info.ZoneId)
	{
		Character.Recover(ECharacterPool::Life, S(TEXT("area.per_hit_life")));
		Character.Recover(ECharacterPool::Mana, S(TEXT("area.per_hit_mana")));
		Zones->OnHit(H);
	}
	if (Ailments->Layers(H.Id, TEXT("poison")) && S(TEXT("poison.vs_leech")) > 0)
		Character.AddLeech(ECharacterPool::Life, H.Amount * S(TEXT("poison.vs_leech")) / 100);
	Minions->OnHeroHit(H);
	if (!H.Info.Strike.IsNone() && H.Info.Strike != TEXT("zone"))
		return;
	for (const auto *T : this->T(TEXT("extra_damage")))
		if (SourceMatches(int32(T->Param(TEXT("source"))), H.Element))
		{
			FDWStrikeOptions O;
			O.Effect = TEXT("extra");
			Hit(H.Id, H.Amount * T->Param(TEXT("n")) / 100, uint8(T->Param(TEXT("element"))), O);
		}
	for (const auto *T : this->T(TEXT("convert_damage")))
		if (SourceMatches(int32(T->Param(TEXT("source"))), H.Element) && H.Info.Converted > 0)
		{
			FDWStrikeOptions O;
			O.Effect = TEXT("convert");
			Hit(H.Id,
				H.Info.Converted * T->Param(TEXT("n")) / FMath::Max(1.e-9, FMath::Min(100., ConvertTotal(H.Element))),
				uint8(T->Param(TEXT("element"))), O);
		}
}
bool FDWCombatRuntime::SourceMatches(int32 Source, uint8 Element)
{
	return Source == -1 || (Source == -2 ? Element <= 2 : Source == Element);
}
double FDWCombatRuntime::ConvertTotal(uint8 Element)
{
	double Total = 0;
	for (const auto *T : this->T(TEXT("convert_damage")))
		if (SourceMatches(int32(T->Param(TEXT("source"))), Element))
			Total += T->Param(TEXT("n"));
	return Total;
}
void FDWCombatRuntime::OnKill(const FDWCombatHit &H)
{
	if (!H.bMinion)
	{
		const int32 Was = Character.Charges;
		Character.Charges = FMath::Min(3, Was + 1);
		if (Character.Charges > Was)
		{
			Character.Emit(TEXT("charge_gained"));
			if (Character.Charges == 3)
				Character.Emit(TEXT("charge_max"));
		}
	}
	if (H.Info.bExecuted)
	{
		Character.Recover(ECharacterPool::Life, S(TEXT("execute.life")));
		for (const auto *T : this->T(TEXT("execute_spread")))
			Ailments->SpreadLayers(H.Id, TEXT("erosion"), int32(T->Param(TEXT("n"))), 3);
		if (Chance(S(TEXT("execute.collapse_chance"))))
			Ailments->Collapse(H.Id, true);
	}
	Minions->OnKill(H);
	Zones->OnKill(H);
}
void FDWCombatRuntime::Counter(uint32 Id, FName Type, const FDWCharacterEvent &Event)
{
	if (!Enemy(Id))
		return;
	if (Type == TEXT("block"))
	{
		FDWStrikeOptions O;
		O.Effect = TEXT("reflect");
		if (S(TEXT("block.reflect_physical")) > 0)
			Hit(Id, S(TEXT("block.reflect_physical")), 3, O);
		if (S(TEXT("block.reflect_fire")) > 0)
			Hit(Id, S(TEXT("block.reflect_fire")), 0, O);
		if (Chance(S(TEXT("block.stun_chance"))))
			World.ApplyStatusKey(Id, StatusKey(TEXT("stun")), 1, 1, Hero.Owner);
	}
	double Rate = Type == TEXT("block") ? S(TEXT("block.counter_chance")) : 0;
	if (Type != TEXT("block"))
		for (const auto *T : this->T(TEXT("evade_counter")))
			Rate = FMath::Max(Rate, T->Param(TEXT("chance")));
	if (!Chance(Rate))
		return;
	FDWStrikeOptions O;
	O.Effect = TEXT("counter");
	O.bArea = false;
	O.Only = Id;
	const double Amount =
		Hero.Power * (1 + Event.Overflow * S(TEXT("block.counter_per_overcap")) / 100) + Character.CounterBank;
	Character.CounterBank = 0;
	Strike(Enemy(Id)->Position, .01, Amount, 3, O);
}
bool FDWCombatRuntime::ModifyStatus(uint32 Id, uint64 Key, float &Value, float &Duration, int32 Owner)
{
	return Ailments->OnStatus(Id, Key, Value, Duration, Owner);
}
void FDWCombatRuntime::StatusConsumed(uint32 Id,uint64 Key,int32 Count)
{
    Ailments->OnStatusConsumed(Id,Key,Count);
}
bool FDWCombatRuntime::Area(const FDWAreaRequest &Request)
{
	if (Zones->OnArea(Request))
		return true;
	Effects.Add({Request.Trait, FVector3d(Request.Event->Position), Request.Radius, Request.Amount, Request.Element});
	return false;
}
void FDWCombatRuntime::Spawn(int32 Index, int32 Root, const FDWSpawnInfo &Data)
{
	Projectiles->OnSpawn(Index, Root, Data);
    Modern->Spawn(Index,Root);
}
void FDWCombatRuntime::Motion(int32 Index, float Dt)
{
    if(!Modern->Motion(Index,Dt))Projectiles->OnMotion(Index, Dt);
    if(OnProjectileMotion)OnProjectileMotion(Index,Dt);
}
void FDWCombatRuntime::SetProjectileVelocity(int32 Index,const FVector3f& Velocity)
{
    if(Index>=0&&Index<World.Carriers.Num&&!Velocity.ContainsNaN())World.Carriers.Velocity[Index]=Velocity;
}
bool FDWCombatRuntime::Contact(int32 Index, int32 Target, const FDWDamageResult &Result)
{
    const bool Existing=Projectiles->OnContact(Index,Target,Result);
    return Modern->Contact(Index,Target)||Existing;
}
bool FDWCombatRuntime::Wall(int32 Index)
{
    return Modern->Wall(Index)||Projectiles->OnWall(Index);
}
bool FDWCombatRuntime::End(int32 Index, EEndReason Reason)
{
    const bool Keep=Projectiles->OnEnd(Index,Reason);
    if(!Keep){const auto& C=World.Carriers;if(World.GetPlan(World.GetRoot(C.Root[Index]).Plan).KindNames[C.Kind[Index]]==TEXT("treasury_ea"))
    {if(Effects.Num()>=2048)Effects.RemoveAt(0);auto& E=Effects.AddDefaulted_GetRef();E.Kind=TEXT("__treasury_ea_impact");E.Position=FVector3d(C.Position[Index]);E.Position.Z=FMath::Max(.05,E.Position.Z);E.Element=3;E.Radius=3.5;}}
    return Keep;
}
int32 FDWCombatRuntime::Ricochet(int32 Index, const FDWEvent &Event, float Radius)
{
	return Projectiles->OnRicochet(Index, Event, Radius);
}
void FDWCombatRuntime::Redirect(int32 Index, ERedirect Mode)
{
	Projectiles->OnRedirect(Index, Mode);
}
int32 FDWCombatRuntime::SummonCap(int32 Cap, uint16 Kind, int32 Root)
{
	return Minions->CapFor(Cap, Kind, Root);
}
void FDWCombatRuntime::Summon(FDWSummon &Summon, int32 Root, const FDWSummon *Replaced)
{
	Minions->OnSummon(Summon, Root, Replaced);
}
int32 FDWCombatRuntime::SummonTarget(FDWSummon &Summon, int32 Nearest)
{
	return Minions->Target(Summon, Nearest);
}
void FDWCombatRuntime::SummonEnd(const FDWSummon &Summon)
{
	Minions->OnEnd(Summon);
}
} // namespace DWRules
