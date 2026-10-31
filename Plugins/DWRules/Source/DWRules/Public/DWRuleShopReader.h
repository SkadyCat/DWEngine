#pragma once
#include "DWRuleShop.h"
class DWRULES_API FDWRuleShopReader
{
  public:
	static bool ReadText(const FString &Text, DWRules::FDWShopCatalog &Out, FString &Error);
};
