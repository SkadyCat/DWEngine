#include "DWRuleProgramReader.h"
#include "DWRuleSheet.h"

#include "DWRuleAssets.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

using namespace DWRules;

namespace
{
	using FObject = TSharedPtr<FJsonObject>;
	using FValue = TSharedPtr<FJsonValue>;

	const TArray<FValue>& EmptyList()
	{
		static const TArray<FValue> Empty;
		return Empty;
	}

	const TArray<FValue>& ListOf(const FValue& Value)
	{
		return Value && Value->Type == EJson::Array ? Value->AsArray() : EmptyList();
	}

	FObject ObjectOf(const FValue& Value)
	{
		return Value && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
	}

	bool Parse(const FString& Text, FObject& Out)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	}

	struct FReader
	{
		FString Error;

		bool Fail(const FString& Path, const FString& Message)
		{
			if (Error.IsEmpty())
			{
				Error = FString::Printf(TEXT("%s: %s"), *Path, *Message);
			}
			return false;
		}

		bool Closed(const FObject& Object, std::initializer_list<const TCHAR*> Allowed, const FString& Path)
		{
			for (const TPair<FString, FValue>& Pair : Object->Values)
			{
				bool bKnown = false;
				for (const TCHAR* Field : Allowed)
				{
					bKnown |= Pair.Key == Field;
				}
				if (!bKnown)
				{
					return Fail(Path, FString::Printf(TEXT("unknown field %s"), *Pair.Key));
				}
			}
			return true;
		}

		static FString String(const FObject& Object, const TCHAR* Field)
		{
			FString Out;
			Object->TryGetStringField(Field, Out);
			return Out;
		}

		static FName Name(const FObject& Object, const TCHAR* Field)
		{
			const FString Text = String(Object, Field);
			return Text.IsEmpty() ? NAME_None : FName(*Text);
		}

		bool ReadScope(const FString& Text, EScope& Out, const FString& Path)
		{
			if (Text.IsEmpty() || Text == TEXT("owner")) { Out = EScope::Owner; return true; }
			if (Text == TEXT("target")) { Out = EScope::Target; return true; }
			if (Text == TEXT("cast")) { Out = EScope::Cast; return true; }
			return Fail(Path, TEXT("scope must be owner/target/cast"));
		}

		bool ReadCompare(const FString& Text, ECompare& Out, const FString& Path)
		{
			static const TCHAR* const Names[] = { TEXT("eq"), TEXT("ne"), TEXT("gt"), TEXT("gte"), TEXT("lt"), TEXT("lte") };
			for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Names)); ++Index)
			{
				if (Text == Names[Index])
				{
					Out = ECompare(Index);
					return true;
				}
			}
			return Fail(Path, TEXT("compare must be eq/ne/gt/gte/lt/lte"));
		}

		bool Expr(const FValue& Value, FDWExprSource& Out, const FString& Path)
		{
			if (!Value)
			{
				return Fail(Path, TEXT("missing value"));
			}
			if (Value->Type == EJson::Number)
			{
				Out = FDWExprSource::Number(Value->AsNumber());
				return true;
			}
			const FObject Object = ObjectOf(Value);
			if (!Object)
			{
				return Fail(Path, TEXT("expected number or expression"));
			}
			static const TPair<const TCHAR*, FDWExprSource::EKind> Arith[] = {
				{ TEXT("mul"), FDWExprSource::EKind::Mul },
				{ TEXT("add"), FDWExprSource::EKind::Add },
				{ TEXT("div"), FDWExprSource::EKind::Div },
				{ TEXT("min"), FDWExprSource::EKind::Min },
				{ TEXT("max"), FDWExprSource::EKind::Max },
				{ TEXT("pow"), FDWExprSource::EKind::Pow }
			};
			for (const TPair<const TCHAR*, FDWExprSource::EKind>& Entry : Arith)
			{
				const FValue Terms = Object->TryGetField(Entry.Key);
				if (!Terms)
				{
					continue;
				}
				Out.Kind = Entry.Value;
				const TArray<FValue>& List = ListOf(Terms);
				for (int32 Index = 0; Index < List.Num(); ++Index)
				{
					FDWExprSource Term;
					if (!Expr(List[Index], Term, FString::Printf(TEXT("%s.%s[%d]"), *Path, Entry.Key, Index)))
					{
						return false;
					}
					Out.Args.Add(MoveTemp(Term));
				}
				return true;
			}
			if (Object->HasField(TEXT("param")))
			{
				Out = FDWExprSource::Param(Name(Object, TEXT("param")));
				return true;
			}
			if (Object->HasField(TEXT("event")))
			{
				Out = FDWExprSource::Event(Name(Object, TEXT("event")));
				return true;
			}
			if (Object->HasField(TEXT("stat")))
			{
				Out.Kind = FDWExprSource::EKind::Stat;
				Out.Name = Name(Object, TEXT("stat"));
				return true;
			}
			if (Object->HasField(TEXT("state")))
			{
				Out.Kind = FDWExprSource::EKind::State;
				Out.Name = Name(Object, TEXT("state"));
				Out.Channel = Name(Object, TEXT("channel"));
				return ReadScope(String(Object, TEXT("scope")), Out.Scope, Path);
			}
			return Fail(Path, TEXT("unknown expression"));
		}

		bool ReadModifier(const FObject& Object, FDWModifierSource& Out, const FString& Path)
		{
			if (!Closed(Object, { TEXT("target"), TEXT("stat"), TEXT("op"), TEXT("value"), TEXT("element"), TEXT("scope"), TEXT("filter") }, Path))
			{
				return false;
			}
			const FString Target = String(Object, TEXT("target"));
			const FString Stat = String(Object, TEXT("stat"));
			Out.Stat = Target == TEXT("element")
				? FString::Printf(TEXT("element.%s.%s"), *String(Object, TEXT("element")), *Stat)
				: FString::Printf(TEXT("%s.%s"), *Target, *Stat);
			const FString OpText = String(Object, TEXT("op"));
			if (OpText == TEXT("add")) Out.Op = EModOp::Add;
			else if (OpText == TEXT("multiply")) Out.Op = EModOp::Multiply;
			else if (OpText == TEXT("set")) Out.Op = EModOp::Set;
			else if (OpText == TEXT("multiply_percent")) Out.Op = EModOp::IncreasedPercent;
			else if (OpText == TEXT("reduce_percent")) Out.Op = EModOp::ReducedPercent;
			else if (OpText == TEXT("min")) Out.Op = EModOp::Min;
			else if (OpText == TEXT("max")) Out.Op = EModOp::Max;
			else return Fail(Path, FString::Printf(TEXT("unknown modifier op %s"), *OpText));
			const FValue Value = Object->TryGetField(TEXT("value"));
			if (Value && Value->Type == EJson::String)
			{
				return Fail(Path, TEXT("enumerated modifiers (layout/emission_mode) are not supported yet"));
			}
			return Expr(Value, Out.Value, Path + TEXT(".value"));
		}

		bool ReadCondition(const FObject& Object, FDWConditionSource& Out, const FString& Path)
		{
			if (!Closed(Object, { TEXT("op"), TEXT("tag"), TEXT("key"), TEXT("kind"), TEXT("channel"), TEXT("scope"),
				TEXT("compare"), TEXT("value"), TEXT("max"), TEXT("probability") }, Path))
			{
				return false;
			}
			Out.Op = Name(Object, TEXT("op"));
			Out.Tag = Name(Object, TEXT("tag"));
			Out.Key = Name(Object, TEXT("key"));
			Out.Kind = Name(Object, TEXT("kind"));
			Out.Channel = Name(Object, TEXT("channel"));
			if (!ReadScope(String(Object, TEXT("scope")), Out.Scope, Path))
			{
				return false;
			}
			if (Object->HasField(TEXT("compare")) && !ReadCompare(String(Object, TEXT("compare")), Out.Compare, Path))
			{
				return false;
			}
			for (const TCHAR* Field : { TEXT("value"), TEXT("max"), TEXT("probability") })
			{
				if (const FValue Value = Object->TryGetField(Field))
				{
					return Expr(Value, Out.Value, Path + TEXT(".") + Field);
				}
			}
			return true;
		}

		bool ReadOp(const FString& Text, EOp& Out, const FString& Path)
		{
			static const TPair<const TCHAR*, EOp> Ops[] = {
				{ TEXT("emit"), EOp::Emit },
				{ TEXT("copy"), EOp::Copy },
				{ TEXT("damage_area"), EOp::DamageArea },
				{ TEXT("damage_target"), EOp::DamageTarget },
				{ TEXT("counter"), EOp::Counter },
				{ TEXT("accumulate"), EOp::Accumulate },
				{ TEXT("consume"), EOp::Consume },
				{ TEXT("emit_event"), EOp::EmitEvent },
				{ TEXT("redirect"), EOp::Redirect },
				{ TEXT("apply_status"), EOp::ApplyStatus },
				{ TEXT("grant_traits"), EOp::GrantTraits },
                { TEXT("summon"), EOp::Summon },
                { TEXT("storm"), EOp::Storm },
                { TEXT("sacrifice"), EOp::Sacrifice },
                { TEXT("consume_status"), EOp::ConsumeStatus },
                { TEXT("repeat_skill"), EOp::RepeatSkill },
                { TEXT("maintain_emitter"), EOp::MaintainEmitter },
                { TEXT("spawn_monster"), EOp::SpawnMonster },
                { TEXT("multiply_damage"), EOp::MultiplyDamage }
			};
			for (const TPair<const TCHAR*, EOp>& Pair : Ops)
			{
				if (Text == Pair.Key)
				{
					Out = Pair.Value;
					return true;
				}
			}
			return Fail(Path, FString::Printf(TEXT("operator %s is not implemented in the native runtime yet"), *Text));
		}

		bool ReadAction(const FObject& Object, FDWActionSource& Out, const FString& Path, int32 Depth)
		{
			if (Depth > MaxActionDepth)
			{
				return Fail(Path, TEXT("nesting exceeds 12"));
			}
			if (!Closed(Object, {TEXT("op"),		  TEXT("count"),	  TEXT("spread"),	 TEXT("speed"),
								 TEXT("lifetime"),	  TEXT("damage"),	  TEXT("radius"),	 TEXT("amount"),
								 TEXT("cap"),		  TEXT("threshold"),  TEXT("pierce"),	 TEXT("duration"),
								 TEXT("interval"),	  TEXT("value"),	  TEXT("kind"),		 TEXT("event"),
								 TEXT("emitEvent"),	  TEXT("endEvent"),	  TEXT("key"),		 TEXT("channel"),
								 TEXT("scope"),		  TEXT("layout"),	  TEXT("origin"),	 TEXT("mode"),
								 TEXT("inheritance"), TEXT("element"),	  TEXT("tags"),		 TEXT("then"),
								 TEXT("traits"),	  TEXT("trajectory"), TEXT("arc_height"),
                                 TEXT("aim"), TEXT("offset"), TEXT("scatter"), TEXT("targeting"), TEXT("rate"), TEXT("decay_interval"), TEXT("invulnerable")},
						Path))
			{
				return false;
			}
			if (!ReadOp(String(Object, TEXT("op")), Out.Op, Path))
			{
				return false;
			}
			if (Out.Op == EOp::GrantTraits)
			{
				const FValue Grant = Object->TryGetField(TEXT("traits"));
				if (Grant && Grant->Type == EJson::String && Grant->AsString().Equals(TEXT("stitched"), ESearchCase::IgnoreCase))
					Out.bHasGrant = Out.bGrantStitched = true;
				else if (Grant && Grant->Type == EJson::Array && Grant->AsArray().Num() > 0)
				{
					Out.bHasGrant = true;
					for (const FValue& Item : Grant->AsArray())
					{
						FDWTraitEntry Entry;
						if (const FObject Obj = ObjectOf(Item); Obj && !Closed(Obj, {TEXT("id"), TEXT("params")}, Path + TEXT(".traits"))) return false;
						if (!ReadEntry(Item, Entry, Path + TEXT(".traits"), 0)) return false;
						Out.GrantEntries.Add(MoveTemp(Entry));
					}
				}
				else return Fail(Path, TEXT("grant_traits needs traits"));
			}
			else if (Object->HasField(TEXT("traits"))) return Fail(Path, TEXT("unknown field traits"));
			static const TCHAR *const Numeric[] = {
				TEXT("count"),	  TEXT("spread"),	TEXT("speed"),	   TEXT("lifetime"),  TEXT("damage"),
				TEXT("radius"),	  TEXT("amount"),	TEXT("cap"),	   TEXT("threshold"), TEXT("pierce"),
				TEXT("duration"), TEXT("interval"), TEXT("arc_height"), TEXT("rate"), TEXT("decay_interval")};
			for (const TCHAR* Field : Numeric)
			{
				if (const FValue Value = Object->TryGetField(Field))
				{
					FDWExprSource Number;
					if (!Expr(Value, Number, Path + TEXT(".") + Field))
					{
						return false;
					}
					Out.Numbers.Add(Field, MoveTemp(Number));
				}
			}
			if (const FValue Value = Object->TryGetField(TEXT("value")))
			{
				if (Value->Type == EJson::String && Value->AsString() == TEXT("all"))
				{
					Out.bConsumeAll = true;
				}
				else
				{
					FDWExprSource Number;
					if (!Expr(Value, Number, Path + TEXT(".value")))
					{
						return false;
					}
					Out.Numbers.Add(TEXT("value"), MoveTemp(Number));
				}
			}
			const FString Trajectory = String(Object, TEXT("trajectory"));
			if ((!Trajectory.IsEmpty() && Trajectory != TEXT("linear") && Trajectory != TEXT("parabolic")) ||
				((!Trajectory.IsEmpty() || Object->HasField(TEXT("arc_height"))) && Out.Op != EOp::Emit))
				return Fail(Path, TEXT("invalid emit trajectory"));
			Out.bParabolic = Trajectory == TEXT("parabolic");
			Out.Kind = Name(Object, TEXT("kind"));
            Out.bAimTarget = String(Object,TEXT("aim"))==TEXT("target");
            Out.bFeed = String(Object,TEXT("mode"))==TEXT("feed");
            Out.Targeting = Name(Object,TEXT("targeting"));
            Object->TryGetBoolField(TEXT("invulnerable"),Out.bInvulnerable);
            if(const auto Offset=ObjectOf(Object->TryGetField(TEXT("offset"))))
            {
                if(!Closed(Offset,{TEXT("forward"),TEXT("up")},Path+TEXT(".offset")))return false;
                for(const TCHAR* Field:{TEXT("forward"),TEXT("up")})if(const auto V=Offset->TryGetField(Field))
                {FDWExprSource N;if(!Expr(V,N,Path+TEXT(".offset.")+Field))return false;Out.Numbers.Add(FName(*(FString(TEXT("offset_"))+Field)),MoveTemp(N));}
            }
            if(const auto Scatter=ObjectOf(Object->TryGetField(TEXT("scatter"))))
            {
                if(!Closed(Scatter,{TEXT("shape"),TEXT("radius"),TEXT("distribution")},Path+TEXT(".scatter")))return false;
                if(String(Scatter,TEXT("shape"))!=TEXT("hemisphere")||String(Scatter,TEXT("distribution"))!=TEXT("random"))return Fail(Path,TEXT("unsupported scatter geometry"));
                Out.bHemisphere=true;FDWExprSource N;if(!Expr(Scatter->TryGetField(TEXT("radius")),N,Path+TEXT(".scatter.radius")))return false;Out.Numbers.Add(TEXT("scatter_radius"),MoveTemp(N));
            }
			Out.Event = Name(Object, TEXT("event"));
			Out.EmitEvent = Name(Object, TEXT("emitEvent"));
			Out.EndEvent = Name(Object, TEXT("endEvent"));
			Out.Key = Name(Object, TEXT("key"));
			Out.Channel = Name(Object, TEXT("channel"));
			if (!ReadScope(String(Object, TEXT("scope")), Out.Scope, Path))
			{
				return false;
			}
			const FString Layout = String(Object, TEXT("layout"));
			Out.Layout = Layout == TEXT("radial") ? ELayout::Radial : (Layout == TEXT("point") ? ELayout::Point : ELayout::Fan);
			const FString Origin = String(Object, TEXT("origin"));
			Out.Origin = Origin == TEXT("owner") ? EOrigin::Owner : Origin == TEXT("source") ? EOrigin::Source
				: Origin == TEXT("target") ? EOrigin::Target : EOrigin::Event;
			const FString Mode = String(Object, TEXT("mode"));
			if (Out.Op == EOp::Redirect)
			{
				if (Mode == TEXT("ricochet")) Out.Mode = ERedirect::Ricochet;
				else if (Mode == TEXT("reflect")) Out.Mode = ERedirect::Reflect;
				else if (Mode == TEXT("return")) Out.Mode = ERedirect::Return;
				else return Fail(Path, FString::Printf(TEXT("redirect mode %s is not implemented yet"), *Mode));
			}
			Out.bInherit = String(Object, TEXT("inheritance")) != TEXT("explicit");
			const FString Element = String(Object, TEXT("element"));
			if (!Element.IsEmpty())
			{
				EElement Parsed;
				if (!ParseElement(Element, Parsed))
				{
					return Fail(Path, FString::Printf(TEXT("unknown element %s"), *Element));
				}
				Out.Element = int8(Parsed);
			}
			for (const FValue& Tag : ListOf(Object->TryGetField(TEXT("tags"))))
			{
				Out.Tags.Add(FName(*Tag->AsString()));
			}
			if (const FValue Then = Object->TryGetField(TEXT("then")))
			{
				return ReadActions(Then, Out.Then, Path + TEXT(".then"), Depth + 1);
			}
			return true;
		}

		bool ReadActions(const FValue& List, TArray<FDWActionSource>& Out, const FString& Path, int32 Depth)
		{
			const TArray<FValue>& Items = ListOf(List);
			for (int32 Index = 0; Index < Items.Num(); ++Index)
			{
				const FObject Object = ObjectOf(Items[Index]);
				FDWActionSource Item;
				const FString ItemPath = FString::Printf(TEXT("%s[%d]"), *Path, Index);
				if (!Object)
				{
					return Fail(ItemPath, TEXT("expected action object"));
				}
				if (!ReadAction(Object, Item, ItemPath, Depth))
				{
					return false;
				}
				Out.Add(MoveTemp(Item));
			}
			return true;
		}

		bool ReadRule(const FObject& Object, FDWRuleSource& Out, const FString& Path)
		{
			if (!Closed(Object, { TEXT("id"), TEXT("event"), TEXT("conditions"), TEXT("actions"), TEXT("chain") }, Path))
			{
				return false;
			}
			Out.Id = Name(Object, TEXT("id"));
			Out.Event = Name(Object, TEXT("event"));
			if (const FObject Chain = ObjectOf(Object->TryGetField(TEXT("chain"))))
			{
				Chain->TryGetNumberField(TEXT("maxDepth"), Out.MaxDepth);
				Chain->TryGetNumberField(TEXT("maxEvents"), Out.MaxEvents);
				Out.Channel = Name(Chain, TEXT("channel"));
			}
			const TArray<FValue>& Conditions = ListOf(Object->TryGetField(TEXT("conditions")));
			for (int32 Index = 0; Index < Conditions.Num(); ++Index)
			{
				const FObject CondObject = ObjectOf(Conditions[Index]);
				FDWConditionSource Source;
				const FString CondPath = FString::Printf(TEXT("%s.conditions[%d]"), *Path, Index);
				if (!CondObject)
				{
					return Fail(CondPath, TEXT("expected condition object"));
				}
				if (!ReadCondition(CondObject, Source, CondPath))
				{
					return false;
				}
				Out.Conditions.Add(MoveTemp(Source));
			}
			const FValue List = Object->TryGetField(TEXT("actions"));
			return List ? ReadActions(List, Out.Actions, Path + TEXT(".actions"), 0) : Fail(Path, TEXT("rule needs actions"));
		}

		bool ReadEntry(const FValue& Value, FDWTraitEntry& Out, const FString& Path, int32 Depth)
		{
			if (Depth > 3) return Fail(Path, TEXT("trait stitching exceeds 3"));
			if (Value && Value->Type == EJson::String)
			{
				Out.Id = FName(*Value->AsString());
				return !Out.Id.IsNone() || Fail(Path, TEXT("expected trait id"));
			}
			const FObject Object = ObjectOf(Value);
			if (!Object || !Closed(Object, {TEXT("id"), TEXT("params"), TEXT("with"), TEXT("duration"), TEXT("tier")}, Path)) return false;
			Out.Id = Name(Object, TEXT("id"));
			if (Out.Id.IsNone()) return Fail(Path, TEXT("expected trait id"));
			if (Object->HasField(TEXT("tier")))
			{
				double Tier;
				if (!Object->TryGetNumberField(TEXT("tier"), Tier) || !FMath::IsFinite(Tier) || Tier < 1 || Tier > 10 || FMath::FloorToDouble(Tier) != Tier) return Fail(Path, TEXT("tier must be an integer from 1 to 10"));
				Out.Tier = int32(Tier);
			}
			if (Object->HasField(TEXT("params")))
			{
				const FObject Params = ObjectOf(Object->TryGetField(TEXT("params")));
				if (!Params) return Fail(Path, TEXT("params must be an object"));
				for (const auto& P : Params->Values)
				{
					if (!P.Value || P.Value->Type != EJson::Number || !FMath::IsFinite(P.Value->AsNumber())) return Fail(Path, TEXT("params must be finite numbers"));
					Out.Params.Add(FName(*P.Key), P.Value->AsNumber());
				}
			}
			if (Object->HasField(TEXT("duration")))
			{
				if (!Object->TryGetNumberField(TEXT("duration"), Out.Duration) || !FMath::IsFinite(Out.Duration) || Out.Duration < 0.0 || Out.Duration > 3600.0 || (Depth>0 && Out.Duration>0)) return Fail(Path, TEXT("duration must be positive, at most 3600 seconds and only on the outer entry"));
			}
			if (Object->HasField(TEXT("with")))
			{
				const FValue With = Object->TryGetField(TEXT("with"));
				if (!With || With->Type != EJson::Array) return Fail(Path, TEXT("with must be an array"));
				for (const FValue& Child : With->AsArray())
				{
					FDWTraitEntry Entry;
					if (!ReadEntry(Child, Entry, Path + TEXT(".with"), Depth + 1)) return false;
					Out.With.Add(MoveTemp(Entry));
				}
				TSet<FName> Seen;
				for (const auto& Entry : Out.With) if (Entry.With.IsEmpty())
				{
					if (Seen.Contains(Entry.Id)) return Fail(Path, TEXT("duplicate stitched trait"));
					Seen.Add(Entry.Id);
				}
			}
			return true;
		}

		bool ReadSheetValue(const FValue& Value, FDWSheetValue& Out, const FString& Path, int32 Depth = 0)
		{
			if (Depth > 32) return Fail(Path, TEXT("sheet expression nesting exceeds 32"));
			if (Value && Value->Type == EJson::Number) { Out.Number = Value->AsNumber(); return FMath::IsFinite(Out.Number) || Fail(Path, TEXT("expected finite number")); }
			const FObject Object = ObjectOf(Value);
			if (!Object || Object->Values.Num() != 1) return Fail(Path, TEXT("expected sheet value expression"));
			if (Object->HasTypedField<EJson::String>(TEXT("param"))) { Out.Kind = FDWSheetValue::EKind::Param; Out.Param = Name(Object, TEXT("param")); return true; }
			const TCHAR* Ops[] = { TEXT("add"), TEXT("sub"), TEXT("mul"), TEXT("div") };
			for (int32 I = 0; I < 4; ++I)
			{
				const FValue Args = Object->TryGetField(Ops[I]);
				if (!Args) continue;
				if (Args->Type != EJson::Array || Args->AsArray().Num() < 2) return Fail(Path, TEXT("sheet operation needs at least two arguments"));
				Out.Kind = FDWSheetValue::EKind(int32(FDWSheetValue::EKind::Add) + I);
				for (const FValue& Arg : Args->AsArray())
				{
					FDWSheetValue Child;
					if (!ReadSheetValue(Arg, Child, Path, Depth + 1)) return false;
					Out.Args.Add(MoveTemp(Child));
				}
				return true;
			}
			return Fail(Path, TEXT("unknown sheet value expression"));
		}

		bool ReadSheet(const FObject& Root, FDWTraitSource& Trait)
		{
			if (!Root->HasField(TEXT("sheet"))) return true;
			const FValue Sheet = Root->TryGetField(TEXT("sheet"));
			if (!Sheet || Sheet->Type != EJson::Array) return Fail(Trait.Id.ToString(), TEXT("sheet must be an array"));
			for (int32 I = 0; I < Sheet->AsArray().Num(); ++I)
			{
				const FObject Obj = ObjectOf(Sheet->AsArray()[I]);
				const FString Path = FString::Printf(TEXT("%s.sheet[%d]"), *Trait.Id.ToString(), I);
				if (!Obj) return Fail(Path, TEXT("expected sheet entry"));
				FDWSheetEntry Entry;
				if (Obj->HasField(TEXT("grant_skill")))
                {
                    if (!Closed(Obj, {TEXT("grant_skill")}, Path)) return false;
                    Entry.GrantSkill=Name(Obj,TEXT("grant_skill"));
                    if(Entry.GrantSkill.IsNone())return Fail(Path,TEXT("expected granted skill name"));
                }
                else if (Obj->HasField(TEXT("trigger")))
				{
					if (!Closed(Obj, {TEXT("trigger")}, Path)) return false;
					Entry.Trigger = Name(Obj, TEXT("trigger"));
					if (Entry.Trigger.IsNone()) return Fail(Path, TEXT("expected trigger name"));
				}
				else
				{
					if (!Closed(Obj, {TEXT("stat"), TEXT("value")}, Path)) return false;
					Entry.Stat = Name(Obj, TEXT("stat"));
					if (!ReadSheetValue(Obj->TryGetField(TEXT("value")), Entry.Value, Path)) return false;
				}
				Trait.Sheet.Add(MoveTemp(Entry));
			}
			if (!ValidateSheet(Trait, Error)) return false;
			return true;
		}

		bool ReadProgram(const FObject& Root, FDWTraitSource& Trait)
		{
			const TArray<FValue>& Modifiers = ListOf(Root->TryGetField(TEXT("modifiers")));
			for (int32 Index = 0; Index < Modifiers.Num(); ++Index)
			{
				const FObject Object = ObjectOf(Modifiers[Index]);
				FDWModifierSource Modifier;
				const FString Path = FString::Printf(TEXT("%s.modifiers[%d]"), *Trait.Id.ToString(), Index);
				if (!Object)
				{
					return Fail(Path, TEXT("expected modifier object"));
				}
				if (!ReadModifier(Object, Modifier, Path))
				{
					return false;
				}
				Trait.Modifiers.Add(MoveTemp(Modifier));
			}
			const TArray<FValue>& Rules = ListOf(Root->TryGetField(TEXT("rules")));
			for (int32 Index = 0; Index < Rules.Num(); ++Index)
			{
				const FObject Object = ObjectOf(Rules[Index]);
				FDWRuleSource Rule;
				const FString Path = FString::Printf(TEXT("%s.rules[%d]"), *Trait.Id.ToString(), Index);
				if (!Object)
				{
					return Fail(Path, TEXT("expected rule object"));
				}
				if (!ReadRule(Object, Rule, Path))
				{
					return false;
				}
				Trait.Rules.Add(MoveTemp(Rule));
			}
			return true;
		}
	};
}

