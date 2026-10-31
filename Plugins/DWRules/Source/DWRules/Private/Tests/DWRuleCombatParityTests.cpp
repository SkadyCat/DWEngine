#include "DWRuleAilments.h"
#include "DWRuleCombat.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleMinions.h"
#include "DWRuleProgramReader.h"
#include "DWRuleProjectileAffixes.h"
#include "DWRuleZones.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
namespace
{
using FCombatObject = TSharedPtr<FJsonObject>;
using FCombatValue = TSharedPtr<FJsonValue>;
FCombatValue CombatNumber(double N)
{
	return MakeShared<FJsonValueNumber>(N);
}
FCombatObject CombatSnapshot(FDWCombatRuntime &R)
{
	auto O = MakeShared<FJsonObject>();
	O->SetNumberField(TEXT("time"), R.Time);
	O->SetNumberField(TEXT("life"), R.Character.Life);
	O->SetNumberField(TEXT("shield"), R.Character.Shield);
	O->SetNumberField(TEXT("mana"), R.Character.Mana);
	O->SetNumberField(TEXT("cooldown"), R.Cooldown);
	O->SetNumberField(TEXT("charges"), R.Character.Charges);
	O->SetNumberField(TEXT("empowered"), R.Character.PendingEmpower);
	O->SetNumberField(TEXT("triggered"), R.Triggers->Fired);
	O->SetNumberField(TEXT("recalled"), R.Projectiles->Recalled);
	O->SetNumberField(TEXT("projectiles"), R.World.Projectiles().Num);
	O->SetNumberField(TEXT("summons"), R.World.GetSummons().Num());
	O->SetNumberField(TEXT("zones"), R.Zones->Zones.Num());
	TArray<FCombatValue> Enemies;
	for (const auto &M : R.Enemies)
	{
		auto V = MakeShared<FJsonObject>();
		V->SetNumberField(TEXT("id"), M.Id);
		V->SetNumberField(TEXT("health"), M.Health);
		V->SetNumberField(TEXT("shield"), M.Shield);
		V->SetNumberField(TEXT("x"), M.Position.X);
		V->SetNumberField(TEXT("y"), M.Position.Y);
		V->SetNumberField(TEXT("poison"), R.Ailments->Layers(M.Id, TEXT("poison")));
		V->SetNumberField(TEXT("erosion"), R.Ailments->Layers(M.Id, TEXT("erosion")));
		V->SetNumberField(TEXT("shock"), R.Ailments->ShockLayers(M.Id));
		V->SetNumberField(TEXT("ignite"), R.Ailments->Layers(M.Id, TEXT("ignite")));
		TArray<FCombatValue> Statuses;
		for (const TCHAR *Name :
			 {TEXT("poison"), TEXT("erosion"), TEXT("shock"), TEXT("ignite"), TEXT("chill"), TEXT("freeze")})
			Statuses.Add(CombatNumber(R.Status(M.Id, Name)));
		V->SetArrayField(TEXT("statuses"), Statuses);
		Enemies.Add(MakeShared<FJsonValueObject>(V));
	}
	O->SetArrayField(TEXT("enemies"), Enemies);
	TArray<FCombatValue> Minions;
	for (auto *St : R.Minions->Alive())
	{
		const auto &Sm = R.Minions->SummonOf(*St);
		auto V = MakeShared<FJsonObject>();
		V->SetNumberField(TEXT("id"), St->Id);
		V->SetNumberField(TEXT("life"), St->Life);
		V->SetNumberField(TEXT("shield"), St->Shield);
		V->SetNumberField(TEXT("x"), Sm.Position.X);
		V->SetNumberField(TEXT("y"), Sm.Position.Y);
		Minions.Add(MakeShared<FJsonValueObject>(V));
	}
	O->SetArrayField(TEXT("minions"), Minions);
	return O;
}
bool CombatCompare(const FCombatValue &Expected, const FCombatValue &Actual, const FString &Path, FString &Error)
{
	if (!Expected || !Actual || Expected->Type != Actual->Type)
	{
		Error = Path + TEXT(": missing value or type mismatch");
		return false;
	}
	if (Expected->Type == EJson::Number)
	{
		const double E = Expected->AsNumber(), A = Actual->AsNumber();
		if (!FMath::IsFinite(A) || FMath::Abs(E - A) > FMath::Max(1.e-5, FMath::Abs(E) * 2.e-6))
		{
			Error = FString::Printf(TEXT("%s expected %.12g got %.12g"), *Path, E, A);
			return false;
		}
	}
	else if (Expected->Type == EJson::String)
	{
		if (Expected->AsString() != Actual->AsString())
		{
			Error = Path + TEXT(": expected ") + Expected->AsString() + TEXT(" got ") + Actual->AsString();
			return false;
		}
	}
	else if (Expected->Type == EJson::Object)
	{
		for (const auto &P : Expected->AsObject()->Values)
		{
			const auto *V = Actual->AsObject()->Values.Find(P.Key);
			if (!V || !CombatCompare(P.Value, *V, Path + TEXT(".") + FString(P.Key), Error))
				return false;
		}
	}
	else if (Expected->Type == EJson::Array)
	{
		const auto &E = Expected->AsArray();
		const auto &A = Actual->AsArray();
		if (E.Num() != A.Num())
		{
			Error = FString::Printf(TEXT("%s length expected %d got %d"), *Path, E.Num(), A.Num());
			return false;
		}
		for (int32 I = 0; I < E.Num(); ++I)
			if (!CombatCompare(E[I], A[I], Path + FString::Printf(TEXT("[%d]"), I), Error))
				return false;
	}
	return true;
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWCombatParityTest, "DWRules.Migration.CombatParity",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWCombatParityTest::RunTest(const FString &Parameters)
{
	FString Text, Error;
	const FString File = FPaths::ProjectSavedDir() / TEXT("RulesMigration/combat-parity.json");
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		AddError(TEXT("Run make-native-combat-parity.mjs first"));
		return false;
	}
	FCombatObject Reference;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Reference))
	{
		AddError(TEXT("invalid combat reference"));
		return false;
	}
	FDWRuleLibrary Library;
	if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir() / TEXT("Rules"), Library, Error))
	{
		AddError(Error);
		return false;
	}
	int32 Failed = 0, RowsChecked = 0, CasesChecked = 0;
	const double Dt = Reference->GetNumberField(TEXT("dt"));
	const int32 Frames = int32(Reference->GetNumberField(TEXT("frames")));
	for (const auto &CaseValue : Reference->GetArrayField(TEXT("cases")))
	{
		const auto Case = CaseValue->AsObject();
		const FString Id = Case->GetStringField(TEXT("id"));
		++CasesChecked;
		FDWRuleHost Host;
		if (!Host.SetLibrary(Library, Error))
		{
			AddError(Error);
			return false;
		}
		FDWRuleCharacter C(5);
		TArray<FDWHeldTrait> Held;
		TArray<FDWTraitSource> Sources;
		for (const auto &V : Case->GetArrayField(TEXT("entries")))
		{
			FDWTraitEntry E;
			FDWTraitSource S;
			if (!FDWRuleProgramReader::ReadTraitEntry(V, E, Error) || !Library.SourceFromEntry(E, S, Error))
			{
				AddError(Id + TEXT(": ") + Error);
				return false;
			}
			FDWHeldTrait H;
			H.Source = S;
			Held.Add(H);
			Sources.Add(S);
		}
		if (!C.SetSheet(Held, Error))
		{
			AddError(Error);
			return false;
		}
		const int32 Owner = Host.World.AddOwner(1, 0);
		Host.World.SetOwnerStatusId(Owner, 0x40000000);
		Host.World.UpdateOwner(Owner, FVector3f::ZeroVector, FVector3f(1, 0, 0));
		FDWCombatRuntime R(Host, C, 7);
		R.Hero.Owner = Owner;
		R.Hero.Power = 30;
		R.Hero.Skill = FName(*Case->GetStringField(TEXT("skill")));
		FDWBuildSource Base;
		if (!Library.NamedBuildSource(R.Hero.Skill, {}, Base, Error))
		{
			AddError(Error);
			return false;
		}
		FDWBuildSource Initial = Base;
		Initial.Traits.Append(Sources);
		R.Hero.Plan = Host.CompileSource(Initial, Error);
		if (R.Hero.Plan == INDEX_NONE)
		{
			AddError(Error);
			return false;
		}
		for (const auto &MV : Case->GetArrayField(TEXT("monsters")))
		{
			const auto M = MV->AsObject();
			FDWCombatEnemy Enemy;
			Enemy.Id = uint32(M->GetNumberField(TEXT("id")));
			Enemy.Position = {M->GetNumberField(TEXT("x")) / 100, M->GetNumberField(TEXT("y")) / 100, .6};
			Enemy.Radius = .4;
			Enemy.Health = Enemy.MaxHealth = 10000;
			R.Enemies.Add(Enemy);
		}
		for (const auto &M : R.Enemies)
		{
			Host.World.ApplyStatusKey(M.Id, StatusKey(TEXT("shock")), 20, 3, Owner);
			Host.World.ApplyStatusKey(M.Id, StatusKey(TEXT("ignite")), 12, 4, Owner);
			Host.World.ApplyStatusKey(M.Id, StatusKey(TEXT("chill")), 30, 2, Owner);
			FDWAilmentApplication O;
			O.Amount = 30;
			O.Element = 3;
			R.Ailments->ApplyPoison(M.Id, O);
			O.Element = 0;
			R.Ailments->ApplyErosion(M.Id, O);
		}
		TArray<FCombatValue> ActualRows,ActualStates;
		bool Match = true;
		int32 State = 0;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			C.Tick(Dt, Frame % 60 < 30);
			if (Frame % 45 == 0)
			{
				const auto Pay = C.BeforeCast(2, R.IsAttack(), R.CostScale());
				if (Pay.bOk)
				{
					FDWBuildSource CastSource = Initial;
					FDWTraitSource Combat, Offense;
					if (R.PlanSource(Combat))
						CastSource.Traits.Add(Combat);
					if (C.Offense(Pay.DamageMore, Pay.AreaIncreased, Pay.CritBonus)
							.MakeSource(Host.World.GetPlan(R.Hero.Plan).Stats, Offense))
						CastSource.Traits.Add(Offense);
					const int32 Plan = Host.CompileSource(CastSource, Error);
					if (Plan == INDEX_NONE)
					{
						AddError(Error);
						return false;
					}
					const int32 Root =
						Host.World.Cast(Plan, Owner, FVector3f(.6f, 0, .6f), FVector3f(1, 0, 0), 100 + Frame, 30);
					R.OnHeroCast(Root, Pay);
				}
			}
			Host.World.Targets().Reset();
			for (const auto &M : R.Enemies)
				if (M.Health > 0)
					Host.World.Targets().Add(FVector3f(M.Position), .4f, float(M.Health + M.Shield), 1, M.Id,
											 M.Resistance);
			Host.World.Targets().Add(FVector3f(0, 0, .6f), .45f, float(C.Life + C.Shield), 0, 0x40000000);
			R.Minions->AddTargets();
			R.BeforeStep(Dt);
			Host.World.Step(float(Dt));
			if (Frame == 120)
			{
				FDWStrikeOptions O;
				O.Only = R.Enemies[0].Id;
				O.bArea = false;
				O.Effect = TEXT("fixture_kill");
				R.Strike(R.Enemies[0].Position, .01, 20000, 2, O);
			}
			R.ConsumeRows();
			for (const auto &Row : Host.World.DamageLog())
			{
				auto O = MakeShared<FJsonObject>();
				O->SetNumberField(TEXT("frame"), Frame);
				O->SetNumberField(TEXT("target"), Row.TargetExternalId);
				O->SetNumberField(TEXT("owner"), Row.OwnerExternalId);
				O->SetNumberField(TEXT("amount"), Row.Amount);
				O->SetNumberField(TEXT("element"), Row.Element);
				O->SetNumberField(TEXT("crit"), Row.bCritical);
				O->SetNumberField(TEXT("killed"), Row.bKilled);
				O->SetNumberField(TEXT("dot"), Row.bDot);
				O->SetStringField(TEXT("strike"), Row.Info.Strike.IsNone() ? FString() : Row.Info.Strike.ToString());
				ActualRows.Add(MakeShared<FJsonValueObject>(O));
			}
			Host.World.DamageLog().Reset();
			if ((Frame + 1) % 15 == 0)
			{
                ActualStates.Add(MakeShared<FJsonValueObject>(CombatSnapshot(R)));
				if (Match && !CombatCompare(Case->GetArrayField(TEXT("expected"))[State],
											MakeShared<FJsonValueObject>(CombatSnapshot(R)),
											FString::Printf(TEXT("%s.state[%d]"), *Id, State), Error))
				{
					AddError(Error);
					Match = false;
				}
				++State;
			}
		}
		if (Match && !CombatCompare(MakeShared<FJsonValueArray>(Case->GetArrayField(TEXT("rows"))),
									MakeShared<FJsonValueArray>(ActualRows), Id + TEXT(".rows"), Error))
		{
			AddError(Error);
			Match = false;
		}
        if(!Match){auto Dump=MakeShared<FJsonObject>();Dump->SetStringField(TEXT("id"),Id);Dump->SetArrayField(TEXT("rows"),ActualRows);Dump->SetArrayField(TEXT("expected"),ActualStates);FString Json;FJsonSerializer::Serialize(Dump,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("RulesMigration/failure-")+Id+TEXT(".json")));}
		RowsChecked += ActualRows.Num();
		if (!Match && ++Failed >= 20)
			break;
	}
	AddInfo(FString::Printf(TEXT("Combat parity: %d cases, %d damage rows, %d failed cases"), CasesChecked, RowsChecked,
							Failed));
	return Failed == 0;
}
#endif
