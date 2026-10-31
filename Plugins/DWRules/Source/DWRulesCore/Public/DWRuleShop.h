#pragma once
#include "CoreMinimal.h"

namespace DWRules
{
struct FDWRuleLibrary;
struct FDWShopCatalog
{
	FString CurrencyName = TEXT("灵石");
	int64 StartingStones = 300;
	TArray<FString> Starter{TEXT("skill:frost_lance")}, Hidden;
	TMap<FString, int64> Prices;
};
struct FDWShopListing
{
	FString Key;
	FName Kind, Id, Rarity;
	FString Name, Description;
	int64 Price = 0;
	bool bCustom = false;
};
class DWRULESCORE_API FDWShopRules
{
  public:
	static TArray<FDWShopListing> Listings(const FDWRuleLibrary &Library);
	static int64 DefaultPrice(FName Kind, FName Rarity, FName Category, int32 Level, int32 Traits);
	static bool ValidKey(const FString &Key);
};
} // namespace DWRules
