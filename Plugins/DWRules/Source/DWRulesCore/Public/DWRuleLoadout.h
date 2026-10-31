#pragma once
#include "DWRuleEquipment.h"
#include "DWRuleLibrary.h"

namespace DWRules
{
constexpr int32 ActiveSkillSlotCount = 4;
constexpr int32 SkillGemSlotCount = 6;
struct FDWSkillGems
{
	FName Skill;
	TArray<FName> Slots;
};
struct FDWAvailableSkill
{
	FName Id;
	FString Name;
	bool bEquipmentGranted = false;
};
// Player-owned choices and socket reservations. Definitions and compiled plans stay immutable.
class DWRULESCORE_API FDWRuleLoadout
{
  public:
	FDWRuleLoadout();
	// Host supplied non-equipment collections (talents, etc.). Never serialized as gear.
	TArray<FDWCollectionPart> PassiveParts;
    // Ephemeral host collections (enabled auras, etc.) remain independent of talents and gear.
    TMap<FName,TArray<FDWCollectionPart>> RuntimeGroups;
	TArray<FName> Skills;
	TMap<FName, FName> Equipment;
	TMap<FName, int32> Owned;
	TArray<FDWEquipmentInstance> Instances;
	TArray<FDWSkillGems> Gems;
	TArray<FDWAvailableSkill> AvailableSkills(const FDWRuleLibrary &Library) const;
	void Validate(const FDWRuleLibrary &Library);
	bool ChangeSkill(const FDWRuleLibrary &Library, int32 Slot, FName Value, FString &Error);
	bool ChangeGem(const FDWRuleLibrary &Library, FName Skill, int32 Slot, FName Value, FString &Error);
	int32 SocketedCount(FName Gem, FName ExceptSkill = NAME_None, int32 ExceptSlot = INDEX_NONE) const;
	int32 AvailableGems(FName Gem, FName ExceptSkill = NAME_None, int32 ExceptSlot = INDEX_NONE) const;
	bool ResolveEquipment(const FDWRuleLibrary &Library, TArray<FDWCollectionPart> &Out,
						  FString &Error) const;
	TArray<FDWCollectionPart> EquipmentParts(const FDWRuleLibrary &Library) const;
	TArray<FDWCollectionPart> GemParts(const FDWRuleLibrary &Library, FName Skill) const;
	bool Compose(const FDWRuleLibrary &Library, int32 Slot, FDWBuildSource &Source,
				 TArray<FDWContribution> &Contributions, FString &Error) const;
};
} // namespace DWRules
