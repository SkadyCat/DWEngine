#include "DWRuleStateReader.h"
#include "DWRuleJson.h"
#include "DWRuleProgramReader.h"

using namespace DWRules;
namespace
{
using FStateObject = TSharedPtr<FJsonObject>;
using FStateValue = TSharedPtr<FJsonValue>;
FStateObject StateObject(const FStateObject &Root, const TCHAR *Key)
{
	const FStateObject *Value = nullptr;
	return Root && Root->TryGetObjectField(Key, Value) ? *Value : MakeShared<FJsonObject>();
}
bool StateInteger(const FStateObject &O, const TCHAR *Key, int64 &Out, int64 Min, int64 Max, FString &Error)
{
	if (!O->HasField(Key))
		return true;
	double Value;
	if (!O->TryGetNumberField(Key, Value) || !FMath::IsFinite(Value) || Value < double(Min) || Value > double(Max) ||
		Value != FMath::FloorToDouble(Value))
	{
		Error = FString(Key) + TEXT(" must be an integer within limits");
		return false;
	}
	Out = int64(Value);
	return true;
}
bool StateEntries(const FStateObject &O, TArray<FDWTraitEntry> &Out, const FDWRuleLibrary &Library, FString &Error)
{
	const TArray<FStateValue> *Entries;
	if (!O->TryGetArrayField(TEXT("traits"), Entries))
	{
		Error = TEXT("affix needs traits array");
		return false;
	}
	for (const auto &Value : *Entries)
	{
		FDWTraitEntry E;
		FDWTraitSource S;
		if (!FDWRuleProgramReader::ReadTraitEntry(Value, E, Error) || !Library.SourceFromEntry(E, S, Error))
			return false;
		Out.Add(E);
	}
	return true;
}
bool StateAffix(const FStateObject &O, FDWEquipmentAffix &Out, const FDWRuleLibrary &Library, FString &Error)
{
	if (!DWRulesJson::Fields(O, {TEXT("id"), TEXT("name"), TEXT("traits"), TEXT("tier")}, Error))
		return false;
	FString Id, Name;
	if (!DWRulesJson::String(O, TEXT("id"), Id, Error) || Id.IsEmpty() ||
		!DWRulesJson::String(O, TEXT("name"), Name, Error))
		return false;
	Out.Id = FName(*Id);
	Out.Name = Name;
	int64 Tier = 0;
	if (!StateInteger(O, TEXT("tier"), Tier, 1, 10, Error))
		return false;
	Out.Tier = int32(Tier);
	return StateEntries(O, Out.Traits, Library, Error);
}
bool StateNames(const FStateObject &O, const TCHAR *Key, int32 Count, TArray<FName> &Out, FString &Error)
{
	Out.SetNum(Count);
	if (!O->HasField(Key))
		return true;
	const TArray<FStateValue> *Values;
	if (!O->TryGetArrayField(Key, Values) || Values->Num() > Count)
	{
		Error = FString(Key) + TEXT(" has too many slots");
		return false;
	}
	for (int32 I = 0; I < Values->Num(); ++I)
	{
		const auto &V = (*Values)[I];
		if (V->Type == EJson::Null)
			continue;
		if (V->Type != EJson::String)
		{
			Error = TEXT("slot must contain id or null");
			return false;
		}
		Out[I] = FName(*V->AsString());
	}
	return true;
}
FString StateName(FName Name)
{
	return Name.IsNone() ? FString() : Name.ToString().ToLower();
}
FStateValue StateEntry(const FDWTraitEntry &E)
{
	auto O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("id"), StateName(E.Id));
	auto Params = MakeShared<FJsonObject>();
	for (const auto &P : E.Params)
		Params->SetNumberField(StateName(P.Key), P.Value);
	if (E.Params.Num())
		O->SetObjectField(TEXT("params"), Params);
	if (E.Tier)
		O->SetNumberField(TEXT("tier"), E.Tier);
	if (E.Duration > 0)
		O->SetNumberField(TEXT("duration"), E.Duration);
	TArray<FStateValue> Children;
	for (const auto &C : E.With)
		Children.Add(StateEntry(C));
	if (Children.Num())
		O->SetArrayField(TEXT("with"), Children);
	return MakeShared<FJsonValueObject>(O);
}
FStateValue StateAffixJson(const FDWEquipmentAffix &A)
{
	auto O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("id"), StateName(A.Id));
	O->SetStringField(TEXT("name"), A.Name);
	if (A.Tier)
		O->SetNumberField(TEXT("tier"), A.Tier);
	TArray<FStateValue> Traits;
	for (const auto &E : A.Traits)
		Traits.Add(StateEntry(E));
	O->SetArrayField(TEXT("traits"), Traits);
	return MakeShared<FJsonValueObject>(O);
}
TArray<FStateValue> StateNamesJson(const TArray<FName> &Names)
{
	TArray<FStateValue> Out;
	for (FName N : Names)
	{
		if (N.IsNone())
			Out.Add(MakeShared<FJsonValueNull>());
		else
			Out.Add(MakeShared<FJsonValueString>(StateName(N)));
	}
	return Out;
}
} // namespace
bool FDWRuleStateReader::ReadEntries(const FString &Json, const FDWRuleLibrary &Library, TArray<FDWTraitEntry> &Out,
									 FString &Error)
{
	FStateValue Value;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Value) || !Value ||
		Value->Type != EJson::Array)
	{
		Error = TEXT("traits must be a JSON array");
		return false;
	}
	auto O = MakeShared<FJsonObject>();
	O->SetArrayField(TEXT("traits"), Value->AsArray());
	TArray<FDWTraitEntry> Next;
	if (!StateEntries(O, Next, Library, Error))
		return false;
	Out = MoveTemp(Next);
	return true;
}
bool FDWRuleStateReader::Read(const FString &Json, const FDWRuleLibrary &Library, FDWRuleProfile &Out, FString &Error)
{
	FStateObject Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root)
	{
		Error = TEXT("invalid player JSON");
		return false;
	}
	FDWRuleProfile Next;
 // Optional UI-only placement. Invalid/stale cells heal against ownership in the HUD adapter.
 const auto SavedBag=StateObject(Root,TEXT("bagLayout"));
 double BagKey=1; if(SavedBag->TryGetNumberField(TEXT("nextKey"),BagKey)&&BagKey>=1&&BagKey<=MAX_int32&&FMath::FloorToDouble(BagKey)==BagKey) Next.BagLayout.NextKey=int32(BagKey);
 const TArray<FStateValue>* BagCells=nullptr;
 if(SavedBag->TryGetArrayField(TEXT("slots"),BagCells)) for(const auto& Cell:*BagCells)
 {
  FDWInventoryStackPlacement Placement;
  if(Cell.IsValid()&&Cell->Type==EJson::Object)
  {
   const auto Saved=Cell->AsObject();double Count=0;
   Saved->TryGetStringField(TEXT("key"),Placement.Key);Saved->TryGetStringField(TEXT("ref"),Placement.Ref);
   if(Saved->TryGetNumberField(TEXT("count"),Count)&&Count>=1&&Count<=MAX_int32&&FMath::FloorToDouble(Count)==Count) Placement.Count=int32(Count);
  }
  Next.BagLayout.Slots.Add(MoveTemp(Placement));
 }
	if (!StateInteger(Root, TEXT("stones"), Next.Stones, 0, 9007199254740991ll, Error))
		return false;
	if (!DWRulesJson::Bool(Root, TEXT("initialized"), Next.bInitialized, Error))
		return false;
	const auto Owned = StateObject(Root, TEXT("owned"));
	for (const auto *Kind : {TEXT("skill"), TEXT("equipment"), TEXT("item")})
		for (const auto &Pair : StateObject(Owned, Kind)->Values)
		{
			int64 Count = 0;
			const FName Id(*FString(Pair.Key));
			if (Pair.Value->Type == EJson::Boolean)
				Count = Pair.Value->AsBool() ? 1 : 0;
			else if (!StateInteger(StateObject(Owned, Kind), *FString(Pair.Key), Count, 0, 2147483647, Error))
				return false;
			if (!Count)
				continue;
			const bool Equipment = FString(Kind) == TEXT("equipment"), Skill = FString(Kind) == TEXT("skill");
			if (Equipment ? !Library.Equipment.Contains(Id)
				: Skill	  ? !Library.Builds.Contains(Id)
						  : !Library.Items.Contains(Id))
			{
				Error = TEXT("unknown owned id: ") + Id.ToString();
				return false;
			}
			if (Equipment)
				Next.OwnedEquipment.Add(Id, int32(Count));
			else
				Next.Loadout.Owned.Add(Id, Skill ? 1 : int32(Count));
		}
	for (const auto &Pair : StateObject(Root, TEXT("instances"))->Values)
	{
		if (Pair.Value->Type != EJson::Object)
		{
			Error = TEXT("instance must be object");
			return false;
		}
		const auto O = Pair.Value->AsObject();
		FDWEquipmentInstance Item;
		Item.Uid = O->GetStringField(TEXT("uid"));
		Item.BaseId = FName(*O->GetStringField(TEXT("baseId")));
		const auto *Template = Library.Equipment.Find(Item.BaseId);
		if (!Template || Item.Uid != FString(Pair.Key))
		{
			Error = TEXT("unknown instance base or mismatched uid");
			return false;
		}
		Item.Name = O->GetStringField(TEXT("name"));
		O->TryGetBoolField(TEXT("identified"), Item.bIdentified);
		Item.Slot = FName(*O->GetStringField(TEXT("slot")));
		Item.Rarity = FName(*O->GetStringField(TEXT("rarity")));
		FString Skill;
		O->TryGetStringField(TEXT("skill"), Skill);
		Item.Skill = FName(*Skill);
		int64 Tier = 10, Level = 1;
		if (!StateInteger(O, TEXT("tier"), Tier, 1, 10, Error) || !StateInteger(O, TEXT("level"), Level, 1, 100, Error))
			return false;
		Item.Tier = int32(Tier);
		Item.Level = int32(Level);
		if (Item.Slot != Template->Slot ||
			(Item.bIdentified ? Item.Skill != (Template->Skills.Num() ? Template->Skills[0] : NAME_None) : !Item.Skill.IsNone()))
		{
			Error = TEXT("invalid instance slot or skill");
			return false;
		}
		const TArray<FStateValue> *Affixes;
		if (!O->TryGetArrayField(TEXT("affixes"), Affixes))
		{
			Error = TEXT("invalid random affix array");
			return false;
		}
		for (const auto &A : *Affixes)
		{
			FDWEquipmentAffix Affix;
			if (A->Type != EJson::Object || !StateAffix(A->AsObject(), Affix, Library, Error))
				return false;
			Item.Affixes.Add(Affix);
		}
		if (O->HasField(TEXT("reforgeTargets")))
		{
			const TArray<FStateValue>* Targets;
			if (!O->TryGetArrayField(TEXT("reforgeTargets"), Targets) || Targets->Num() > 3)
			{
				Error = TEXT("invalid equipment reforgeTargets");
				return false;
			}
			for (const auto& V : *Targets)
			{
				double Index;
				if (!V->TryGetNumber(Index) || !FMath::IsFinite(Index) || Index < 0 || Index >= Item.Affixes.Num() ||
					Index != FMath::FloorToDouble(Index) || Item.ReforgeTargets.Contains(int32(Index)))
				{
					Error = TEXT("invalid equipment reforge target index");
					return false;
				}
				Item.ReforgeTargets.Add(int32(Index));
			}
		}
		const auto Fixed = O->TryGetField(TEXT("legendary"));
		const TArray<FStateValue>* Bases;
		if (O->HasField(TEXT("bases")) && !O->TryGetArrayField(TEXT("bases"), Bases))
		{
			Error = TEXT("invalid equipment base array");
			return false;
		}
		if (O->TryGetArrayField(TEXT("bases"), Bases))
		{
			for (const auto& Base : *Bases)
			{
				FDWEquipmentAffix A;
				if (Base->Type != EJson::Object || !StateAffix(Base->AsObject(), A, Library, Error)) return false;
				Item.Bases.Add(MoveTemp(A));
			}
		}
		if (Fixed && Fixed->Type != EJson::Null)
		{
			FDWEquipmentAffix A;
			if (Fixed->Type != EJson::Object || !StateAffix(Fixed->AsObject(), A, Library, Error))
				return false;
			Item.Legendary = A;
		}
		if (O->HasField(TEXT("synthesized")) && !O->TryGetBoolField(TEXT("synthesized"),Item.bSynthesized)) { Error=TEXT("invalid fusion provenance");return false; }
		const TArray<FStateValue>* ExtraFixed=nullptr;
		if (O->HasField(TEXT("extraLegendary")))
		{
			if(!Item.bSynthesized || !O->TryGetArrayField(TEXT("extraLegendary"),ExtraFixed)){Error=TEXT("invalid extra legendary array");return false;}
			for(const auto& V:*ExtraFixed){FDWEquipmentAffix A;if(V->Type!=EJson::Object || !StateAffix(V->AsObject(),A,Library,Error))return false;Item.AdditionalLegendary.Add(MoveTemp(A));}
		}
		if (!FDWEquipmentRules::EnsureBases(Library, Item, Error)) return false;
		Next.Loadout.Instances.Add(MoveTemp(Item));
	}
	const auto L = StateObject(Root, TEXT("loadout"));
	TArray<FName> RequestedSkills;
	if (!StateNames(L, TEXT("skills"), 4, RequestedSkills, Error))
		return false;
	for (const auto &Pair : StateObject(L, TEXT("equipment"))->Values)
	{
		if (Pair.Value->Type == EJson::Null)
			continue;
		if (Pair.Value->Type != EJson::String)
		{
			Error = TEXT("equipment slot must be id");
			return false;
		}
		const FName Id(*Pair.Value->AsString()), Slot(*FString(Pair.Key));
		if (Id.IsNone())
			continue;
		if (!Next.Equip(Library, Slot, Id, Error))
			return false;
	}
	for (const auto &Pair : StateObject(L, TEXT("gems"))->Values)
	{
		const auto Temp = MakeShared<FJsonObject>();
		Temp->SetField(TEXT("slots"), Pair.Value);
		FDWSkillGems G;
		G.Skill = FName(*FString(Pair.Key));
		if (!StateNames(Temp, TEXT("slots"), 6, G.Slots, Error))
			return false;
		Next.Loadout.Gems.Add(MoveTemp(G));
	}
	if (!StateNames(L, TEXT("belt"), 4, Next.Belt, Error))
		return false;
	for (FName& Id : Next.Belt) {
		const auto *I = Library.Items.Find(Id);
		if (!I || I->Category != TEXT("consumable") || Next.Loadout.Owned.FindRef(Id) <= 0) Id = NAME_None;
	}
	const auto Stats = StateObject(Root, TEXT("stats"));
	if (!StateInteger(Stats, TEXT("kills"), Next.Kills, 0, 9007199254740991ll, Error) ||
		!StateInteger(Stats, TEXT("earned"), Next.Earned, 0, 9007199254740991ll, Error) ||
		!StateInteger(Stats, TEXT("spent"), Next.Spent, 0, 9007199254740991ll, Error) ||
		!StateInteger(Stats, TEXT("battles"), Next.Battles, 0, 9007199254740991ll, Error))
		return false;
	const auto Progress = Root->TryGetField(TEXT("progression"));
	if (Progress && Progress->Type == EJson::Object)
	{
		Next.bProgression = true;
		FString Weapon;
		Progress->AsObject()->TryGetStringField(TEXT("weaponId"), Weapon);
		if (!Weapon.IsEmpty())
			Next.StarterWeapon = FName(*Weapon);
		if (!StateInteger(Progress->AsObject(), TEXT("drops"), Next.Drops, 0, 9007199254740991ll, Error))
			return false;
	}
	int64 Seed = Next.Random.State;
	if (!StateInteger(Root, TEXT("nativeLootSeed"), Seed, 0, MAX_uint32, Error))
		return false;
	Next.Random.State = uint32(Seed);
	Next.Loadout.Skills = MoveTemp(RequestedSkills);
	Next.Loadout.Validate(Library);
	Out = MoveTemp(Next);
	return true;
}
FString FDWRuleStateReader::Write(const FDWRuleProfile &P, const FDWRuleLibrary &Library)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("stones"), P.Stones);
	Root->SetBoolField(TEXT("initialized"), P.bInitialized);
	Root->SetNumberField(TEXT("nativeLootSeed"), P.Random.State);
 auto Bag=MakeShared<FJsonObject>();TArray<FStateValue> Cells;
 for(const auto& Placement:P.BagLayout.Slots)
 {
  if(Placement.Key.IsEmpty()||Placement.Count<=0){Cells.Add(MakeShared<FJsonValueNull>());continue;}
  auto Cell=MakeShared<FJsonObject>();Cell->SetStringField(TEXT("key"),Placement.Key);Cell->SetStringField(TEXT("ref"),Placement.Ref);Cell->SetNumberField(TEXT("count"),Placement.Count);Cells.Add(MakeShared<FJsonValueObject>(Cell));
 }
 Bag->SetArrayField(TEXT("slots"),Cells);Bag->SetNumberField(TEXT("nextKey"),P.BagLayout.NextKey);Root->SetObjectField(TEXT("bagLayout"),Bag);
	auto Owned = MakeShared<FJsonObject>(), Skills = MakeShared<FJsonObject>(), Equipment = MakeShared<FJsonObject>(),
		 Items = MakeShared<FJsonObject>();
	for (const auto &Pair : P.Loadout.Owned)
		if (Library.Builds.Contains(Pair.Key))
			Skills->SetBoolField(StateName(Pair.Key), Pair.Value > 0);
		else
			Items->SetNumberField(StateName(Pair.Key), Pair.Value);
	for (const auto &Pair : P.OwnedEquipment)
		Equipment->SetNumberField(StateName(Pair.Key), Pair.Value);
	Owned->SetObjectField(TEXT("skill"), Skills);
	Owned->SetObjectField(TEXT("equipment"), Equipment);
	Owned->SetObjectField(TEXT("item"), Items);
	Root->SetObjectField(TEXT("owned"), Owned);
	auto Instances = MakeShared<FJsonObject>();
	for (const auto &I : P.Loadout.Instances)
	{
		auto O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("uid"), I.Uid);
		O->SetBoolField(TEXT("identified"), I.bIdentified);
		O->SetStringField(TEXT("baseId"), StateName(I.BaseId));
		O->SetStringField(TEXT("name"), I.Name);
		O->SetStringField(TEXT("slot"), StateName(I.Slot));
		O->SetStringField(TEXT("rarity"), StateName(I.Rarity));
		O->SetNumberField(TEXT("level"), I.Level);
		O->SetNumberField(TEXT("tier"), I.Tier);
		O->SetStringField(TEXT("skill"), StateName(I.Skill));
		TArray<FStateValue> Affixes;
		for (const auto &A : I.Affixes)
			Affixes.Add(StateAffixJson(A));
		O->SetArrayField(TEXT("affixes"), Affixes);
		if (!I.ReforgeTargets.IsEmpty())
		{
			TArray<FStateValue> Targets;
			for (int32 Index : I.ReforgeTargets) Targets.Add(MakeShared<FJsonValueNumber>(Index));
			O->SetArrayField(TEXT("reforgeTargets"), Targets);
		}
		TArray<FStateValue> Bases;
		for (const auto& Base : I.Bases) Bases.Add(StateAffixJson(Base));
		O->SetArrayField(TEXT("bases"), Bases);
		O->SetField(TEXT("legendary"),
					I.Legendary.IsSet() ? StateAffixJson(I.Legendary.GetValue()) : MakeShared<FJsonValueNull>());
		if(I.bSynthesized)O->SetBoolField(TEXT("synthesized"),true);
		if(!I.AdditionalLegendary.IsEmpty()){TArray<FStateValue> Extra;for(const auto& A:I.AdditionalLegendary)Extra.Add(StateAffixJson(A));O->SetArrayField(TEXT("extraLegendary"),Extra);}
		Instances->SetObjectField(I.Uid, O);
	}
	Root->SetObjectField(TEXT("instances"), Instances);
	auto L = MakeShared<FJsonObject>(), Eq = MakeShared<FJsonObject>(), Gems = MakeShared<FJsonObject>();
	L->SetArrayField(TEXT("skills"), StateNamesJson(P.Loadout.Skills));
	L->SetArrayField(TEXT("belt"), StateNamesJson(P.Belt));
	for (const auto &Pair : P.Loadout.Equipment)
		Eq->SetStringField(StateName(Pair.Key), StateName(Pair.Value));
	for (const auto &G : P.Loadout.Gems)
		Gems->SetArrayField(StateName(G.Skill), StateNamesJson(G.Slots));
	L->SetObjectField(TEXT("equipment"), Eq);
	L->SetObjectField(TEXT("gems"), Gems);
	Root->SetObjectField(TEXT("loadout"), L);
	auto Stats = MakeShared<FJsonObject>();
	Stats->SetNumberField(TEXT("kills"), P.Kills);
	Stats->SetNumberField(TEXT("earned"), P.Earned);
	Stats->SetNumberField(TEXT("spent"), P.Spent);
	Stats->SetNumberField(TEXT("battles"), P.Battles);
	Root->SetObjectField(TEXT("stats"), Stats);
	if (P.bProgression)
	{
		auto G = MakeShared<FJsonObject>();
		G->SetStringField(TEXT("weaponId"), StateName(P.StarterWeapon));
		G->SetNumberField(TEXT("drops"), P.Drops);
		Root->SetObjectField(TEXT("progression"), G);
	}
	else
		Root->SetField(TEXT("progression"), MakeShared<FJsonValueNull>());
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	return Text;
}
