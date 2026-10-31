#include "DWRuleAilments.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleStateReader.h"
#include "DWRulePawnEquipment.h"
#include "DWRuleTargetComponent.h"
#include "DWRuleWorldSubsystem.h"
#include "GameFramework/Actor.h"

using namespace DWRules;
bool UDWRuleWorldSubsystem::EnablePlayer(int32 Owner, FString &Error)
{
	if (!Runtime || !Runtime->GetOwner(Owner) || Runtime->GetOwner(Owner)->Team != 0 || Host->Library.Traits.IsEmpty())
	{
		Error = TEXT("load Rules and register a player owner first");
		return false;
	}
	if (Player)
	{
		if (Player->Combat.Hero.Owner == Owner)
			return true;
		Error = TEXT("this world already has a player session");
		return false;
	}
	const uint32 Id = ResolveTargetId(int32(Runtime->GetOwnerExternalId(Owner)));
	Player = MakeUnique<FDWRuleSession>(*Host, Owner, Id);
 HUDReceipts.Reset(); HUDDamageMeter.Reset(); HUDReceiptSerial=0;
 Player->Combat.OnHeroHurt=[this](const FDWCharacterHit& Hit,const FDWTakenHit& Taken)
 {
  const double Lost=Taken.LifeLost+Taken.ShieldLost;
  if (Lost<=.01 || !Player || !Runtime) return;
  FDWRuleHUDReceipt Receipt; Receipt.Amount=Lost; Receipt.bHero=true;
  Receipt.Element=uint8(Hit.Element); Receipt.bCritical=Hit.bCritical;
  const int32 Target=Runtime->TargetIndexOf(Player->TargetId);
  if (Target>=0) Receipt.Position=FVector(Runtime->Targets().Position[Target])*100;
  else Receipt.Position=FVector(Player->Combat.HeroPosition())*100;
  AddHUDReceipt(Receipt);
 };
	if (!PlayerProfile.bInitialized)
		PlayerProfile.Reset(Host->Library);
	return true;
}
bool UDWRuleWorldSubsystem::ApplyPlayerProfile(FDWRuleProfile Next, FString &Error)
{
	if (!Player)
	{
		Error = TEXT("player session is not enabled");
		return false;
	}
	Next.Loadout.PassiveParts = Player->Loadout.PassiveParts;
	for (auto& Item : Next.Loadout.Instances) if (!FDWEquipmentRules::EnsureBases(Host->Library, Item, Error)) return false;
	if (!Player->SetLoadout(Next.Loadout, Error))
		return false;
	Player->Belt = Next.Belt;
	PlayerProfile = MoveTemp(Next);
	SyncPlayerTarget();
	return true;
}
bool UDWRuleWorldSubsystem::ImportPlayerState(const FString &Json, FString &Error)
{
	if(!PlayerEventLocks.IsEmpty()){Error=TEXT("请先收取祭坛里锁定的装备");return false;}
	FDWRuleProfile Next;
	return FDWRuleStateReader::Read(Json, Host->Library, Next, Error) && ApplyPlayerProfile(MoveTemp(Next), Error);
}
FString UDWRuleWorldSubsystem::ExportPlayerState() const
{
	return FDWRuleStateReader::Write(PlayerProfile, Host->Library);
}
FDWRulePlayerPools UDWRuleWorldSubsystem::GetPlayerPools() const
{
	FDWRulePlayerPools Out;
	if (!Player)
		return Out;
	const auto &C = Player->Character;
	Out.bActive = true;
	Out.TargetId = int32(Runtime->GetOwnerExternalId(Player->Combat.Hero.Owner));
	Out.bDead = C.bDead;
	Out.Life = C.Life;
	Out.MaxLife = C.MaxLife;
	Out.Shield = C.Shield;
	Out.MaxShield = C.MaxShield;
	Out.Mana = C.Mana;
	Out.MaxMana = C.MaxMana;
	float Remaining = 0;
	const bool Control = Runtime->GetStatusEffect(Player->TargetId, EStatusEffect::Control, Remaining) > 0;
	Out.MoveScale =
		Control || C.Stunned > 0
			? 0
			: FMath::Max(.1, 1 + C.MoveSpeed() / 100) *
				  FMath::Max(.1f,
							 1 + Runtime->GetStatusEffect(Player->TargetId, EStatusEffect::MoveSpeed, Remaining) / 100);
	Out.ActionScale =
		Control || C.Stunned > 0
			? 0
			: FMath::Max(.1, 1 + (C.ActionSpeed(Player->Combat.IsAttack()) + Player->Combat.CastSpeed()) / 100) *
				  FMath::Max(
					  .1f, 1 + Runtime->GetStatusEffect(Player->TargetId, EStatusEffect::ActionSpeed, Remaining) / 100);
	Out.Cooldown = Player->Combat.Cooldown;
	return Out;
}
bool UDWRuleWorldSubsystem::ChangePlayerSkill(int32 Slot, FName Skill, FString &Error)
{
	auto Next = PlayerProfile;
	return Next.Loadout.ChangeSkill(Host->Library, Slot, Skill, Error) && ApplyPlayerProfile(MoveTemp(Next), Error);
}
FDWRulePlayerPools UDWRuleWorldSubsystem::GetTargetPools(int32 TargetId) const
{
	const uint32 Id = ResolveTargetId(TargetId);
	if (Player && Player->TargetId == Id)
		return GetPlayerPools();
	FDWRulePlayerPools Out;
	if (const auto *M = CombatEnemy(Id))
	{
		Out.bActive = true;
		Out.TargetId = TargetId;
		Out.bDead = M->Health <= 0;
		Out.Life = M->Health;
		Out.MaxLife = M->MaxHealth;
		Out.Shield = M->Shield;
		Out.MaxShield = M->MaxShield;
		Out.MoveScale = GetMonsterMoveScale(TargetId);
	}
	return Out;
}
bool UDWRuleWorldSubsystem::ChangePlayerGem(FName Skill, int32 Slot, FName Gem, FString &Error)
{
	auto Next = PlayerProfile;
	return Next.Loadout.ChangeGem(Host->Library, Skill, Slot, Gem, Error) && ApplyPlayerProfile(MoveTemp(Next), Error);
}
bool UDWRuleWorldSubsystem::EquipPlayerItem(FName Slot, FName Item, FString &Error)
{
	if(IsPlayerItemLocked(Item.ToString())){Error=TEXT("该装备已被祭坛锁定");return false;}
	auto Next = PlayerProfile;
	return Next.Equip(Host->Library, Slot, Item, Error) && ApplyPlayerProfile(MoveTemp(Next), Error);
}
bool UDWRuleWorldSubsystem::BuyPlayerListing(const FString &Key, FString &Error)
{
	auto Next = PlayerProfile;
	if (!Next.Buy(Host->Library, Key, Error, TOptional<int64>(0))) return false;
 // Existing GM channel is free; only consumables auto-equip in the current web shop.
 FString Kind,IdText; Key.Split(TEXT(":"),&Kind,&IdText); const FName Id(*IdText);
 if(Kind==TEXT("item")) { const auto* D=Host->Library.Items.Find(Id); if(D && D->Category==TEXT("consumable") && !Next.Belt.Contains(Id)) { if(const int32 Empty=Next.Belt.IndexOfByKey(NAME_None); Empty>=0) Next.Belt[Empty]=Id; else if(Next.Belt.Num()<4) Next.Belt.Add(Id); } }
 return ApplyPlayerProfile(MoveTemp(Next), Error);
}
bool UDWRuleWorldSubsystem::IdentifyPlayerEquipment(FName Item, FString &Uid, FString &Error)
{
	if(IsPlayerItemLocked(Item.ToString())){Error=TEXT("该装备已被祭坛锁定");return false;}
	auto Next = PlayerProfile;
	return Next.Identify(Host->Library, Item, Uid, Error) && ApplyPlayerProfile(MoveTemp(Next), Error);
}
bool UDWRuleWorldSubsystem::CraftPlayerEquipment(const FString &Uid, FName Currency, FName Omen, int32 Index,
												 FString &Error)
{
	if(IsPlayerItemLocked(Uid)){Error=TEXT("请在锁定这件装备的祭坛中制作");return false;}
	auto Next = PlayerProfile;
	return Next.Craft(Host->Library, Uid, Currency, Omen, Index, Error) && ApplyPlayerProfile(MoveTemp(Next), Error);
}
bool UDWRuleWorldSubsystem::SelectPlayerSkill(int32 Slot, FString &Error)
{
	return Player && Player->Select(Slot, Error);
}
bool UDWRuleWorldSubsystem::DiscardPlayerEquipment(const FString &Uid, FString &Error)
{
	if(IsPlayerItemLocked(Uid)){Error=TEXT("该装备已被祭坛锁定");return false;}
	auto Next = PlayerProfile;
	Next.Discard(Uid);
	Next.Loadout.Validate(Host->Library);
	return ApplyPlayerProfile(MoveTemp(Next), Error);
}
bool UDWRuleWorldSubsystem::UsePlayerItem(FName Item, FString &Error)
{
	if (!Player || PlayerProfile.Loadout.Owned.FindRef(Item) <= 0)
	{
		Error = TEXT("没有该物品");
		return false;
	}
	const auto *O = Runtime->GetOwner(Player->Combat.Hero.Owner);
	if (!O || !Player->UseItem(Item, O->Position, O->Aim, Mix64(++CastCounter), Error))
		return false;
	PlayerProfile.Consume(Item);
	Player->Loadout.Owned = PlayerProfile.Loadout.Owned;
	Player->Belt = PlayerProfile.Belt;
 const auto* ItemDoc=Host->Library.Items.Find(Item);
 AddHUDLog(TEXT("使用 ")+(ItemDoc?ItemDoc->Name:Item.ToString()));
	return true;
}
bool UDWRuleWorldSubsystem::StartPlayerProgression(FString &Error)
{
	if(!PlayerEventLocks.IsEmpty()){Error=TEXT("请先收取祭坛里锁定的装备");return false;}
	auto Next = PlayerProfile;
	return Next.StartProgression(Host->Library, Error) && ApplyPlayerProfile(MoveTemp(Next), Error);
}
void UDWRuleWorldSubsystem::RespawnPlayer()
{
	if (Player)
	{
		Player->Respawn();
		SyncPlayerTarget();
	}
}
void UDWRuleWorldSubsystem::SetPlayerGod(bool Enabled)
{
	if (Player)
		Player->bGod = Enabled;
}
bool UDWRuleWorldSubsystem::DodgePlayer()
{
	return Player && Player->Dodge();
}
void UDWRuleWorldSubsystem::SetPlayerCasting(bool Casting)
{
	if (Player)
		Player->Combat.Triggers->bCasting = Casting;
}
bool UDWRuleWorldSubsystem::SacrificePlayerMinion()
{
	return Player && Player->Sacrifice();
}
void UDWRuleWorldSubsystem::SyncPlayerTarget()
{
	if (!Player)
		return;
	for (auto &Weak : ActorTargets)
		if (auto *T = Weak.Get(); T && uint32(T->TargetId) == Player->TargetId)
		{
			T->Health = Player->Character.Life;
			T->MaxHealth = Player->Character.MaxLife;
			T->Shield = Player->Character.Shield;
			T->MaxShield = Player->Character.MaxShield;
		}
}
const FDWCombatEnemy *UDWRuleWorldSubsystem::CombatEnemy(uint32 Id) const
{
	if (Player)
		if (const auto *M = Player->Combat.Enemy(Id))
			return M;
	return FrameEnemies.Find(Id);
}
void UDWRuleWorldSubsystem::ExportCombatTarget(uint32 Id, const FVector3f &Position, float Radius, float Health,
											   float MaxHealth, float Shield, float MaxShield, uint8 Team, bool Elite,
											   const float *Resistance)
{
	if (Player && Team != Runtime->GetOwner(Player->Combat.Hero.Owner)->Team)
	{
		FDWCombatEnemy M;
		if (const auto *Old = Player->Combat.Enemy(Id))
			M = *Old;
		else if (const auto *Spawned = FrameEnemies.Find(Id))
			M = *Spawned;
		M.Id = Id;
		M.Position = FVector3d(Position);
		M.Radius = Radius;
		M.Health = Health;
		M.MaxHealth = MaxHealth;
		M.Shield = Shield;
		M.MaxShield = MaxShield;
		M.bElite = Elite;
		for (int32 I = 0; I < NumResistances; ++I)
			M.Resistance[I] = Resistance ? Resistance[I] : 0;
		if (MonsterStates.Contains(Id))
			if (const auto *Owned = FrameEnemies.Find(Id))
			{
				M.Health = Owned->Health;
				M.MaxHealth = Owned->MaxHealth;
				M.Shield = Owned->Shield;
				M.MaxShield = Owned->MaxShield;
				M.Radius = Owned->Radius;
				M.bElite = Owned->bElite;
				for (int32 I = 0; I < NumResistances; ++I)
					M.Resistance[I] = Owned->Resistance[I];
			}
		FrameEnemies.Add(Id, M);
		FrameEnemyIds.Add(Id);
		Health = M.Health;
		Shield = M.Shield;
		Radius = M.Radius;
		Resistance = FrameEnemies[Id].Resistance;
	}
	Runtime->Targets().Add(Position, Radius, Health + Shield, Team, Id, Resistance);
}
TArray<FName> UDWRuleWorldSubsystem::GetMonsterTypes() const
{
	TArray<FName> Out;
	for (const auto &Pair : Host->Library.Monsters)
		if (Pair.Value.bEnabled && Pair.Value.Weight > 0)
			Out.Add(Pair.Key);
	return Out;
}
int32 UDWRuleWorldSubsystem::RegisterMonster(int32 TargetId, FName Type, FVector Location, FString &Error)
{
	const auto *Definition = Host->Library.Monsters.Find(Type);
	if (!Definition)
	{
		Error = TEXT("unknown monster type: ") + Type.ToString();
		return INDEX_NONE;
	}
	if (!MonsterPlans.Contains(Type))
	{
		FDWCompiledMonster Compiled;
		if (!FDWMonsterRules::Compile(*Host, *Definition, Compiled, Error))
			return INDEX_NONE;
		MonsterPlans.Add(Type, MoveTemp(Compiled));
	}
	return RegisterCompiledMonster(TargetId, MonsterPlans[Type], Location, Error);
}
int32 UDWRuleWorldSubsystem::RegisterMonsterDefinition(int32 TargetId, const FDWMonsterDefinition& Definition, FVector Location, FString& Error, float RadiusCm)
{
	FDWCompiledMonster Compiled;
	if (!FDWMonsterRules::Compile(*Host, Definition, Compiled, Error))
		return INDEX_NONE;
	return RegisterCompiledMonster(TargetId, Compiled, Location, Error, RadiusCm);
}
int32 UDWRuleWorldSubsystem::RegisterCompiledMonster(int32 TargetId, const FDWCompiledMonster& Compiled, FVector Location, FString& Error, float RadiusCm)
{
	const uint32 Id = ResolveTargetId(TargetId);
	ForgetTarget(Id);
	auto &State = MonsterStates.Add(Id);
	State.Compiled = Compiled;
	State.Owner = Runtime->AddOwner(uint32(TargetId), 1);
	Runtime->SetOwnerStatusId(State.Owner, Id);
	Runtime->UpdateOwner(State.Owner, FVector3f(Location) * .01f, FVector3f(1, 0, 0));
	State.Cooldowns.Shared = .2;
	FDWCombatEnemy M;
	M.Id = Id;
	M.Owner = State.Owner;
	M.Position = FVector3d(Location) * .01;
	M.Health = M.MaxHealth = Compiled.Definition.Health;
	M.Rarity = FDWPawnEquipmentRules::Rarity(Compiled.Definition.Equipment);
	M.Radius = .45;
	if (Player)
		Player->Combat.OnEnemySpawn(M);
	if (RadiusCm > 0) M.Radius = RadiusCm * .01;
	State.BaseHealth = M.MaxHealth;
	State.BaseShield = M.MaxShield;
	if (!FDWMonsterRules::Equip(*Host, Compiled, State.BaseHealth, State.BaseShield, State.Character, Error))
		return INDEX_NONE;
	M.Health = M.MaxHealth = State.Character.MaxLife;
	M.Shield = M.MaxShield = State.Character.MaxShield;
	if (!Host->SetTargetDefenseSources(Id, State.Compiled.Sources, Error))
		return INDEX_NONE;
	FrameEnemies.Add(Id, M);
	if (Player)
		Player->Combat.Enemies.Add(M);
	return State.Owner;
}
bool UDWRuleWorldSubsystem::ApplyPlayerPreset(const FString &Json, FString &Error)
{
	if(!PlayerEventLocks.IsEmpty()){Error=TEXT("请先收取祭坛里锁定的装备");return false;}
	FDWRuleProfile Preset;
	if (!FDWRuleStateReader::ReadPreset(Json, Host->Library, Preset, Error))
		return false;
	auto Next = PlayerProfile;
	return Next.ApplyPreset(Host->Library, Preset, Error) && ApplyPlayerProfile(MoveTemp(Next), Error);
}
bool UDWRuleWorldSubsystem::SetMonsterHitPower(int32 TargetId, float Power, FString& Error)
{
	auto* State = MonsterStates.Find(ResolveTargetId(TargetId));
	if (!State || !FMath::IsFinite(Power) || Power < 0)
	{
		Error = TEXT("invalid monster or hit power");
		return false;
	}
	auto Skills = State->Compiled.Skills;
	for (auto& Skill : Skills)
	{
		const auto* Original = Host->PlanSource(Skill.Plan);
		if (!Original) { Error = TEXT("monster skill has no source"); return false; }
		auto Source = *Original;
		for (auto& Trait : Source.Traits)
			if (Trait.Id == Source.ActiveId)
				if (auto* Damage = Trait.Params.Find(TEXT("damage"))) *Damage = Power;
		Skill.Plan = Host->CompileSource(Source, Error);
		if (Skill.Plan < 0) return false;
		Skill.Power = Power;
	}
	State->Compiled.Skills = MoveTemp(Skills);
	return true;
}
bool UDWRuleWorldSubsystem::SetMonsterTraits(int32 TargetId, const FString &Json, FString &Error)
{
	const uint32 Id = ResolveTargetId(TargetId);
	auto *State = MonsterStates.Find(Id);
	auto *M = FrameEnemies.Find(Id);
	if (!State || !M)
	{
		Error = TEXT("monster has not been registered");
		return false;
	}
	auto Definition = State->Compiled.Definition;
	if (!FDWRuleStateReader::ReadEntries(Json, Host->Library, Definition.Traits, Error))
		return false;
	FDWCompiledMonster Compiled;
	if (!FDWMonsterRules::Compile(*Host, Definition, Compiled, Error))
		return false;
	if (!Host->SetTargetDefenseSources(Id, Compiled.Sources, Error))
		return false;
	const double LifeRatio = M->MaxHealth > 0 ? M->Health / M->MaxHealth : 1;
	const double ShieldRatio = M->MaxShield > 0 ? M->Shield / M->MaxShield : 1;
	FDWRuleCharacter Character;
	if (!FDWMonsterRules::Equip(*Host, Compiled, State->BaseHealth, State->BaseShield, Character, Error))
		return false;
	M->MaxHealth = Character.MaxLife;
	M->Health = M->MaxHealth * LifeRatio;
	M->MaxShield = Character.MaxShield;
	M->Shield = M->MaxShield * ShieldRatio;
	Character.Life = M->Health;
	Character.Shield = M->Shield;
	State->Character = MoveTemp(Character);
	State->Compiled = MoveTemp(Compiled);
	if (Player)
		if (auto *Live = Player->Combat.Enemy(Id))
			*Live = *M;
	return true;
}
bool UDWRuleWorldSubsystem::TickMonsterSkill(int32 TargetId, FVector Location, FVector TargetLocation,
											 float DeltaSeconds, bool TargetAlive)
{
	const uint32 Id = ResolveTargetId(TargetId);
	auto *State = MonsterStates.Find(Id);
	if (!State)
		return false;
	float Left = 0;
	const bool Frozen = Runtime->GetStatusEffect(Id, EStatusEffect::Control, Left) > 0;
	const double Action = FMath::Max(.1f, 1 + Runtime->GetStatusEffect(Id, EStatusEffect::ActionSpeed, Left) / 100);
	const int32 Skill =
		FDWMonsterRules::SelectSkill(State->Compiled, State->Cooldowns, DeltaSeconds,
									 FVector::Dist2D(Location, TargetLocation), Action, Frozen, TargetAlive);
	if (Skill < 0)
		return false;
	const auto &S = State->Compiled.Skills[Skill];
	const FVector Aim = (TargetLocation - Location).GetSafeNormal2D();
	const FVector Origin = Location + Aim * 60;
	Runtime->UpdateOwner(State->Owner, FVector3f(Origin) * .01f, FVector3f(Aim));
	if (const auto *M = CombatEnemy(Id))
	{
		State->Character.Life = M->Health;
		State->Character.Shield = M->Shield;
	}
	FString Error;
	const int32 Plan = FDWMonsterRules::OffensePlan(*Host, State->Compiled, State->Character, S.Plan, Error);
	Runtime->SetOwnerTargetPoint(State->Owner,
								 FVector3f(TargetLocation.X * .01f, TargetLocation.Y * .01f, 0));
	const bool Cast = Plan >= 0 && CastBuildPower(Plan, State->Owner, Origin, Aim, float(S.Power)) >= 0;
	if (Cast)
 {
  ++MonsterCastCounter;
  const auto* CastDefinition=Host->Library.Builds.Find(S.Id);
  State->LastSkill=CastDefinition?CastDefinition->DisplayName:S.Id.ToString();
  State->LastCastAt=Player?Player->Combat.Time:0;
  const double Time=Player?Player->Combat.Time:0;
  if(Time-LastMonsterHUDLogAt>=.5)
  {
   LastMonsterHUDLogAt=Time;
   const auto* Build=Host->Library.Builds.Find(S.Id);
   AddHUDLog(State->Compiled.Definition.Name+TEXT(" 释放 ")+(Build?Build->DisplayName:S.Id.ToString()));
  }
 }
	return Cast;
}
float UDWRuleWorldSubsystem::GetMonsterSeekRange(int32 TargetId, float DefaultMeters) const
{
 const auto* State = MonsterStates.Find(ResolveTargetId(TargetId));
 return float(FMath::Max(0., State && State->Character.Sheet.Stats.Contains(TEXT("monster.seek_range"))
  ? State->Character.S(TEXT("monster.seek_range")) : double(DefaultMeters)));
}
float UDWRuleWorldSubsystem::GetMonsterMoveScale(int32 TargetId) const
{
	const uint32 Id = ResolveTargetId(TargetId);
	float Left = 0;
	if (Runtime->GetStatusEffect(Id, EStatusEffect::Control, Left) > 0)
		return 0;
	const auto *State = MonsterStates.Find(Id);
	const double EquipmentMove = State ? FMath::Max(.1, 1 + State->Character.MoveSpeed() / 100) : 1;
	return EquipmentMove *
		   FMath::Clamp(1 + Runtime->GetStatusEffect(Id, EStatusEffect::MoveSpeed, Left) / 100, .1f, 1.f) *
		   (Player ? Player->Combat.EnemyPace(Id) : 1);
}

