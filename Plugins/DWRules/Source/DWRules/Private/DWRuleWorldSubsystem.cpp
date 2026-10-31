#include "DWRuleWorldSubsystem.h"

#include "DWRuleAssets.h"
#include "DWRuleCompiler.h"
#include "DWRuleProgramReader.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleSamples.h"
#include "DWRuleTargets.h"
#include "DWRuleTargetComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DWRuleVisualHost.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "MassEntityManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDWRules, Log, All);

using namespace DWRules;

namespace
{
	constexpr float CmToM = 0.01f;
	constexpr uint32 StaticTargetBit = 0x80000000u;
	constexpr uint32 ActorTargetBit = 0x40000000u;
	constexpr int32 MaxPendingDamageRows = 65536;

	FVector3f ToMeters(const FVector& Value)
	{
		return FVector3f(Value) * CmToM;
	}
}

void UDWRuleWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CreateRuntime();
}

void UDWRuleWorldSubsystem::Deinitialize()
{
	Player.Reset();
	Runtime = nullptr;
	Host.Reset();
	VisualHost = nullptr;
	Super::Deinitialize();
}

bool UDWRuleWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UDWRuleWorldSubsystem* UDWRuleWorldSubsystem::ForContext(const UObject* Context)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UDWRuleWorldSubsystem>() : nullptr;
}

void UDWRuleWorldSubsystem::CreateRuntime()
{
	Player.Reset();
	HUDReceipts.Reset(); HUDDamageMeter.Reset(); HUDReceiptSerial=0;
	MonsterStates.Reset(); MonsterPlans.Reset(); FrameEnemies.Reset(); FrameEnemyIds.Reset();
	FDWWorldConfig Settings;
	if (Config)
	{
		Settings.MaxProjectiles = Config->MaxProjectiles;
		Settings.WorkPerFrame = Config->WorkPerFrame;
		Settings.RootWorkCap = Config->RootWorkCap;
		Settings.MaxWaves = Config->MaxWaves;
		Settings.MaxDeferredEvents = Config->MaxDeferredEvents;
		Settings.FixedStep = 1.f / FMath::Max(15.f, Config->SimulationRate);
		Settings.MaxSubsteps = Config->MaxSubsteps;
		Settings.CellSize = Config->CellSizeMeters;
	}
	FDWRuleLibrary Previous = Host ? Host->Library : FDWRuleLibrary();
	Host = MakeUnique<FDWRuleHost>(Settings);
	Runtime = &Host->World;
	Runtime->SweepGeometry=[this](const FVector3f& Start,const FVector3f& End,float Radius,float& Time)
	{
		UWorld* W=GetWorld(); if (!W) return false;
		FHitResult Hit;
		// Rules run in the combat plane. A thin horizontal box ignores the walkable floor.
		const bool Blocked=W->SweepSingleByObjectType(Hit,FVector(Start)*100,FVector(End)*100,FQuat::Identity,
			FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeBox(FVector(FMath::Max(1.f,Radius*100),FMath::Max(1.f,Radius*100),1)),FCollisionQueryParams(SCENE_QUERY_STAT(DWRuleGeometry),false));
		Time=Hit.Time; return Blocked;
	};
	if (!Previous.Traits.IsEmpty()) { FString Error; Host->SetLibrary(MoveTemp(Previous), Error); }
}

void UDWRuleWorldSubsystem::Configure(UDWRuleWorldConfig* InConfig)
{
	Config = InConfig;
	CreateRuntime();
}

int32 UDWRuleWorldSubsystem::CompileSource(const FDWBuildSource& Source, FString& Error)
{
	return Host->CompileSource(Source, Error);
}

int32 UDWRuleWorldSubsystem::PlanWithGrantedTraits(int32 Plan, int32 Owner)
{
	FString Error;
	return Host->PlanWithGrantedTraits(Plan, Owner, Error);
}

