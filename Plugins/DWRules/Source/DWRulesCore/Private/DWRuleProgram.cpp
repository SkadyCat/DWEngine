#include "DWRuleProgram.h"

namespace DWRules
{
bool FDWSheetValue::Evaluate(const TMap<FName, double> &Params, double &Out) const
{
	if (Kind == EKind::Number)
	{
		Out = Number;
		return FMath::IsFinite(Out);
	}
	if (Kind == EKind::Param)
	{
		const double *Value = Params.Find(Param);
		if (!Value)
			return false;
		Out = *Value;
		return FMath::IsFinite(Out);
	}
	if (Args.Num() < 2 || !Args[0].Evaluate(Params, Out))
		return false;
	for (int32 I = 1; I < Args.Num(); ++I)
	{
		double Value;
		if (!Args[I].Evaluate(Params, Value))
			return false;
		switch (Kind)
		{
		case EKind::Add:
			Out += Value;
			break;
		case EKind::Sub:
			Out -= Value;
			break;
		case EKind::Mul:
			Out *= Value;
			break;
		case EKind::Div:
			Out = Value == 0.0 ? 0.0 : Out / Value;
			break;
		default:
			return false;
		}
	}
	return FMath::IsFinite(Out);
}

FString TraitSourceKey(const FDWTraitSource &Source)
{
	FString Text = Source.Id.ToString();
	TArray<FName> Keys;
	Source.Params.GetKeys(Keys);
	Keys.Sort(FNameLexicalLess());
	for (FName Key : Keys)
		Text += FString::Printf(TEXT("|%s=%.9g"), *Key.ToString(), Source.Params[Key]);
	if (!Source.Stitched.IsEmpty())
	{
		Text += TEXT("|with(");
		for (int32 I = 0; I < Source.Stitched.Num(); ++I)
		{
			if (I)
				Text += TEXT(",");
			Text += TraitSourceKey(Source.Stitched[I]);
		}
		Text += TEXT(")");
	}
	return Text;
}
} // namespace DWRules
