#include "DWRuleCharacter.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleProgramReader.h"
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
using FObject = TSharedPtr<FJsonObject>;
double Number(const FObject &O, const TCHAR *Key, double Default = 0)
{
	double N;
	return O->TryGetNumberField(Key, N) ? N : Default;
}
bool Bool(const FObject &O, const TCHAR *Key)
{
	bool V = false;
	O->TryGetBoolField(Key, V);
	return V;
}
TMap<FString, double> Snapshot(const FDWRuleCharacter &C)
{
	TMap<FString, double> O;
	const auto Off = C.Offense();
	const auto Block = C.BlockChances(), Evade = C.EvadeChances();
#define V(Key, Value) O.Add(TEXT(Key), Value)
	V("life", C.Life);
	V("shield", C.Shield);
	V("mana", C.Mana);
	V("maxLife", C.MaxLife);
	V("maxShield", C.MaxShield);
	V("maxMana", C.MaxMana);
	V("shieldCap", C.ShieldCap);
	V("rechargeDelay", C.RechargeDelay);
	V("rechargeRate", C.RechargeRate);
	V("stunned", C.Stunned);
	V("dead", C.bDead ? 1 : 0);
	V("evasion", C.EvasionValue());
	V("armour", C.ArmourValue());
	V("blockAttack", Block.Attack);
	V("blockSpell", Block.Spell);
	V("evadeAttack", Evade.Attack);
	V("evadeSpell", Evade.Spell);
	V("actionAttack", C.ActionSpeed(true));
	V("actionSpell", C.ActionSpeed(false));
	V("moveSpeed", C.MoveSpeed());
	V("critical", Off.Critical);
	V("area", Off.Area);
	for (int32 I = 0; I < NumElements; ++I)
	{
		const FString Name = ElementName(EElement(I));
		O.Add(TEXT("inc_") + Name, Off.Increased[I]);
		O.Add(TEXT("more_") + Name, Off.More[I]);
		O.Add(TEXT("flat_") + Name, Off.Flat[I]);
	}
	V("total_lifeRecovered", C.Totals.LifeRecovered);
	V("total_shieldRecovered", C.Totals.ShieldRecovered);
	V("total_lifeLost", C.Totals.LifeLost);
	V("total_shieldLost", C.Totals.ShieldLost);
	V("total_prevented", C.Totals.Prevented);
	V("total_manaRecovered", C.Totals.ManaRecovered);
	V("total_blocked", C.Totals.Blocked);
	V("total_evaded", C.Totals.Evaded);
	V("total_stuns", C.Totals.Stuns);
	V("total_cheatedDeath", C.Totals.CheatedDeath);
	V("total_triggers", C.Totals.Triggers);
	V("flow_lifeRestored", C.Flow.LifeRestored);
	V("flow_shieldRestored", C.Flow.ShieldRestored);
	V("flow_manaRestored", C.Flow.ManaRestored);
	V("flow_manaSpent", C.Flow.ManaSpent);
	V("flow_lifeSpent", C.Flow.LifeSpent);
	V("flow_shieldSpent", C.Flow.ShieldSpent);
	V("flow_lifeLost", C.Flow.LifeLost);
	V("flow_hitTaken", C.Flow.HitTaken);
	V("events", C.CombatEvents.Num());
	V("pendingCasts", C.PendingCasts.Num());
	V("pendingGrants", C.PendingGrants.Num());
#undef V
	return O;
}
bool Match(const FObject &Expected, const FDWRuleCharacter &C, FString &Error)
{
	for (const auto &P : Snapshot(C))
	{
		double N;
		if (!Expected->TryGetNumberField(P.Key, N) || FMath::Abs(N - P.Value) > 1.e-8 * (1 + FMath::Abs(N)))
		{
			Error = FString::Printf(TEXT("%s web=%.17g ue=%.17g"), *P.Key, Expected->GetNumberField(P.Key), P.Value);
			return false;
		}
	}
	const auto &Triggers = Expected->GetArrayField(TEXT("triggers"));
	if (Triggers.Num() != C.Sheet.Triggers.Num())
	{
		Error = TEXT("trigger count differs");
		return false;
	}
	for (int32 I = 0; I < Triggers.Num(); ++I)
	{
		const auto T = Triggers[I]->AsObject();
		const auto &Native = C.Sheet.Triggers[I];
		if (T->GetStringField(TEXT("name")) != Native.Name.ToString() ||
			T->GetStringField(TEXT("id")) != Native.Trait.ToString() ||
			FMath::Abs(Number(T, TEXT("cooldown")) - Native.Cooldown) > 1.e-8 ||
			FMath::Abs(Number(T, TEXT("acc")) - Native.Accumulator) > 1.e-8)
		{
			Error = TEXT("trigger runtime state differs: ") + Native.Name.ToString();
			return false;
		}
	}
	return true;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWMigrationCharacterParity, "DWRules.Migration.CharacterParity",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWMigrationCharacterParity::RunTest(const FString &Parameters)
{
	FString Text, Error;
	FObject Root;
	const FString File = FPaths::ProjectSavedDir() / TEXT("RulesMigration/character-parity.json");
	if (!FFileHelper::LoadFileToString(Text, *File) ||
		!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		AddError(
			TEXT("Generate reference first: node CustomTools/RuleStudio/scripts/make-native-character-parity.mjs"));
		return false;
	}
	FDWRuleLibrary Library;
	if (!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir() / TEXT("Rules"), Library, Error))
	{
		AddError(Error);
		return false;
	}
	const auto &Cases = Root->GetArrayField(TEXT("cases"));
	const auto &Ops = Root->GetArrayField(TEXT("ops"));
	int32 Checked = 0, Failures = 0;
	for (const auto &CaseValue : Cases)
	{
		const auto Case = CaseValue->AsObject();
		const FString Id = Case->GetStringField(TEXT("id"));
		TArray<FDWHeldTrait> Entries;
		bool Valid = true;
		for (const auto &EntryValue : Case->GetArrayField(TEXT("entries")))
		{
			const auto Obj = MakeShared<FJsonObject>(*EntryValue->AsObject());
			FDWHeldTrait Held;
			Held.bEquipment = Bool(Obj, TEXT("equipment"));
			FString Slot;
			Obj->TryGetStringField(TEXT("slot"), Slot);
			Held.Slot = FName(*Slot);
			Obj->RemoveField(TEXT("equipment"));
			Obj->RemoveField(TEXT("slot"));
			FDWTraitEntry Entry;
			if (!FDWRuleProgramReader::ReadTraitEntry(MakeShared<FJsonValueObject>(Obj), Entry, Error) ||
				!Library.SourceFromEntry(Entry, Held.Source, Error))
			{
				AddError(Id + TEXT(": ") + Error);
				Valid = false;
				break;
			}
			Entries.Add(MoveTemp(Held));
		}
		if (!Valid)
		{
			++Failures;
			continue;
		}
		FDWRuleCharacter C(uint32(Number(Root, TEXT("seed"), 5)));
		FString Offhand;
		Case->TryGetStringField(TEXT("offhand"), Offhand);
		C.Offhand = FName(*Offhand);
		if (!C.SetSheet(Entries, Error))
		{
			AddError(Id + TEXT(": ") + Error);
			++Failures;
			continue;
		}
		for (const auto &P : Case->GetObjectField(TEXT("stats"))->Values)
			if (FMath::Abs(C.Sheet.Stat(FName(*P.Key)) - P.Value->AsNumber()) > 1.e-8)
			{
				AddError(Id + TEXT(": sheet stat differs ") + P.Key);
				Valid = false;
				break;
			}
		C.Life *= .6;
		C.Shield *= .35;
		C.Mana *= .7;
		const auto &Expected = Case->GetArrayField(TEXT("expected"));
		for (int32 Step = 0; Valid && Step < Expected.Num(); ++Step)
		{
			if (Step > 0)
			{
				const auto O = Ops[Step - 1]->AsObject();
				const FString Op = O->GetStringField(TEXT("op"));
				if (Op == TEXT("tick"))
					C.Tick(Number(O, TEXT("dt")), Bool(O, TEXT("moving")));
				if (Op == TEXT("hit"))
				{
					FDWCharacterHit H;
					H.Amount = Number(O, TEXT("amount"));
					ParseElement(O->GetStringField(TEXT("element")), H.Element);
					H.bDot = Bool(O, TEXT("dot"));
					H.bCritical = Bool(O, TEXT("critical"));
					const auto K = O->GetObjectField(TEXT("kind"));
					H.Kind = {Bool(K, TEXT("attack")), Bool(K, TEXT("spell")), Bool(K, TEXT("melee")),
							  Bool(K, TEXT("projectile")), Bool(K, TEXT("area"))};
					C.TakeHit(H);
				}
				if (Op == TEXT("dealt"))
				{
					EElement E;
					ParseElement(O->GetStringField(TEXT("element")), E);
					C.OnDealt(Number(O, TEXT("amount")), E, Bool(O, TEXT("critical")), Bool(O, TEXT("killed")),
							  Bool(O, TEXT("dot")));
				}
				if (Op == TEXT("cast"))
					C.BeforeCast(Number(O, TEXT("cost")), Bool(O, TEXT("attack")), Number(O, TEXT("costScale"), 1));
				if (Op == TEXT("item"))
					C.UseItem({Library.Traits[FName(*O->GetStringField(TEXT("id")))].Source});
			}
			if (!Match(Expected[Step]->AsObject(), C, Error))
			{
				AddError(FString::Printf(TEXT("%s step %d: %s"), *Id, Step, *Error));
				Valid = false;
			}
			++Checked;
		}
		if (!Valid)
			++Failures;
		if (Failures >= 30)
		{
			AddError(TEXT("Stopped after 30 mismatched cases"));
			break;
		}
	}
	AddInfo(FString::Printf(TEXT("Character parity: %d cases, %d state comparisons, %d failed cases"), Cases.Num(),
							Checked, Failures));
	return Failures == 0;
}
#endif