bool UDWRuleWorldSubsystem::ApplyStatus(int32 TargetId, FName Status, float Value, float Duration)
{
	return Runtime && Runtime->ApplyStatusKey(ResolveTargetId(TargetId), StatusKey(Status.ToString()), Value, Duration);
}

void UDWRuleWorldSubsystem::RefreshGrantedDefense()
{
	Host->RefreshGrantedDefense();
}

int32 UDWRuleWorldSubsystem::CompileBuild(FName ActiveId, const TArray<UDWTraitDefinition*>& Traits, FString& Error)
{
	FDWBuildSource Source;
	Source.ActiveId = ActiveId;
	for (const UDWTraitDefinition* Asset : Traits)
	{
		if (!Asset)
		{
			continue;
		}
		FDWTraitSource Trait;
		if (!FDWRuleProgramReader::ReadTrait(*Asset, Trait, Error))
		{
			return INDEX_NONE;
		}
		Source.Traits.Add(MoveTemp(Trait));
	}
	return CompileSource(Source, Error);
}

int32 UDWRuleWorldSubsystem::CompileSampleBuild(FString& Error)
{
	return CompileSource(FDWRuleSamples::StackedIceLance(), Error);
}

int32 UDWRuleWorldSubsystem::RegisterOwner(int32 ExternalId, int32 Team)
{
	return Runtime->AddOwner(uint32(ExternalId), uint8(FMath::Clamp(Team, 0, 255)));
}

void UDWRuleWorldSubsystem::UpdateOwner(int32 Owner, FVector Location, FVector Aim)
{
	Runtime->UpdateOwner(Owner, ToMeters(Location), FVector3f(Aim));
	// Buffs on the caster live under its target id, so its own defenses (stoneskin) see them too.
	const uint32 ExternalId = Runtime->GetOwnerExternalId(Owner);
	if (ExternalId != InvalidId)
	{
		Runtime->SetOwnerStatusId(Owner, ResolveTargetId(int32(ExternalId)));
	}
}

float UDWRuleWorldSubsystem::GetStatusEffect(int32 TargetId, FName Effect, float& Remaining) const
{
	Remaining = 0.f;
	EStatusEffect Type = EStatusEffect::Num;
	if (!Runtime || !ParseStatusEffect(Effect.ToString(), Type))
	{
		return 0.f;
	}
	return Runtime->GetStatusEffect(ResolveTargetId(TargetId), Type, Remaining);
}

int32 UDWRuleWorldSubsystem::GetStatusStacks(int32 TargetId, FName Status) const
{
	return Runtime ? Runtime->GetStatusStacks(ResolveTargetId(TargetId), StatusKey(Status.ToString())) : 0;
}

void UDWRuleWorldSubsystem::SetOwnerTargetPoint(int32 Owner, FVector Point)
{
	Runtime->SetOwnerTargetPoint(Owner, ToMeters(Point));
}

int32 UDWRuleWorldSubsystem::CastBuild(int32 Plan, int32 Owner, FVector Origin, FVector Direction)
{
	return CastBuildPower(Plan,Owner,Origin,Direction,0);
}

int32 UDWRuleWorldSubsystem::CastBuildPower(int32 Plan, int32 Owner, FVector Origin, FVector Direction, float Power)
{
	const uint64 Seed = Mix64(++CastCounter ^ (uint64(Plan) << 32) ^ uint64(Owner));
	if (const auto* O=Runtime->GetOwner(Owner); O && O->Team==0)
	{
		FString Error;
		if (EnablePlayer(Owner,Error))
			return Player->Cast(Plan,ToMeters(Origin),FVector3f(Direction),Power,Seed,Error);
	}
	return Runtime->Cast(PlanWithGrantedTraits(Plan, Owner), Owner, ToMeters(Origin), FVector3f(Direction), Seed, Power, Runtime->GetOwner(Owner) ? Runtime->GetOwner(Owner)->TargetPoint : TOptional<FVector3f>());
}