bool UDWRuleWorldSubsystem::SetPlayerBeltSlot(int32 Slot,FName Id,FString& Error)
{ auto Next=PlayerProfile; return Next.SetBeltSlot(Host->Library,Slot,Id,Error) && ApplyPlayerProfile(MoveTemp(Next),Error); }
bool UDWRuleWorldSubsystem::QuotePlayerEquipmentSale(const FString& Id,int64& Price,FString& Error) const
{ return PlayerProfile.QuoteSale(Host->Library,Id,Price,Error); }
bool UDWRuleWorldSubsystem::SellPlayerEquipment(const FString& Id,int64& Price,FString& Error)
{ if(IsPlayerItemLocked(Id)){Error=TEXT("该装备已被祭坛锁定");return false;} auto Next=PlayerProfile; return Next.Sell(Host->Library,Id,Price,Error) && ApplyPlayerProfile(MoveTemp(Next),Error); }
void UDWRuleWorldSubsystem::BeginPlayerFloorRewards(int32 Seed,int32 Floor,int32 Visit,int32 MaxTier)
{ FloorDropLibrary.Reset(); FloorRewards=MakeUnique<FDWRuleFloorRewards>(uint32(Seed),Floor,Visit,MaxTier); HUDLog.Reset(); HUDReceipts.Reset(); HUDDamageMeter.Reset(); HUDReceiptSerial=0; LastMonsterHUDLogAt=-1.e30; }
bool UDWRuleWorldSubsystem::SetPlayerFloorDropPool(const TArray<FName>& Equipment,const TArray<FName>& Items,FString& Error)
{
 if(!Host||!FloorRewards){Error=TEXT("floor rewards are not initialized");return false;}
 TSet<FName> EquipmentIds,ItemIds;
 for(FName Id:Equipment){const auto* D=Host->Library.Equipment.Find(Id);if(!D||FDWPawnEquipmentRules::IsFixedSlot(D->Slot)||EquipmentIds.Contains(Id)){Error=TEXT("invalid drop equipment: ")+Id.ToString();return false;}EquipmentIds.Add(Id);}
 for(FName Id:Items){const auto* D=Host->Library.Items.Find(Id);if(!D||ItemIds.Contains(Id)||(D->Category!=TEXT("consumable")&&D->Category!=TEXT("currency")&&D->Category!=TEXT("omen")&&D->Category!=TEXT("material"))){Error=TEXT("invalid drop item: ")+Id.ToString();return false;}ItemIds.Add(Id);}
 // Copy once per map and remove candidates in place, preserving native iteration
 // order. A full default pool produces the exact same rolls as the local rules.
 auto Next=MakeUnique<FDWRuleLibrary>(Host->Library);
 for(auto It=Next->Equipment.CreateIterator();It;++It)if(!EquipmentIds.Contains(It.Key()))It.RemoveCurrent();
 for(auto It=Next->Items.CreateIterator();It;++It)if(!ItemIds.Contains(It.Key()))It.RemoveCurrent();
 FloorDropLibrary=MoveTemp(Next);Error.Reset();return true;
}
void UDWRuleWorldSubsystem::CompletePlayerFloorRewards() { if(FloorRewards) FloorRewards->Complete(Host->Library); }
bool UDWRuleWorldSubsystem::PickupPlayerLoot(const FString& Key,FVector Position,FString& Error)
{
 if(!FloorRewards || !Player) { Error=TEXT("当前没有掉落"); return false; }
 auto NextProfile=PlayerProfile; auto NextRewards=*FloorRewards;
 if(!NextRewards.Pickup(Key,NextProfile,FVector2d(Position.X,Position.Y),Player->Character.bDead,Error) || !ApplyPlayerProfile(MoveTemp(NextProfile),Error)) return false;
 *FloorRewards=MoveTemp(NextRewards); return true;
}
bool UDWRuleWorldSubsystem::BuyPlayerMerchant(const FString& Key,FString& Error)
{
 if(!FloorRewards) { Error=TEXT("当前没有商人"); return false; }
 auto NextProfile=PlayerProfile; auto NextRewards=*FloorRewards;
 if(!NextRewards.Buy(Key,NextProfile,Error) || !ApplyPlayerProfile(MoveTemp(NextProfile),Error)) return false;
 *FloorRewards=MoveTemp(NextRewards); return true;
}