bool FDWRuleProgramReader::ReadProgramText(const FString& Json, FDWTraitSource& InOutTrait, FString& OutError)
{
	FObject Root;
	if (!Parse(Json, Root))
	{
		OutError = FString::Printf(TEXT("%s: program is not valid JSON"), *InOutTrait.Id.ToString());
		return false;
	}
	FReader Reader;
	if (!Reader.Closed(Root, { TEXT("modifiers"), TEXT("rules") }, InOutTrait.Id.ToString()) || !Reader.ReadProgram(Root, InOutTrait))
	{
		OutError = Reader.Error;
		return false;
	}
	return true;
}

bool FDWRuleProgramReader::ReadTraitEntry(const TSharedPtr<FJsonValue>& Value, FDWTraitEntry& OutEntry, FString& OutError)
{
	OutEntry = FDWTraitEntry();
	FReader Reader;
	const bool Result = Reader.ReadEntry(Value, OutEntry, TEXT("trait entry"), 0);
	OutError = Reader.Error;
	return Result;
}

bool FDWRuleProgramReader::ReadTraitText(const FString& Json, FDWTraitSource& OutTrait, FString& OutDisplayName, FString& OutError)
{
	OutTrait = FDWTraitSource();
	FObject Root;
	if (!Parse(Json, Root))
	{
		OutError = TEXT("trait file is not valid JSON");
		return false;
	}
	FReader Reader;
	if (!Reader.Closed(Root, { TEXT("id"), TEXT("parent"), TEXT("name"), TEXT("description"), TEXT("params"), TEXT("modifiers"), TEXT("rules"), TEXT("sheet") }, TEXT("trait")))
	{
		OutError = Reader.Error;
		return false;
	}
	OutTrait.Id = FReader::Name(Root, TEXT("id"));
	if (OutTrait.Id.IsNone())
	{
		OutError = TEXT("trait needs id");
		return false;
	}
	OutTrait.ParentId = FReader::Name(Root, TEXT("parent"));
	OutDisplayName = FReader::String(Root, TEXT("name"));
	if (const FObject Params = ObjectOf(Root->TryGetField(TEXT("params"))))
	{
		for (const TPair<FString, FValue>& Pair : Params->Values)
		{
			if (!Pair.Value || Pair.Value->Type != EJson::Number)
			{
				OutError = FString::Printf(TEXT("%s.params.%s must be a number"), *OutTrait.Id.ToString(), *Pair.Key);
				return false;
			}
			OutTrait.Params.Add(FName(*Pair.Key), Pair.Value->AsNumber());
		}
	}
	if (!Reader.ReadProgram(Root, OutTrait) || !Reader.ReadSheet(Root, OutTrait))
	{
		OutError = Reader.Error;
		return false;
	}
	return true;
}

