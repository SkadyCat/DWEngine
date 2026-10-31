#pragma once
#include "DWRuleCollection.h"

namespace DWRules
{
struct FDWRuleLibrary;
enum class EPawnKind : uint8
{
	Player,
	Monster,
	Summon
};
class DWRULESCORE_API FDWPawnEquipmentRules
{
  public:
	static bool IsSlot(FName Slot, EPawnKind Kind);
	static bool IsFixedSlot(FName Slot);
	static FDWPawnEquipment Defaults(EPawnKind Kind);
	static bool Resolve(const FDWRuleLibrary &Library, const FDWPawnEquipment &Equipment, EPawnKind Kind,
						TArray<FDWCollectionPart> &Out, FString &Error);
	static bool Aggregate(const FDWRuleLibrary &Library, const FDWPawnEquipment &Equipment, EPawnKind Kind,
						  TArray<FDWTraitSource> &Out, FString &Error);
	static bool SetTower(const FDWRuleLibrary &Library, FDWPawnEquipment &Equipment, int32 Floor,
						 FString &Error);
	static FName Rarity(const FDWPawnEquipment &Equipment);
};
} // namespace DWRules
