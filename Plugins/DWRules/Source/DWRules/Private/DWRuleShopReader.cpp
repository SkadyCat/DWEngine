#include "DWRuleShopReader.h"
#include "DWRuleJson.h"

using namespace DWRules;

bool FDWRuleShopReader::ReadText(const FString &Text, FDWShopCatalog &Out, FString &Error)
{
	using namespace DWRulesJson;
	FObject Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) ||
		!Fields(Root, {TEXT("currency"), TEXT("startingStones"), TEXT("starter"), TEXT("prices"), TEXT("hidden")},
				Error))
		return false;
	FDWShopCatalog Next;
	if (Root->HasField(TEXT("currency")))
	{
		const FObject *C = nullptr;
		if (!Root->TryGetObjectField(TEXT("currency"), C) || !Fields(*C, {TEXT("id"), TEXT("name")}, Error))
			return false;
		FString Currency;
		if (!String(*C, TEXT("id"), Currency, Error) || Currency != TEXT("spirit_stone"))
		{
			Error = TEXT("currency must be spirit_stone");
			return false;
		}
		if (!String(*C, TEXT("name"), Next.CurrencyName, Error))
			return false;
		if (Next.CurrencyName.IsEmpty())
			Next.CurrencyName = TEXT("灵石");
	}
	double Starting = double(Next.StartingStones);
	if (!Number(Root, TEXT("startingStones"), Starting, 0, 9.e15, Error) || FMath::FloorToDouble(Starting) != Starting)
	{
		Error = TEXT("startingStones must be a nonnegative integer");
		return false;
	}
	Next.StartingStones = int64(Starting);
	for (const TCHAR *Field : {TEXT("starter"), TEXT("hidden")})
		if (Root->HasField(Field))
		{
			const TArray<TSharedPtr<FJsonValue>> *List = nullptr;
			if (!Root->TryGetArrayField(Field, List))
			{
				Error = TEXT("shop keys must be arrays");
				return false;
			}
			auto &Target = FString(Field) == TEXT("starter") ? Next.Starter : Next.Hidden;
			Target.Reset();
			for (const auto &Value : *List)
			{
				FString Key;
				if (!Value->TryGetString(Key) || !FDWShopRules::ValidKey(Key))
				{
					Error = TEXT("invalid shop key");
					return false;
				}
				Target.AddUnique(Key);
			}
		}
	if (Root->HasField(TEXT("prices")))
	{
		const FObject *Prices = nullptr;
		if (!Root->TryGetObjectField(TEXT("prices"), Prices))
		{
			Error = TEXT("shop prices must be an object");
			return false;
		}
		for (const auto &Pair : (*Prices)->Values)
		{
			const FString Key(Pair.Key);
			double Value;
			if (!FDWShopRules::ValidKey(Key) || !Pair.Value->TryGetNumber(Value) || !FMath::IsFinite(Value) ||
				Value < 0 || Value > 9.e15 || FMath::FloorToDouble(Value) != Value)
			{
				Error = TEXT("invalid shop price ") + Key;
				return false;
			}
			Next.Prices.Add(Key, int64(Value));
		}
	}
	Out = MoveTemp(Next);
	return true;
}
