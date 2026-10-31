#include "DWRuleMonsterReader.h"
#include "DWRuleJson.h"
#include "DWRuleProgramReader.h"
#include "DWRulePawnEquipment.h"

using namespace DWRules;

bool FDWRuleMonsterReader::ReadText(const FString &Text, FDWMonsterDefinition &Out, FString &Error)
{
	using namespace DWRulesJson;
	FObject Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) ||
		!Fields(Root,
				{TEXT("id"), TEXT("name"), TEXT("description"), TEXT("enabled"), TEXT("weight"),
				 TEXT("health"), TEXT("speed"), TEXT("stopDistance"), TEXT("color"), TEXT("traits"),
				 TEXT("skills"), TEXT("equipment"), TEXT("minFloor"),TEXT("scale"),TEXT("followDistance")},
				Error))
		return false;
	FDWMonsterDefinition Next;
	Next.Equipment = FDWPawnEquipmentRules::Defaults(EPawnKind::Monster);
	if (Root->HasField(TEXT("minFloor")))
	{
		double V;
		if (!Root->TryGetNumberField(TEXT("minFloor"), V) || !FMath::IsFinite(V) || V < 1 || V > MAX_int32 ||
			V != FMath::FloorToDouble(V))
		{
			Error = TEXT("invalid minFloor");
			return false;
		}
		Next.MinFloor = int32(V);
	}
	if (Root->HasField(TEXT("equipment")))
	{
		const FObject *Equipment;
		if (!Root->TryGetObjectField(TEXT("equipment"), Equipment))
		{
			Error = TEXT("equipment must be an object");
			return false;
		}
		for (const auto &P : (*Equipment)->Values)
		{
			const FName Slot(*P.Key);
			if (!FDWPawnEquipmentRules::IsSlot(Slot, EPawnKind::Monster))
			{
				Error = TEXT("invalid monster slot");
				return false;
			}
			if (P.Value->Type == EJson::Null)
				Next.Equipment.Ids.Add(Slot, NAME_None);
			else if (P.Value->Type == EJson::String && Id(P.Value->AsString()))
				Next.Equipment.Ids.Add(Slot, FName(*P.Value->AsString()));
			else
			{
				Error = TEXT("equipment must contain ids or null");
				return false;
			}
		}
	}
	FString IdText;
	if (!String(Root, TEXT("id"), IdText, Error) || !Id(IdText))
	{
		Error = TEXT("monster requires a lowercase id");
		return false;
	}
	Next.Id = FName(*IdText);
	if (!String(Root, TEXT("name"), Next.Name, Error) || Next.Name.TrimStartAndEnd().IsEmpty())
	{
		Error = TEXT("monster requires a name");
		return false;
	}
	if (!String(Root, TEXT("description"), Next.Description, Error) ||
		!String(Root, TEXT("color"), Next.Color, Error) || !Bool(Root, TEXT("enabled"), Next.bEnabled, Error))
		return false;
	if (Next.Color.Len() != 7 || Next.Color[0] != '#')
	{
		Error = TEXT("monster color must be #RRGGBB");
		return false;
	}
	for (int32 I = 1; I < 7; ++I)
		if (!FChar::IsHexDigit(Next.Color[I]))
		{
			Error = TEXT("invalid monster color");
			return false;
		}
	if (!Number(Root, TEXT("weight"), Next.Weight, 0, 10000, Error) ||
		!Number(Root, TEXT("health"), Next.Health, 1, 1.e6, Error) ||
		!Number(Root, TEXT("speed"), Next.SpeedCm, 0, 3000, Error) ||
		!Number(Root, TEXT("stopDistance"), Next.StopDistanceCm, 0, 5000, Error)||
        !Number(Root,TEXT("scale"),Next.VisualScale,.05,10,Error)||
        !Number(Root,TEXT("followDistance"),Next.FollowDistanceCm,0,5000,Error))
		return false;
	if (Root->HasField(TEXT("traits")))
	{
		const TArray<TSharedPtr<FJsonValue>> *Traits = nullptr;
		if (!Root->TryGetArrayField(TEXT("traits"), Traits))
		{
			Error = TEXT("monster traits must be an array");
			return false;
		}
		TSet<FName> Seen;
		for (const auto &Value : *Traits)
		{
			if (Value->Type == EJson::Object &&
				!Fields(Value->AsObject(), {TEXT("id"), TEXT("params"), TEXT("tier")}, Error))
				return false;
			FDWTraitEntry E;
			if (!FDWRuleProgramReader::ReadTraitEntry(Value, E, Error))
				return false;
			if (!Id(E.Id.ToString()) || Seen.Contains(E.Id))
			{
				Error = TEXT("invalid or duplicate monster trait ") + E.Id.ToString();
				return false;
			}
			Seen.Add(E.Id);
			Next.Traits.Add(MoveTemp(E));
		}
	}
	if (Root->HasField(TEXT("skills")))
	{
		const TArray<TSharedPtr<FJsonValue>> *Skills = nullptr;
		if (!Root->TryGetArrayField(TEXT("skills"), Skills) || Skills->Num() > 8)
		{
			Error = TEXT("monster supports at most eight skills");
			return false;
		}
		TSet<FName> Seen;
		for (const auto &Value : *Skills)
		{
			const auto O = Value->AsObject();
			if (!Fields(O, {TEXT("id"), TEXT("enabled"), TEXT("range"), TEXT("cooldown"), TEXT("power")}, Error))
				return false;
			FDWMonsterSkill Skill;
			FString Name = Skill.Id.ToString();
			if (!String(O, TEXT("id"), Name, Error) || !Id(Name) || Seen.Contains(FName(*Name)))
			{
				Error = TEXT("invalid or duplicate monster skill ") + Name;
				return false;
			}
			Skill.Id = FName(*Name);
			Seen.Add(Skill.Id);
			if (!Bool(O, TEXT("enabled"), Skill.bEnabled, Error) ||
				!Number(O, TEXT("range"), Skill.RangeCm, 1, 5000, Error) ||
				!Number(O, TEXT("cooldown"), Skill.Cooldown, .1, 120, Error) ||
				!Number(O, TEXT("power"), Skill.Power, 0, 1.e5, Error))
				return false;
			Next.Skills.Add(Skill);
		}
	}
	Out = MoveTemp(Next);
	return true;
}