float UDWRuleWorldSubsystem::GetStatusRemaining(int32 TargetId, FName Status) const
{
	return Runtime ? Runtime->GetStatusRemaining(ResolveTargetId(TargetId), StatusKey(Status.ToString())) : 0.f;
}

void UDWRuleWorldSubsystem::UpdateActorTarget(AActor* Actor, int32 ExternalId, int32 Team, float Health, float MaxHealth, float RadiusCm)
{
	if (!Actor)
	{
		return;
	}
	UDWRuleTargetComponent* Target = Actor->FindComponentByClass<UDWRuleTargetComponent>();
	if (!Target)
	{
		Target = NewObject<UDWRuleTargetComponent>(Actor, TEXT("DWRuleTarget"));
		Target->ExternalId = ExternalId;
		Target->Team = uint8(FMath::Clamp(Team, 0, 255));
		Target->MaxHealth = FMath::Max(1.f, MaxHealth);
		Target->RegisterComponent();
	}
	Target->ExternalId = ExternalId;
	Target->Team = uint8(FMath::Clamp(Team, 0, 255));
	Target->MaxHealth = FMath::Max(1.f, MaxHealth);
	Target->Health = FMath::Max(0.f, Health);
    // Script GameMode BeginPlay can precede component BeginPlay. Bind its stable id now.
    if(!ActorTargets.Contains(Target)) RegisterActorTarget(Target);
	MapGameId(uint32(Target->TargetId),ExternalId);
	// Native character pools stay authoritative when a legacy script republishes its attributes.
	if (Player && uint32(Target->TargetId)==Player->TargetId) SyncPlayerTarget();
	if (RadiusCm > 0.f)
	{
		Target->RadiusCm = RadiusCm;
	}
}

TArray<FDWRuleDamageRow> UDWRuleWorldSubsystem::DrainDamage()
{
	TArray<FDWRuleDamageRow> Rows = MoveTemp(PendingDamage);
	PendingDamage.Reset();
	return Rows;
}

int32 UDWRuleWorldSubsystem::AddStaticTarget(FVector Location, float RadiusCm, float Health, int32 Team)
{
	FStaticTarget Target;
	Target.Position = ToMeters(Location);
	Target.Radius = FMath::Max(1.f, RadiusCm) * CmToM;
	Target.Health = Health;
	Target.MaxHealth = Health;
	Target.Team = uint8(FMath::Clamp(Team, 0, 255));
	return int32(StaticTargetBit | uint32(StaticTargets.Add(Target)));
}

void UDWRuleWorldSubsystem::ClearStaticTargets()
{
	for (int32 Index = 0; Index < StaticTargets.Num(); ++Index)
	{
		ForgetTarget(StaticTargetBit | uint32(Index));
	}
	StaticTargets.Reset();
}

void UDWRuleWorldSubsystem::BeginTargets()
{
	Runtime->Targets().Reset();
	MassTargets.Reset();
	GameIdByTarget.Reset();
	TargetByGameId.Reset();
	FrameEnemyIds.Reset();
}

void UDWRuleWorldSubsystem::MapGameId(uint32 Internal, int32 GameId)
{
	if (GameId > 0)
	{
		GameIdByTarget.Add(Internal, GameId);
		TargetByGameId.Add(GameId, Internal);
	}
}

uint32 UDWRuleWorldSubsystem::ResolveTargetId(int32 Id) const
{
	const uint32* Internal = TargetByGameId.Find(Id);
	return Internal ? *Internal : uint32(Id);
}

void UDWRuleWorldSubsystem::AddMassTarget(const FMassEntityHandle& Entity, const FVector& Location, const FDWRuleTargetFragment& Target)
{
	// Game ids are unique per spawn, so statuses never leak into a pooled/reused Mass handle.
	const uint32 ExternalId = Target.ExternalId > 0 ? uint32(Target.ExternalId) : uint32(Entity.Index) + 1u;
	MassTargets.Add(ExternalId, Entity);
	MapGameId(ExternalId, Target.ExternalId);
	ExportCombatTarget(ExternalId,ToMeters(Location),Target.RadiusCm*CmToM,Target.Health,Target.MaxHealth,Target.Shield,Target.MaxShield,Target.Team,Target.bElite,Target.Resistance);
}

