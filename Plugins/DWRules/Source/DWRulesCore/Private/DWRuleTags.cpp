#include "DWRuleTags.h"
#include "DWRuleLibrary.h"
#include "Internationalization/Regex.h"

namespace DWRules
{
namespace
{
bool TagMatches(const FString &Text, const TCHAR *Pattern)
{
	FRegexMatcher M(FRegexPattern(Pattern), Text);
	return M.FindNext();
}
FName ElementTag(FName Name)
{
	if (Name == TEXT("cold"))
		return TEXT("frost");
	static const TSet<FName> Elements{TEXT("fire"),		TEXT("frost"),	TEXT("lightning"),
									  TEXT("physical"), TEXT("poison"), TEXT("void")};
	return Elements.Contains(Name) ? Name : NAME_None;
}
void SortTags(TArray<FName> &Tags)
{
	Tags.Sort([](FName A, FName B) { return A.ToString() < B.ToString(); });
}
void SheetTags(const FString &Path, TSet<FName> &Tags)
{
	TArray<FString> Parts;
	Path.ParseIntoArray(Parts, TEXT("."));
	if (!Parts.Num())
		return;
	const FString Head = Parts[0], Second = Parts.Num() > 1 ? Parts[1] : FString(),
				  Third = Parts.Num() > 2 ? Parts[2] : FString();
	const FName Element = ElementTag(FName(*Parts.Last()));
	if (TagMatches(Path, TEXT("^(shield|threshold)|shield")))
		Tags.Add(TEXT("shield"));
	if (TagMatches(Path, TEXT("^(life|heal|flask)|life")))
		Tags.Add(TEXT("life"));
	if (Head == TEXT("leech"))
	{
		Tags.Add(TEXT("leech"));
		Tags.Add(Second == TEXT("shield") ? TEXT("shield") : TEXT("life"));
	}
	if (Head == TEXT("recoup"))
	{
		Tags.Add(TEXT("recovery"));
		Tags.Add(Second == TEXT("shield") ? TEXT("shield") : TEXT("life"));
	}
	if (TagMatches(Path, TEXT("regen|on_|recharge|recover|flask|heal")))
		Tags.Add(TEXT("recovery"));
	if (Head == TEXT("mana"))
		Tags.Add(TEXT("mana"));
	if (Head == TEXT("attr"))
		Tags.Add(TEXT("attribute"));
	if (Head == TEXT("defense"))
		Tags.Add(TEXT("defense"));
	if (TagMatches(Path, TEXT("damage_taken|taken_|bypass|threshold")))
		Tags.Add(TEXT("defense"));
	if (Head == TEXT("cost") || Path.Contains(TEXT("cost")))
		Tags.Add(TEXT("cost"));
	if (Head == TEXT("damage") || TagMatches(Path, TEXT("damage$|\\.damage\\b")))
		Tags.Add(TEXT("damage"));
	if (Head == TEXT("damage") && Third == TEXT("elemental"))
		for (const TCHAR *E : {TEXT("fire"), TEXT("frost"), TEXT("lightning"), TEXT("void")})
			Tags.Add(E);
	if (Head == TEXT("damage") && !Element.IsNone())
		Tags.Add(Element);
	if (Head == TEXT("area") || Path.Contains(TEXT("area")))
		Tags.Add(TEXT("area"));
	if (TagMatches(Path, TEXT("critical|crit")))
		Tags.Add(TEXT("critical"));
	if (Path.Contains(TEXT("kill")))
		Tags.Add(TEXT("kill"));
	if (Path.Contains(TEXT("speed")) && !TagMatches(Path, TEXT("regen|recharge|leech|flask|recoup")))
		Tags.Add(TEXT("speed"));
}
} // namespace
TArray<FName> FDWTagIndex::DeriveTraitTags(const FDWTraitSource &Trait)
{
	TSet<FName> Tags;
	for (const auto &M : Trait.Modifiers)
	{
		TArray<FString> P;
		M.Stat.ToLower().ParseIntoArray(P, TEXT("."));
		if (P.Num() < 2)
			continue;
		if (P[0] == TEXT("element"))
		{
			const FName E = ElementTag(FName(*P[1]));
			if (!E.IsNone())
				Tags.Add(E);
			if (P.Num() > 2 && TagMatches(P[2], TEXT("resistance|penetration")))
				Tags.Add(TEXT("defense"));
			else
				Tags.Add(TEXT("damage"));
		}
		if (P[0] == TEXT("projectile"))
		{
			Tags.Add(TEXT("projectile"));
			if (TagMatches(P[1], TEXT("ricochet|reflection|return|refraction|pierce")))
				Tags.Add(TEXT("chain"));
			if (P[1] == TEXT("damage"))
				Tags.Add(TEXT("damage"));
		}
		if (P[0] == TEXT("area") || P[0] == TEXT("explosion"))
			Tags.Add(TEXT("area"));
		if (P[1].Contains(TEXT("critical")))
			Tags.Add(TEXT("critical"));
		if (P[1].Contains(TEXT("speed")) && P[0] == TEXT("cast"))
			Tags.Add(TEXT("speed"));
	}
	for (const auto &Rule : Trait.Rules)
	{
		if (Rule.Event == TEXT("critical"))
			Tags.Add(TEXT("critical"));
		if (Rule.Event == TEXT("kill"))
			Tags.Add(TEXT("kill"));
		for (const auto &C : Rule.Conditions)
		{
			const FName E = ElementTag(C.Tag);
			if (!E.IsNone())
				Tags.Add(E);
			if (C.Tag == TEXT("projectile"))
				Tags.Add(TEXT("projectile"));
		}
	}
	TFunction<void(const TArray<FDWActionSource> &)> Walk = [&](const auto &Actions) {
		for (const auto &A : Actions)
		{
			for (FName Tag : A.Tags)
			{
				const FName E = ElementTag(Tag);
				if (!E.IsNone())
					Tags.Add(E);
				else if (Tag == TEXT("melee") || Tag == TEXT("spell"))
					Tags.Add(Tag);
				else if (Tag == TEXT("shatter"))
					Tags.Add(TEXT("ailment"));
			}
			if (A.Element >= 0 && A.Element < NumElements)
				Tags.Add(FName(ElementName(EElement(A.Element))));
			if (A.Op == EOp::DamageArea)
			{
				Tags.Add(TEXT("area"));
				Tags.Add(TEXT("damage"));
			}
			if (A.Op == EOp::DamageTarget)
				Tags.Add(TEXT("damage"));
			if (A.Op == EOp::Emit || A.Op == EOp::Copy)
				Tags.Add(TEXT("projectile"));
			if (A.Op == EOp::Redirect)
			{
				Tags.Add(TEXT("projectile"));
				Tags.Add(TEXT("chain"));
			}
			if (A.Op == EOp::ApplyStatus)
				Tags.Add(TEXT("ailment"));
			if (A.Op == EOp::GrantTraits)
			{
				Tags.Add(TEXT("duration"));
				Tags.Add(TEXT("trigger"));
			}
			Walk(A.Then);
		}
	};
	for (const auto &Rule : Trait.Rules)
		Walk(Rule.Actions);
	for (const auto &E : Trait.Sheet)
	{
		if (!E.Stat.IsNone())
			SheetTags(E.Stat.ToString().ToLower(), Tags);
		if (!E.Trigger.IsNone())
		{
			Tags.Add(TEXT("trigger"));
			const FString Path = E.Trigger.ToString().ToLower();
			SheetTags(Path, Tags);
			if (Path.Contains(TEXT("empower")))
				Tags.Add(TEXT("damage"));
			if (TagMatches(Path, TEXT("guard|cheat")))
				Tags.Add(TEXT("defense"));
			if (TagMatches(Path, TEXT("buff|guard")))
				Tags.Add(TEXT("duration"));
		}
	}
	auto Out = Tags.Array();
	SortTags(Out);
	return Out;
}
bool FDWTagIndex::Build(const FDWRuleLibrary &Library, const FDWTagDocument &Doc, FString &Error)
{
	FDWTagIndex Next;
	Next.Document = Doc;
	TSet<FName> FamilyIds;
	for (const auto &F : Doc.Families)
	{
		if (F.Id.IsNone() || FamilyIds.Contains(F.Id))
		{
			Error = TEXT("invalid or duplicate family");
			return false;
		}
		FamilyIds.Add(F.Id);
	}
	for (const auto &Pair : Doc.Tags)
	{
		const auto &T = Pair.Value;
		if (FamilyIds.Contains(T.Id) || !FamilyIds.Contains(T.Family))
		{
			Error = TEXT("invalid tag family ") + T.Id.ToString();
			return false;
		}
		Next.ByName.Add(T.Id, T);
		Next.ByName.Add(FName(*T.Name), T);
	}
	for (const auto &F : Doc.Families)
	{
		FDWTagDefinition Tag{F.Id, F.Id, F.Name + TEXT("族")};
		Next.ByName.Add(F.Id, Tag);
		Next.ByName.Add(FName(*Tag.Name), Tag);
	}
	for (const auto &Pair : Library.Traits)
	{
		TSet<FName> Tags;
		for (FName T : DeriveTraitTags(Pair.Value.Source))
			Tags.Add(T);
		if (const auto *Extra = Doc.TraitTags.Find(Pair.Key))
			for (FName T : *Extra)
				Tags.Add(T);
		auto Sorted = Tags.Array();
		SortTags(Sorted);
		TSet<FName> Families;
		for (FName Tag : Sorted)
		{
			if (const auto *T = Next.ByName.Find(Tag))
				Families.Add(T->Family);
			else
				Next.Unmapped.FindOrAdd(Tag).Add(Pair.Key);
		}
		auto F = Families.Array();
		SortTags(F);
		Next.TraitTags.Add(Pair.Key, MoveTemp(Sorted));
		Next.TraitFamilies.Add(Pair.Key, MoveTemp(F));
	}
	if (Next.Unmapped.Num())
	{
		const auto It = Next.Unmapped.CreateConstIterator();
		Error = TEXT("tag has no family: ") + It.Key().ToString();
		return false;
	}
	*this = MoveTemp(Next);
	return true;
}
const FDWTagDefinition *FDWTagIndex::Tag(const FString &Name) const
{
	return ByName.Find(FName(*Name));
}
TArray<FName> FDWTagIndex::FamiliesOf(FName Trait) const
{
	const auto *F = TraitFamilies.Find(Trait);
	return F ? *F : TArray<FName>();
}
TArray<FName> FDWTagIndex::CollectionFamilies(const TArray<FString> &Tags, TArray<FString> *Unknown) const
{
	TArray<FName> Out;
	for (const auto &Name : Tags)
	{
		if (const auto *T = Tag(Name))
			Out.AddUnique(T->Family);
		else if (Unknown)
			Unknown->Add(Name);
	}
	return Out;
}
FString FDWTagIndex::FamilyName(FName Id) const
{
	for (const auto &F : Document.Families)
		if (F.Id == Id)
			return F.Name.IsEmpty() ? Id.ToString() : F.Name + TEXT("族");
	return Id.ToString();
}
double FDWTagIndex::RollValue(FName Name, double Base, double Random)
{
	if (!FMath::IsFinite(Base) || Base == 0)
		return Base;
	double V = Base * (.5 + Random * 1.5);
	if (TagMatches(Name.ToString().ToLower(), TEXT("chance|percent|slow")))
		V = FMath::Min(100., V);
	if (FMath::FloorToDouble(Base) == Base)
	{
		V = FMath::FloorToDouble(V + .5);
		return V != 0 ? V : FMath::Sign(Base);
	}
	V = FMath::FloorToDouble(V * 100 + .5) / 100;
	return V != 0 ? V : FMath::Sign(Base) * .01;
}
} // namespace DWRules
