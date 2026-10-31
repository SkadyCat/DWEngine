#include "DWRuleShop.h"
#include "DWRuleLibrary.h"
#include "DWRulePawnEquipment.h"

namespace DWRules
{
bool FDWShopRules::ValidKey(const FString &Key)
{
	FString Kind, Id;
	if (!Key.Split(TEXT(":"), &Kind, &Id) ||
		(Kind != TEXT("skill") && Kind != TEXT("equipment") && Kind != TEXT("item")) || Id.IsEmpty())
		return false;
	for (TCHAR C : Id)
		if (!(C >= 'a' && C <= 'z') && !(C >= '0' && C <= '9') && C != '_')
			return false;
	return true;
}
int64 FDWShopRules::DefaultPrice(FName Kind, FName Rarity, FName Category, int32 Level, int32 Traits)
{
	if (Kind == TEXT("skill"))
		return 100 + 80 * FMath::Max(0, Traits - 1);
	static const TMap<FName, double> Rarities{{TEXT("common"), 40},
											  {TEXT("magic"), 120},
											  {TEXT("rare"), 320},
											  {TEXT("legendary"), 900},
											  {TEXT("unique"), 1500}};
	const auto *BaseValue = Rarities.Find(Rarity);
	const double Base = BaseValue ? *BaseValue : 100;
	if (Kind == TEXT("equipment"))
		return int64(FMath::FloorToDouble((Base * (1 + (Level ? Level : 1) / 20.) + 25 * Traits) / 5 + .5) * 5);
	static const TMap<FName, double> Factors{{TEXT("consumable"), .25}, {TEXT("rune"), 1},	{TEXT("gem"), 1.2},
											 {TEXT("material"), .15},	{TEXT("quest"), 0}, {TEXT("currency"), 0}};
	const auto *Factor = Factors.Find(Category);
	return FMath::Max<int64>(5,
							 int64(FMath::FloorToDouble((Base * (Factor ? *Factor : .5) + 10 * Traits) / 5 + .5) * 5));
}
TArray<FDWShopListing> FDWShopRules::Listings(const FDWRuleLibrary &Library)
{
	TArray<FDWShopListing> Out;
	auto Add = [&](FName Kind, FName Id, FName Rarity, FName Category, const FString &Name, const FString &Description,
				   int32 Level, int32 Traits) {
		const FString Key = Kind.ToString() + TEXT(":") + Id.ToString();
		if (Library.Shop.Hidden.Contains(Key))
			return;
		const auto *Custom = Library.Shop.Prices.Find(Key);
		Out.Add({Key, Kind, Id, Rarity, Name.IsEmpty() ? Id.ToString() : Name, Description,
				 Custom ? *Custom : DefaultPrice(Kind, Rarity, Category, Level, Traits), Custom != nullptr});
	};
	for (const auto &Pair : Library.Builds)
		if (!Pair.Key.ToString().StartsWith(TEXT("monster_")))
			Add(TEXT("skill"), Pair.Key, TEXT("skill"), NAME_None, Pair.Value.DisplayName, Pair.Value.Description, 1,
				Pair.Value.Traits.Num());
	for (const auto &Pair : Library.Equipment)
	{
		const auto &D = Pair.Value;
		if (FDWPawnEquipmentRules::IsFixedSlot(D.Slot))
			continue;
		Add(TEXT("equipment"), D.Id, D.Rarity, D.Category, D.Name, D.Description, D.Level, D.Traits.Num());
	}
	for (const auto &Pair : Library.Items)
	{
		const auto &D = Pair.Value;
		Add(TEXT("item"), D.Id, D.Rarity, D.Category, D.Name, D.Description, D.Level, D.Traits.Num());
	}
	return Out;
}
} // namespace DWRules