void UDWRuleWorldSubsystem::StepFrame(float DeltaSeconds, FMassEntityManager* EntityManager)
{
	if (!bSimulationEnabled)
	{
		if (VisualHost) VisualHost->ClearWebPresentation();
		return;
	}
	if (!Runtime || LastFrame == GFrameCounter)
	{
		return;
	}
	LastFrame = GFrameCounter;
	if (Player)
	{
		bool Moving=false;
		for (const auto& Weak : ActorTargets)
			if (const auto* T=Weak.Get(); T && uint32(T->TargetId)==Player->TargetId && T->GetOwner()) Moving=!T->GetOwner()->GetVelocity().IsNearlyZero();
		Player->TickCharacter(FMath::Min(DeltaSeconds,.05f),Moving);
		SyncPlayerTarget();
	}
	FDWTargets& Targets = Runtime->Targets();
	for (int32 Index = 0; Index < StaticTargets.Num(); ++Index)
	{
		const FStaticTarget& Target = StaticTargets[Index];
		ExportCombatTarget(StaticTargetBit|uint32(Index),Target.Position,Target.Radius,Target.Health,Target.MaxHealth,0,0,Target.Team,false,nullptr);
	}
	for (int32 Index = 0; Index < ActorTargets.Num(); ++Index)
	{
		const UDWRuleTargetComponent* Target = ActorTargets[Index].Get();
		const AActor* Owner = Target ? Target->GetOwner() : nullptr;
		if (Owner && Target->Health > 0.f)
		{
			MapGameId(ActorTargetBit | uint32(Index), Target->ExternalId);
			ExportCombatTarget(ActorTargetBit|uint32(Index),ToMeters(Owner->GetActorLocation()),Target->RadiusCm*CmToM,Target->Health,Target->MaxHealth,Target->Shield,Target->MaxShield,Target->Team,Target->bElite,Target->Resistance);
		}
	}

	if (Player)
	{
		Player->Combat.Enemies.Reset();
		for (uint32 Id : FrameEnemyIds) Player->Combat.Enemies.Add(FrameEnemies[Id]);
		Player->BeforeWorld(FMath::Min(DeltaSeconds,.05f));
	}
	Host->Step(DeltaSeconds);
	if (Player) { Player->AfterWorld(); SyncPlayerTarget(); }
	// Publish every pool, including recovery on frames with no damage.
	if (Player)
		for (const auto& M : Player->Combat.Enemies)
		{
			FrameEnemies.Add(M.Id,M);
			if (M.Id & StaticTargetBit)
			{
				const int32 I=int32(M.Id & ~StaticTargetBit); if (StaticTargets.IsValidIndex(I)) StaticTargets[I].Health=M.Health;
			}
			else if (M.Id & ActorTargetBit)
			{
				const int32 I=int32(M.Id & ~ActorTargetBit); if (auto* T=ActorTargets.IsValidIndex(I)?ActorTargets[I].Get():nullptr) { T->Health=M.Health; T->Shield=M.Shield; }
			}
			else if (const auto* E=MassTargets.Find(M.Id); E && EntityManager && EntityManager->IsEntityValid(*E))
				if (auto* F=EntityManager->GetFragmentDataPtr<FDWRuleTargetFragment>(*E)) { F->Health=M.Health; F->Shield=M.Shield; }
		}

	// Commit the mirror's damage to the authoritative stores, then expose a summary.
	for (const FDWDamageRecord& Record : Runtime->DamageLog())
	{
		const auto* NativeEnemy=Player ? Player->Combat.Enemy(Record.TargetExternalId) : nullptr;
		const bool NativeHero=Player && Record.TargetExternalId==Player->TargetId;
		const bool NativeResolved=NativeEnemy || NativeHero;
  // Enemy amounts are already applied; hero amounts are observed after Character.TakeHit instead.
  if (NativeEnemy && Record.Amount>0)
  {
   FDWRuleHUDReceipt Receipt; Receipt.Position=FVector(NativeEnemy->Position)*100;
   Receipt.Amount=Record.Amount; Receipt.Element=Record.Element; Receipt.bCritical=Record.bCritical;
   Receipt.TargetId=Record.TargetExternalId; Receipt.bPlayerDamage=Player && Record.OwnerExternalId==Runtime->GetOwnerExternalId(Player->Combat.Hero.Owner);
   AddHUDReceipt(Receipt);
  }
		if (Record.TargetExternalId & StaticTargetBit)
		{
			const int32 Index = int32(Record.TargetExternalId & ~StaticTargetBit);
			if (StaticTargets.IsValidIndex(Index))
			{
				if (!NativeResolved) StaticTargets[Index].Health -= Record.Amount;
			}
		}
		else if (Record.TargetExternalId & ActorTargetBit)
		{
			const int32 Index = int32(Record.TargetExternalId & ~ActorTargetBit);
			if (UDWRuleTargetComponent* Target = ActorTargets.IsValidIndex(Index) ? ActorTargets[Index].Get() : nullptr)
			{
				if (!NativeResolved) Target->Health = FMath::Max(0.f, Target->Health - Record.Amount);
				Target->OnRuleDamaged.Broadcast(Record.Amount, int32(Record.Element), NativeHero ? Player->Character.bDead : Record.bKilled);
			}
		}
		else if (const FMassEntityHandle* Entity = MassTargets.Find(Record.TargetExternalId))
		{
			if (EntityManager && EntityManager->IsEntityValid(*Entity))
			{
				if (FDWRuleTargetFragment* Fragment = EntityManager->GetFragmentDataPtr<FDWRuleTargetFragment>(*Entity))
				{
					if (!NativeResolved) Fragment->Health = FMath::Max(0.f, Fragment->Health - Record.Amount);
				}
			}
		}
		if (PendingDamage.Num() < MaxPendingDamageRows)
		{
			FDWRuleDamageRow Row;
			const int32* GameId = GameIdByTarget.Find(Record.TargetExternalId);
			Row.TargetId = GameId ? *GameId : int32(Record.TargetExternalId);
			Row.OwnerId = int32(Record.OwnerExternalId);
			Row.Amount = Record.Amount;
			Row.Element = Record.Element;
			Row.bCritical = Record.bCritical;
			Row.bKilled = Record.bKilled;
			Row.bCharacterResolved=NativeResolved;
			if (NativeHero) { Row.Health=Player->Character.Life; Row.Shield=Player->Character.Shield; Row.bKilled=Player->Character.bDead; }
			else if (NativeEnemy) { Row.Health=NativeEnemy->Health; Row.Shield=NativeEnemy->Shield; }
			PendingDamage.Add(Row);
		}
		if (NativeEnemy && Record.bKilled)
		{
			if(FloorRewards)
   {
    int32 Level=1;
    if(const auto* Monster=MonsterStates.Find(Record.TargetExternalId))
    {
     const auto& Equipment=Monster->Compiled.Definition.Equipment;
     const auto* Tower=Equipment.Overrides.Find(TEXT("tower")); if(!Tower) Tower=Host->Library.Equipment.Find(Equipment.Ids.FindRef(TEXT("tower")));
     if(Tower) for(const auto& T:Tower->Traits) if(T.Id==TEXT("monster_tower_level")) if(const auto* Value=T.Params.Find(TEXT("level"))) Level=int32(*Value);
    }
    const int64 PaidStones=FloorRewards->KillStones(NativeEnemy->MaxHealth,Level);
    PlayerProfile.RecordKill(PaidStones);
    if(PaidStones>0) { FDWRuleHUDReceipt Receipt; Receipt.Position=FVector(NativeEnemy->Position)*100; Receipt.Amount=double(PaidStones); Receipt.bStones=true; AddHUDReceipt(Receipt); }
    FloorRewards->OnKill(FloorDropLibrary?*FloorDropLibrary:Host->Library,FVector2d(NativeEnemy->Position.X*100,NativeEnemy->Position.Y*100),NativeEnemy->Rarity==TEXT("boss"));
   }
   else { FString Error; PlayerProfile.AwardKill(Host->Library,NativeEnemy->MaxHealth,Error); }
		}
	}
	const double RenderStart = FPlatformTime::Seconds();
	SyncVisuals();
	Runtime->DamageLog().Reset();
	const FDWFrameStats& Stats = Runtime->GetStats();
	Report.Projectiles = Stats.Projectiles;
	Report.Targets = Targets.Num();
	Report.Contacts = Stats.Contacts;
	Report.Hits = Stats.Hits;
	Report.Kills = Stats.Kills;
	Report.Events = Stats.Events;
	Report.Actions = Stats.Actions;
	Report.Deferred = Stats.Deferred;
	Report.DroppedEvents = Stats.DroppedEvents;
	Report.BudgetStops = Stats.BudgetStops;
	Report.CapacityDrops = Stats.CapacityDrops;
	Report.StatusApplied = Stats.StatusApplied;
	Report.Statuses = Stats.Statuses;
	Report.SimulationMs = float(Stats.TotalMs);
	Report.MotionMs = float(Stats.MotionMs);
	Report.RulesMs = float(Stats.RulesMs);
	Report.RenderMs = float((FPlatformTime::Seconds() - RenderStart) * 1000.0);
	Report.MonsterCasts=MonsterCastCounter;
}

