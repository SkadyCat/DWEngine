#pragma once
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace DWRulesJson
{
using FObject = TSharedPtr<FJsonObject>;
inline bool Fields(const FObject &O, const TSet<FString> &Allowed, FString &Error)
{
	if (!O)
	{
		Error = TEXT("expected JSON object");
		return false;
	}
	for (const auto &P : O->Values)
		if (!Allowed.Contains(FString(P.Key)))
		{
			Error = TEXT("unknown field ") + FString(P.Key);
			return false;
		}
	return true;
}
inline bool Id(const FString &Value)
{
	if (Value.IsEmpty())
		return false;
	for (TCHAR C : Value)
		if (!(C >= 'a' && C <= 'z') && !(C >= '0' && C <= '9') && C != '_')
			return false;
	return true;
}
inline bool String(const FObject &O, const TCHAR *Key, FString &Out, FString &Error)
{
	if (!O->HasField(Key))
		return true;
	if (O->TryGetStringField(Key, Out))
		return true;
	Error = FString(Key) + TEXT(" must be text");
	return false;
}
inline bool Bool(const FObject &O, const TCHAR *Key, bool &Out, FString &Error)
{
	if (!O->HasField(Key))
		return true;
	if (O->TryGetBoolField(Key, Out))
		return true;
	Error = FString(Key) + TEXT(" must be boolean");
	return false;
}
inline bool Number(const FObject &O, const TCHAR *Key, double &Out, double Min, double Max, FString &Error)
{
	if (!O->HasField(Key))
		return true;
	if (O->TryGetNumberField(Key, Out) && FMath::IsFinite(Out) && Out >= Min && Out <= Max)
		return true;
	Error = FString::Printf(TEXT("%s must be finite and within %g to %g"), Key, Min, Max);
	return false;
}
} // namespace DWRulesJson