bool FDWRuleProgramReader::ReadStatusText(const FString& Json, FDWStatusDef& OutStatus, FString& OutDisplayName, FString& OutError)
{
	OutStatus = FDWStatusDef();
	FObject Root;
	if (!Parse(Json, Root))
	{
		OutError = TEXT("status file is not valid JSON");
		return false;
	}
	FReader Reader;
	if (!Reader.Closed(Root, { TEXT("id"), TEXT("name"), TEXT("description"), TEXT("category"), TEXT("stacking"), TEXT("maxStacks"), TEXT("cap"), TEXT("effects"), TEXT("traits") }, TEXT("status")))
	{
		OutError = Reader.Error;
		return false;
	}
	const FString Id = FReader::String(Root, TEXT("id"));
	if (Id.IsEmpty())
	{
		OutError = TEXT("status needs id");
		return false;
	}
	auto Fail = [&OutError](const FString& Message)
	{
		OutError = Message;
		return false;
	};
	OutStatus.Id = FName(*Id);
	OutStatus.Key = StatusKey(Id);
	OutDisplayName = FReader::String(Root, TEXT("name"));
	OutStatus.DisplayName = OutDisplayName;
	OutStatus.Description = FReader::String(Root, TEXT("description"));
	FString Category = FReader::String(Root, TEXT("category")).ToLower();
	if (Category.IsEmpty())
	{
		Category = TEXT("debuff");
	}
	if (Category != TEXT("ailment") && Category != TEXT("debuff") && Category != TEXT("buff"))
	{
		return Fail(Id + TEXT(": category must be ailment/debuff/buff"));
	}
	OutStatus.Category = FName(*Category);
	const FString Stacking = FReader::String(Root, TEXT("stacking")).ToLower();
	if (Stacking.IsEmpty() || Stacking == TEXT("strongest")) { OutStatus.Stacking = EStatusStacking::Strongest; }
	else if (Stacking == TEXT("refresh")) { OutStatus.Stacking = EStatusStacking::Refresh; }
	else if (Stacking == TEXT("stack")) { OutStatus.Stacking = EStatusStacking::Stack; }
	else
	{
		return Fail(Id + TEXT(": stacking must be strongest/refresh/stack"));
	}
	if (const FValue MaxStacks = Root->TryGetField(TEXT("maxStacks")))
	{
		const double N = MaxStacks->Type == EJson::Number ? MaxStacks->AsNumber() : -1.0;
		if (N != FMath::FloorToDouble(N) || N < 1.0 || N > double(MaxStatusStacks))
		{
			return Fail(FString::Printf(TEXT("%s: maxStacks must be an integer 1-%d"), *Id, MaxStatusStacks));
		}
		OutStatus.MaxStacks = int32(N);
	}
	if (const FValue Cap = Root->TryGetField(TEXT("cap")))
	{
		const double N = Cap->Type == EJson::Number ? Cap->AsNumber() : -1.0;
		if (!(N >= 0.0) || !FMath::IsFinite(N))
		{
			return Fail(Id + TEXT(": cap must be a number >= 0"));
		}
		OutStatus.Cap = float(N);
	}
	const FValue EffectsValue = Root->TryGetField(TEXT("effects"));
	if (EffectsValue && EffectsValue->Type != EJson::Array)
	{
		return Fail(Id + TEXT(": effects must be a list"));
	}
	const TArray<FValue>& Effects = ListOf(EffectsValue);
	for (int32 Index = 0; Index < Effects.Num(); ++Index)
	{
		const FString Path = FString::Printf(TEXT("%s.effects[%d]"), *Id, Index);
		const FObject Object = ObjectOf(Effects[Index]);
		if (!Object)
		{
			return Fail(Path + TEXT(": expected effect object"));
		}
		if (!Reader.Closed(Object, { TEXT("type"), TEXT("scale"), TEXT("element"), TEXT("interval"), TEXT("control") }, Path))
		{
			OutError = Reader.Error;
			return false;
		}
		FDWStatusEffectDef Effect;
		const FString Type = FReader::String(Object, TEXT("type"));
		if (!ParseStatusEffect(Type, Effect.Type))
		{
			return Fail(Path + TEXT(": unknown effect type ") + Type);
		}
		if (const FValue Scale = Object->TryGetField(TEXT("scale")))
		{
			if (Scale->Type != EJson::Number || !FMath::IsFinite(Scale->AsNumber()))
			{
				return Fail(Path + TEXT(": scale must be a number"));
			}
			Effect.Scale = float(Scale->AsNumber());
		}
		const bool bHasElement = Object->HasField(TEXT("element"));
		const bool bHasInterval = Object->HasField(TEXT("interval"));
		if (Effect.Type == EStatusEffect::DamageOverTime)
		{
			if (OutStatus.TickInterval > 0.f)
			{
				return Fail(Path + TEXT(": only one damage_over_time effect per status"));
			}
			EElement Element;
			if (!ParseElement(FReader::String(Object, TEXT("element")), Element))
			{
				return Fail(Path + TEXT(": damage_over_time needs a valid element"));
			}
			double Interval = 0.5;
			if (const FValue IntervalValue = Object->TryGetField(TEXT("interval")))
			{
				Interval = IntervalValue->Type == EJson::Number ? IntervalValue->AsNumber() : -1.0;
				if (!(Interval >= MinTickInterval) || !FMath::IsFinite(Interval))
				{
					return Fail(FString::Printf(TEXT("%s: interval must be >= %g"), *Path, MinTickInterval));
				}
			}
			Effect.Element = int8(Element);
			Effect.Interval = float(Interval);
			OutStatus.TickInterval = Effect.Interval;
			OutStatus.TickElement = Effect.Element;
		}
		else if (bHasElement || bHasInterval)
		{
			return Fail(Path + TEXT(": element/interval only apply to damage_over_time"));
		}
		const FString Control = FReader::String(Object, TEXT("control")).ToLower();
		if (Effect.Type == EStatusEffect::Control)
		{
			if (Control == TEXT("freeze")) { Effect.Control = EStatusControl::Freeze; }
			else if (Control == TEXT("stun")) { Effect.Control = EStatusControl::Stun; }
			else
			{
				return Fail(Path + TEXT(": control must be freeze/stun"));
			}
			OutStatus.Control = EStatusControl(FMath::Max(uint8(OutStatus.Control), uint8(Effect.Control)));
		}
		else if (Object->HasField(TEXT("control")))
		{
			return Fail(Path + TEXT(": control only applies to type control"));
		}
		OutStatus.Effects.Add(Effect);
	}
	if (const FValue TraitsValue = Root->TryGetField(TEXT("traits")))
	{
		if (TraitsValue->Type != EJson::Array)
		{
			return Fail(Id + TEXT(": traits must be a list of trait ids"));
		}
		for (const FValue& Trait : TraitsValue->AsArray())
		{
			if (!Trait || Trait->Type != EJson::String || Trait->AsString().IsEmpty())
			{
				return Fail(Id + TEXT(": traits must be a list of trait ids"));
			}
			OutStatus.Traits.AddUnique(FName(*Trait->AsString()));
		}
	}
	return true;
}