void UDWRuleWorldSubsystem::SyncVisuals()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		if (Player) Player->DrainEffects();
		return;
	}
	if (!VisualHost)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		VisualHost = World->SpawnActor<ADWRuleVisualHost>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!VisualHost)
		{
			return;
		}
	}
	VisualHost->Sync(*Runtime, Config);
	if (Player) VisualHost->SyncEffects(Player->DrainEffects(),GetWorld()->GetDeltaSeconds(),Config);
	VisualHost->SyncWebCombat(*Runtime,Player?&Player->Combat:nullptr,Host->Library);
}

bool UDWRuleWorldSubsystem::LoadRuleLibrary(const FString& Dir, FString& Error)
{
	FDWRuleLibrary Library;
	if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::Combine(FPaths::ProjectContentDir(), Dir), Library, Error)) return false;
	const auto Previous=Host->Library;
	if (!Host->SetLibrary(MoveTemp(Library), Error)) return false;
	if (Player && Player->bConfigured && !Player->SetLoadout(Player->Loadout,Error))
	{
		const FString Failed=Error; FString RestoreError; Host->SetLibrary(Previous,RestoreError); Player->SetLoadout(Player->Loadout,RestoreError); Error=Failed; return false;
	}
	MonsterPlans.Reset();
	UE_LOG(LogDWRules, Display, TEXT("Rule library %s: %d traits, %d builds, %d statuses"), *Dir, Host->Library.Traits.Num(), Host->Library.Builds.Num(), Host->Library.Statuses.Num());
	return true;
}