void UDWRuleWorldSubsystem::AddHUDLog(const FString& Text)
{
 HUDLog.Insert(FDWRuleHUDLog{Player?Player->Combat.Time:0,Text},0);
 if(HUDLog.Num()>20) HUDLog.SetNum(20);
}

void UDWRuleWorldSubsystem::AddHUDReceipt(FDWRuleHUDReceipt Receipt)
{
 if (!Runtime) return;
 Receipt.Serial=++HUDReceiptSerial; Receipt.Time=Runtime->GetClock();
 // Uncapped hit totals are aggregated before the independent floating-text budget.
 // One simulation frame owns a total and a set, as AverageTargetDps.record does.
 if(Receipt.bPlayerDamage&&!Receipt.bHero&&!Receipt.bStones) HUDDamageMeter.Record(Receipt.Time,Receipt.TargetId,Receipt.Amount);
 // Match the web's short-lived queues without consuming DrainDamage or retaining old floors.
 HUDReceipts.RemoveAll([&](const FDWRuleHUDReceipt& R){return Receipt.Time-R.Time>=1.2;});
 if (HUDReceipts.Num()>=700) HUDReceipts.RemoveAt(0,HUDReceipts.Num()-699,EAllowShrinking::No);
 HUDReceipts.Add(Receipt);
}
double UDWRuleWorldSubsystem::GetStatusValue(int32 TargetId,FName Status) const
{ return Runtime?Runtime->GetStatus(ResolveTargetId(TargetId),StatusKey(Status.ToString())):0; }
bool UDWRuleWorldSubsystem::GetMonsterHUD(int32 TargetId,FString& Name,FString& Skill,double& LastCastAt) const
{
 const auto* State=MonsterStates.Find(ResolveTargetId(TargetId)); if(!State) return false;
 Name=State->Compiled.Definition.Name; Skill=State->LastSkill; LastCastAt=State->LastCastAt; return true;
}

void UDWRuleWorldSubsystem::PlayerHUDDps(double AdditionalTime,double& Value,int32& Targets) const
{
 Value=0; Targets=0; if(!Runtime) return;
 HUDDamageMeter.Read(Runtime->GetClock()+FMath::Max(0.,AdditionalTime),Value,Targets);
}

// Transactional host-source replacement; compiler and combat semantics are unchanged.
bool UDWRuleWorldSubsystem::SetPlayerPassiveParts(const TArray<FDWCollectionPart>& Parts,FString& Error)
{
 if(!Player){Error=TEXT("player session is not enabled");return false;}
 const auto Previous=Player->Loadout;auto Next=Previous;Next.PassiveParts=Parts;
 if(!Player->SetLoadout(MoveTemp(Next),Error)){const FString Failed=Error;FString RestoreError;Player->SetLoadout(Previous,RestoreError);Error=Failed;return false;}
 PlayerProfile.Loadout.PassiveParts=Parts;SyncPlayerTarget();return true;
}