bool FDWRuleProgramReader::ReadBuildText(const FString& Json, FDWRuleBuildDef& OutBuild, FString& OutError)
{
	OutBuild = FDWRuleBuildDef();
	FObject Root;
	if (!Parse(Json, Root))
	{
		OutError = TEXT("build file is not valid JSON");
		return false;
	}
	FReader Reader;
	if (!Reader.Closed(Root, { TEXT("id"), TEXT("name"), TEXT("description"), TEXT("active"), TEXT("traits") }, TEXT("build")))
	{
		OutError = Reader.Error;
		return false;
	}
	OutBuild.Id = FReader::Name(Root, TEXT("id"));
	OutBuild.DisplayName = FReader::String(Root, TEXT("name"));
	OutBuild.Description = FReader::String(Root, TEXT("description"));
	for (const FValue& Trait : ListOf(Root->TryGetField(TEXT("traits"))))
	{
		OutBuild.Traits.Add(FName(*Trait->AsString()));
	}
	OutBuild.ActiveId = FReader::Name(Root, TEXT("active"));
	if (OutBuild.ActiveId.IsNone() && OutBuild.Traits.Num() > 0)
	{
		OutBuild.ActiveId = OutBuild.Traits[0];
	}
	if (OutBuild.Id.IsNone() || OutBuild.ActiveId.IsNone())
	{
		OutError = TEXT("build needs id and at least one trait");
		return false;
	}
	return true;
}

bool FDWRuleProgramReader::ReadTrait(const UDWTraitDefinition& Asset, FDWTraitSource& OutTrait, FString& OutError)
{
	OutTrait = FDWTraitSource();
	OutTrait.Id = Asset.TraitId.IsNone() ? Asset.GetFName() : Asset.TraitId;
	OutTrait.ParentId = Asset.ParentId;
	for (const TPair<FName, float>& Pair : Asset.Parameters)
	{
		OutTrait.Params.Add(Pair.Key, Pair.Value);
	}
	if (Asset.ProgramJson.TrimStartAndEnd().IsEmpty())
	{
		return true;
	}
	return ReadProgramText(Asset.ProgramJson, OutTrait, OutError);
}