int32 UDWRuleWorldSubsystem::CompileTraits(FName ActiveId, const TArray<FName>& TraitIds, FString& Error)
{
	return Host->CompileTraits(ActiveId, TraitIds, Error);
}

int32 UDWRuleWorldSubsystem::CompileNamedBuild(FName BuildId, const TArray<FName>& ExtraTraits, FString& Error)
{
	return Host->CompileNamedBuild(BuildId, ExtraTraits, Error);
}

bool UDWRuleWorldSubsystem::SetTargetDefenseTraits(int32 TargetId, const TArray<FName>& TraitIds, FString& Error)
{
	Error.Reset();
	if (!Host || uint32(TargetId) == InvalidId) { Error = TEXT("invalid rule target or runtime"); return false; }
	return Host->SetTargetDefenseTraits(ResolveTargetId(TargetId), TraitIds, Error);
}

FString UDWRuleWorldSubsystem::GetTraitDisplayName(FName TraitId) const
{
	const auto* Trait = Host ? Host->Library.Traits.Find(TraitId) : nullptr;
	return Trait && !Trait->DisplayName.IsEmpty() ? Trait->DisplayName : TraitId.ToString();
}

FString UDWRuleWorldSubsystem::GetBuildDisplayName(FName BuildId) const
{
	const auto* Build = Host ? Host->Library.Builds.Find(BuildId) : nullptr;
	return Build && !Build->DisplayName.IsEmpty() ? Build->DisplayName : BuildId.ToString();
}

float UDWRuleWorldSubsystem::GetStatus(int32 TargetId, FName Status) const
{
	return GetStatusByKey(ResolveTargetId(TargetId), StatusKey(Status.ToString()));
}

float UDWRuleWorldSubsystem::GetStatusByKey(uint32 TargetId, uint64 Key) const
{
	return Runtime ? Runtime->GetStatus(TargetId, Key) : 0.f;
}

void UDWRuleWorldSubsystem::ForgetTarget(uint32 TargetId)
{
	if (const auto* State=MonsterStates.Find(TargetId)) Runtime->RetireOwner(State->Owner);
	if (Player) Player->Forget(TargetId); else if (Host) Host->ForgetTarget(TargetId);
	MonsterStates.Remove(TargetId); FrameEnemies.Remove(TargetId);
}
void UDWRuleWorldSubsystem::ForgetGameTarget(int32 TargetId) { ForgetTarget(ResolveTargetId(TargetId)); }

void UDWRuleWorldSubsystem::SetVisual(FName Kind, UStaticMesh* Mesh, UMaterialInterface* Material, FVector ScalePerMeter)
{
	if (!Config)
	{
		Config = NewObject<UDWRuleWorldConfig>(this);
	}
	FDWRuleVisual& Visual = Config->Visuals.FindOrAdd(Kind);
	Visual.Mesh = Mesh;
	Visual.Material = Material;
	Visual.ScalePerMeter = ScalePerMeter;
}

void UDWRuleWorldSubsystem::RegisterActorTarget(UDWRuleTargetComponent* Target)
{
	if (!Target)
	{
		return;
	}
    const int32 Existing=ActorTargets.IndexOfByKey(Target);
    if(Existing!=INDEX_NONE){Target->TargetId=int32(ActorTargetBit|uint32(Existing));MapGameId(uint32(Target->TargetId),Target->ExternalId);return;}
	int32 Index = ActorTargets.IndexOfByPredicate([](const TWeakObjectPtr<UDWRuleTargetComponent>& Slot) { return !Slot.IsValid(); });
	if (Index == INDEX_NONE)
	{
		Index = ActorTargets.Add(Target);
	}
	else
	{
		ActorTargets[Index] = Target;
	}
	Target->TargetId = int32(ActorTargetBit | uint32(Index));
	ForgetTarget(uint32(Target->TargetId));
	MapGameId(uint32(Target->TargetId),Target->ExternalId);
}

void UDWRuleWorldSubsystem::UnregisterActorTarget(UDWRuleTargetComponent* Target)
{
	const int32 Index = ActorTargets.IndexOfByKey(Target);
	if (Index != INDEX_NONE)
	{
		if (Player && Player->TargetId==uint32(Target->TargetId))
		{
			Runtime->RetireOwner(Player->Combat.Hero.Owner);
			Player.Reset();
		}
		ForgetTarget(uint32(Target->TargetId));
		ActorTargets[Index] = nullptr;
	}
}

float UDWRuleWorldSubsystem::GetPlanStat(int32 Plan, const FString& Stat) const
{
	uint16 Id = 0;
	if (!Runtime || Plan < 0 || !FindStat(Stat, Id))
	{
		return 0.f;
	}
	return Runtime->GetPlan(Plan).Stats.Get(Id);
}
