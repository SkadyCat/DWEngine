#include "DWRuleCompiler.h"

namespace DWRules
{
	namespace
	{
		const TCHAR* const SlotNames[int32(ESlot::Num)] = {
			TEXT("count"),
			TEXT("spread"),
			TEXT("speed"),
			TEXT("lifetime"),
			TEXT("damage"),
			TEXT("radius"),
			TEXT("amount"),
			TEXT("value"),
			TEXT("cap"),
			TEXT("threshold"),
			TEXT("pierce"),
			TEXT("duration"),
			TEXT("interval"),
            TEXT("arc_height"), TEXT("decay_interval"), TEXT("rate"), TEXT("offset_forward"), TEXT("offset_up"), TEXT("scatter_radius")
		};

		struct FCompiler
		{
			FDWCompiledPlan& Plan;
			const FDWCompileOptions& Options;
			const FDWTraitSource* Trait = nullptr;
			uint16 TraitIndex = 0;
			FString Error;

			FCompiler(FDWCompiledPlan& InPlan, const FDWCompileOptions& InOptions)
				: Plan(InPlan)
				, Options(InOptions)
			{
			}

			bool Fail(const FString& Message)
			{
				if (Error.IsEmpty())
				{
					Error = Trait ? FString::Printf(TEXT("%s: %s"), *Trait->Id.ToString(), *Message) : Message;
				}
				return false;
			}

			uint16 EventId(FName Name)
			{
				const int32 Found = Plan.EventNames.IndexOfByKey(Name);
				if (Found != INDEX_NONE)
				{
					return uint16(Found);
				}
				return uint16(Plan.EventNames.Add(Name));
			}

			uint16 KindId(FName Name)
			{
				const int32 Found = Plan.KindNames.IndexOfByKey(Name);
				if (Found != INDEX_NONE)
				{
					return uint16(Found);
				}
				return uint16(Plan.KindNames.Add(Name));
			}

			bool TagBitFor(FName Name, uint64& OutBit)
			{
				int32 Found = Plan.TagNames.IndexOfByKey(Name);
				if (Found == INDEX_NONE)
				{
					if (Plan.TagNames.Num() >= MaxTags)
					{
						return Fail(TEXT("more than 64 distinct tags in one build"));
					}
					Found = Plan.TagNames.Add(Name);
				}
				OutBit = TagBit(uint32(Found));
				return true;
			}

			bool TagMask(const TArray<FName>& Names, uint64& OutMask)
			{
				for (FName Name : Names)
				{
					uint64 Bit = 0;
					if (!TagBitFor(Name, Bit))
					{
						return false;
					}
					OutMask |= Bit;
				}
				return true;
			}

			bool ChannelId(FName Name, int8& OutChannel)
			{
				OutChannel = -1;
				if (Name.IsNone())
				{
					return true;
				}
				int32 Found = Plan.ChannelNames.IndexOfByKey(Name);
				if (Found == INDEX_NONE)
				{
					if (Plan.ChannelNames.Num() >= MaxChannels)
					{
						return Fail(TEXT("more than 4 chain channels in one build"));
					}
					Found = Plan.ChannelNames.Add(Name);
				}
				OutChannel = int8(Found);
				return true;
			}

			// Without a channel, state is isolated per trait; a channel shares it across traits.
			uint64 StateKey(FName Key, FName Channel) const
			{
				const FString Space = Channel.IsNone()
					? FString::Printf(TEXT("affix:%s"), *Trait->Id.ToString())
					: FString::Printf(TEXT("channel:%s"), *Channel.ToString());
				return HashName(Space + TEXT(":") + Key.ToString());
			}

			bool EmitExpr(const FDWExprSource& Source, TArray<FDWExprInstr>& Out, bool& bConst, int32& Depth, int32& MaxDepth)
			{
				FDWExprInstr In;
				switch (Source.Kind)
				{
				case FDWExprSource::EKind::Const:
					In.Value = float(Source.Const);
					break;
				case FDWExprSource::EKind::Param:
				{
					const double* Value = Trait->Params.Find(Source.Name);
					if (!Value)
					{
						return Fail(FString::Printf(TEXT("unbound parameter %s"), *Source.Name.ToString()));
					}
					In.Value = float(*Value);
					break;
				}
				case FDWExprSource::EKind::Stat:
				{
					uint16 Stat = 0;
					if (!FindStat(Source.Name.ToString(), Stat))
					{
						return Fail(FString::Printf(TEXT("unknown stat %s"), *Source.Name.ToString()));
					}
					// Modifiers are aggregated before rules compile, so stat reads fold to constants.
					In.Value = Plan.Stats.Get(Stat);
					break;
				}
				case FDWExprSource::EKind::Event:
				{
					const FString Field = Source.Name.ToString();
					bConst = false;
					if (Field == TEXT("amount")) In.Op = EExprOp::EventAmount;
					else if (Field == TEXT("damage")) In.Op = EExprOp::EventDamage;
					else if (Field == TEXT("stored_value")) In.Op = EExprOp::EventStored;
					else if (Field == TEXT("generation")) In.Op = EExprOp::EventGeneration;
					else if (Field == TEXT("sequence")) In.Op = EExprOp::EventSequence;
					else return Fail(FString::Printf(TEXT("unsupported event value %s"), *Field));
					break;
				}
				case FDWExprSource::EKind::State:
					bConst = false;
					In.Op = EExprOp::State;
					In.Scope = Source.Scope;
					In.Key = StateKey(Source.Name, Source.Channel);
					break;
				default:
				{
					if (Source.Args.Num() == 0 || Source.Args.Num() > 255)
					{
						return Fail(TEXT("arithmetic expression needs 1..255 terms"));
					}
					if (Source.Kind == FDWExprSource::EKind::Pow && Source.Args.Num() != 2)
					{
						return Fail(TEXT("pow expression needs exactly 2 terms"));
					}
					for (const FDWExprSource& Arg : Source.Args)
					{
						if (!EmitExpr(Arg, Out, bConst, Depth, MaxDepth))
						{
							return false;
						}
					}
					Depth -= Source.Args.Num();
					In.Arity = uint8(Source.Args.Num());
					switch (Source.Kind)
					{
					case FDWExprSource::EKind::Mul: In.Op = EExprOp::Mul; break;
					case FDWExprSource::EKind::Add: In.Op = EExprOp::Add; break;
					case FDWExprSource::EKind::Div: In.Op = EExprOp::Div; break;
					case FDWExprSource::EKind::Min: In.Op = EExprOp::Min; break;
					case FDWExprSource::EKind::Pow: In.Op = EExprOp::Pow; break;
					default: In.Op = EExprOp::Max; break;
					}
					break;
				}
				}
				Out.Add(In);
				MaxDepth = FMath::Max(MaxDepth, ++Depth);
				return true;
			}

			bool CompileExpr(const FDWExprSource& Source, FDWExprRef& Out)
			{
				TArray<FDWExprInstr> Code;
				bool bConst = true;
				int32 Depth = 0;
				int32 MaxDepth = 0;
				if (!EmitExpr(Source, Code, bConst, Depth, MaxDepth))
				{
					return false;
				}
				if (MaxDepth > MaxExprStack)
				{
					return Fail(TEXT("expression too deep"));
				}
				Out.bSet = true;
				if (bConst)
				{
					Out.Const = EvaluateExpr(Code.GetData(), Code.Num(), [](const FDWExprInstr&) { return 0.f; });
					Out.Count = 0;
					return true;
				}
				Out.First = Plan.Expr.Num();
				Out.Count = uint16(Code.Num());
				Plan.Expr.Append(Code);
				return true;
			}

			bool CompileConstant(const FDWExprSource& Source, float& Out)
			{
				FDWExprRef Ref;
				if (!CompileExpr(Source, Ref))
				{
					return false;
				}
				if (Ref.Count != 0)
				{
					return Fail(TEXT("modifier values must be constant (dynamic modifiers are not supported yet)"));
				}
				Out = Ref.Const;
				return true;
			}

			bool CompileModifiers()
			{
				for (const FDWModifierSource& Modifier : Trait->Modifiers)
				{
					uint16 Stat = 0;
					if (!FindStat(Modifier.Stat, Stat))
					{
						return Fail(FString::Printf(TEXT("unknown modifier stat %s"), *Modifier.Stat));
					}
					float Value = 0.f;
					if (!CompileConstant(Modifier.Value, Value))
					{
						return false;
					}
					Plan.Stats.Apply(Stat, Modifier.Op, Value);
				}
				return true;
			}

			bool CompileCondition(const FDWConditionSource& Source, FDWRuleDef& Rule)
			{
				const FString Op = Source.Op.ToString();
				if (Op == TEXT("tag"))
				{
					uint64 Bit = 0;
					if (!TagBitFor(Source.Tag, Bit))
					{
						return false;
					}
					Rule.RequireAll |= Bit;
					return true;
				}
				FDWConditionDef Def;
				Def.Compare = Source.Compare;
				Def.Scope = Source.Scope;
				if (Op == TEXT("generation"))
				{
					Def.Kind = ECond::Generation;
					Def.Compare = ECompare::Lte;
				}
				else if (Op == TEXT("state"))
				{
					Def.Kind = ECond::State;
					Def.Key = StateKey(Source.Key, Source.Channel);
				}
				else if (Op == TEXT("chance"))
				{
					Def.Kind = ECond::Chance;
				}
				else if (Op == TEXT("status"))
				{
					if (Source.Key.IsNone())
					{
						return Fail(TEXT("status condition needs key"));
					}
					Def.Kind = ECond::Status;
					Def.Key = StatusKey(Source.Key.ToString());
					// Bare {op=status,key=chill} means "has the status".
					if (Source.Value.Kind == FDWExprSource::EKind::Const && Source.Value.Const == 0.0 && Source.Compare == ECompare::Gte)
					{
						Def.Compare = ECompare::Gt;
					}
				}
                else if (Op == TEXT("active"))
                {
                    Def.Kind = ECond::Active; Def.Active = Source.Key;
                }
				else if (Op == TEXT("entity_count"))
				{
					Def.Kind = ECond::EntityCount;
					Def.EntityKind = Source.Kind.IsNone() ? MAX_uint16 : KindId(Source.Kind);
				}
				else
				{
					return Fail(FString::Printf(TEXT("unsupported condition %s"), *Op));
				}
				if (!CompileExpr(Source.Value, Def.Value))
				{
					return false;
				}
				Plan.Conditions.Add(Def);
				++Rule.ConditionCount;
				return true;
			}

			bool Require(const FDWActionSource& Source, std::initializer_list<const TCHAR*> Fields)
			{
				for (const TCHAR* Field : Fields)
				{
					if (!Source.Numbers.Contains(FName(Field)))
					{
						return Fail(FString::Printf(TEXT("action missing %s"), Field));
					}
				}
				return true;
			}

			bool ValidateAction(const FDWActionSource& Source)
			{
				switch (Source.Op)
				{
				case EOp::Emit:
					return Source.Kind.IsNone() ? Fail(TEXT("emit needs kind")) : true;
				case EOp::DamageArea:
					return Require(Source, { TEXT("radius"), TEXT("amount") });
				case EOp::DamageTarget:
					return Require(Source, { TEXT("amount") });
				case EOp::Counter:
				case EOp::Accumulate:
					return Source.Key.IsNone() ? Fail(TEXT("state action needs key")) : Require(Source, { TEXT("value") });
                case EOp::ConsumeStatus:
				case EOp::Consume:
					if (Source.Key.IsNone())
					{
						return Fail(TEXT("consume needs key"));
					}
					return Source.bConsumeAll ? true : Require(Source, { TEXT("value") });
				case EOp::EmitEvent:
					return Source.Event.IsNone() ? Fail(TEXT("emit_event needs event")) : true;
				case EOp::ApplyStatus:
					return Source.Key.IsNone() ? Fail(TEXT("apply_status needs key")) : Require(Source, { TEXT("value"), TEXT("duration") });
				case EOp::GrantTraits:
					return Source.bHasGrant ? Require(Source, { TEXT("duration") }) : Fail(TEXT("grant_traits needs traits"));
                case EOp::Storm:
                case EOp::Summon:
					return Source.Kind.IsNone() || Source.Event.IsNone() ? Fail(TEXT("summon needs kind and event")) : Require(Source, { TEXT("duration"), TEXT("interval"), TEXT("radius") });
				default:
					return true;
				}
			}

			bool CompileAction(const FDWActionSource& Source, int32 Index, int32 Depth)
			{
				if (!ValidateAction(Source))
				{
					return false;
				}
				FDWActionDef Def;
				Def.Op = Source.Op;
                Def.bAimTarget=Source.bAimTarget; Def.bHemisphere=Source.bHemisphere;
                Def.bFeed=Source.bFeed; Def.bInvulnerable=Source.bInvulnerable; Def.Targeting=Source.Targeting;
                if(Source.Op==EOp::Sacrifice)Def.Kind=MAX_uint16;
				Def.Layout = Source.Layout;
				Def.Origin = Source.Origin;
				Def.Scope = Source.Scope;
				Def.Mode = Source.Mode;
				Def.Element = Source.Element;
				Def.Flags = uint8((Source.bInherit ? uint8(FDWActionDef::Inherit) : uint8(0)) | (Source.bConsumeAll ? uint8(FDWActionDef::ConsumeAll) : uint8(0)) | (Source.bParabolic ? uint8(FDWActionDef::Parabolic) : uint8(0)));
				if (!Source.Kind.IsNone())
				{
					Def.Kind = KindId(Source.Kind);
				}
				if (!Source.Event.IsNone())
				{
					Def.Event = EventId(Source.Event);
				}
				else if (!Source.EmitEvent.IsNone())
				{
					Def.Event = EventId(Source.EmitEvent);
				}
				else
				{
					Def.Event = uint16(EBuiltinEvent::Hit);
				}
				Def.EndEvent = Source.EndEvent.IsNone() ? MAX_uint16 : EventId(Source.EndEvent);
				if (Source.Op == EOp::GrantTraits)
				{
					FDWGrantDef Grant;
					TArray<FString> Contents;
					if (Source.bGrantStitched)
					{
						Grant.Sources = Trait->Stitched;
						for (const FDWTraitSource& Child : Grant.Sources) Contents.Add(TraitSourceKey(Child));
					}
					else
					{
						Grant.Entries = Source.GrantEntries;
						for (const FDWTraitEntry& Entry : Grant.Entries)
						{
							FString Text = Entry.Id.ToString();
							for (const auto& P : Entry.Params) Text += FString::Printf(TEXT("|%s=%.9g"), *P.Key.ToString(), P.Value);
							Contents.Add(Text);
						}
					}
					FString Wrapper = Trait->Id.ToString();
					int32 Suffix;
					if (Wrapper.FindChar(TEXT('~'), Suffix)) Wrapper.LeftInline(Suffix);
					Grant.Key = (Wrapper + TEXT("(") + FString::Join(Contents, TEXT(",")) + TEXT(")")).ToLower();
					Def.Grant = Plan.Grants.Add(MoveTemp(Grant));
				}
				if (!Source.Key.IsNone())
				{
					Def.StateKey = (Source.Op == EOp::ApplyStatus || Source.Op == EOp::ConsumeStatus) ? StatusKey(Source.Key.ToString()) : StateKey(Source.Key, Source.Channel);
				}
				if (!TagMask(Source.Tags, Def.AddTags))
				{
					return false;
				}
				for (const TPair<FName, FDWExprSource>& Pair : Source.Numbers)
				{
					ESlot Slot;
					if (!FindSlot(Pair.Key, Slot))
					{
						return Fail(FString::Printf(TEXT("unknown numeric field %s"), *Pair.Key.ToString()));
					}
					if (!CompileExpr(Pair.Value, Def.Slots[int32(Slot)]))
					{
						return false;
					}
				}
				Plan.Actions[Index] = Def;
				if (Source.Then.Num() > 0)
				{
					int32 First = 0;
					if (!CompileActionList(Source.Then, Depth + 1, First))
					{
						return false;
					}
					Plan.Actions[Index].ThenFirst = uint16(First);
					Plan.Actions[Index].ThenCount = uint16(Source.Then.Num());
				}
				return true;
			}

			// Siblings are contiguous; children are appended after their parent list.
			bool CompileActionList(const TArray<FDWActionSource>& List, int32 Depth, int32& OutFirst)
			{
				if (Depth > MaxActionDepth)
				{
					return Fail(TEXT("action nesting exceeds 12"));
				}
				OutFirst = Plan.Actions.Num();
				if (OutFirst + List.Num() > MAX_uint16)
				{
					return Fail(TEXT("too many actions"));
				}
				Plan.Actions.AddDefaulted(List.Num());
				for (int32 Index = 0; Index < List.Num(); ++Index)
				{
					if (!CompileAction(List[Index], OutFirst + Index, Depth))
					{
						return false;
					}
				}
				return true;
			}

			bool CompileRule(const FDWRuleSource& Source, TArray<FDWRuleDef>& OutRules)
			{
				if (Source.Event.IsNone())
				{
					return Fail(FString::Printf(TEXT("rule %s has no event"), *Source.Id.ToString()));
				}
				FDWRuleDef Rule;
				Rule.Id = Source.Id;
				Rule.Trait = TraitIndex;
				Rule.Event = EventId(Source.Event);
				Rule.MaxDepth = uint8(FMath::Clamp(Source.MaxDepth, 0, 32));
				const int32 MaxEvents = Source.MaxEvents > 0 ? Source.MaxEvents : Options.DefaultMaxEvents;
				Rule.MaxEvents = uint16(FMath::Clamp(MaxEvents, 1, 16384));
				if (!ChannelId(Source.Channel, Rule.Channel))
				{
					return false;
				}
				Rule.ConditionFirst = uint16(Plan.Conditions.Num());
				for (const FDWConditionSource& Condition : Source.Conditions)
				{
					if (!CompileCondition(Condition, Rule))
					{
						return false;
					}
				}
				int32 First = 0;
				if (!CompileActionList(Source.Actions, 0, First))
				{
					return false;
				}
				Rule.ActionFirst = uint16(First);
				Rule.ActionCount = uint16(Source.Actions.Num());
				OutRules.Add(Rule);
				return true;
			}
		};

		TArray<const FDWTraitSource*> ComposeTraits(const FDWBuildSource& Build, FString& OutError)
		{
			TMap<FName, const FDWTraitSource*> ById;
			for (const FDWTraitSource& Trait : Build.Traits)
			{
				if (Trait.Id.IsNone() || ById.Contains(Trait.Id))
				{
					OutError = FString::Printf(TEXT("missing or duplicate trait id %s"), *Trait.Id.ToString());
					return {};
				}
				ById.Add(Trait.Id, &Trait);
			}
			if (!ById.Contains(Build.ActiveId))
			{
				OutError = FString::Printf(TEXT("active %s is not part of the build"), *Build.ActiveId.ToString());
				return {};
			}
			// Fixed point: a source is enabled only while its parent is enabled.
			TSet<FName> Enabled;
			Enabled.Add(Build.ActiveId);
			bool bChanged = true;
			while (bChanged)
			{
				bChanged = false;
				for (const FDWTraitSource& Trait : Build.Traits)
				{
					const bool bParentOk = Trait.ParentId.IsNone() || Enabled.Contains(Trait.ParentId);
					if (bParentOk && !Enabled.Contains(Trait.Id))
					{
						Enabled.Add(Trait.Id);
						bChanged = true;
					}
				}
			}
			TArray<const FDWTraitSource*> Result;
			for (const FDWTraitSource& Trait : Build.Traits)
			{
				if (Enabled.Contains(Trait.Id))
				{
					Result.Add(&Trait);
				}
			}
			return Result;
		}

		uint64 HashBuild(const TArray<const FDWTraitSource*>& Traits)
		{
			FString Text;
			for (const FDWTraitSource* Trait : Traits)
			{
				Text += TraitSourceKey(*Trait);
				Text += FString::Printf(TEXT("|m%d|r%d;"), Trait->Modifiers.Num(), Trait->Rules.Num());
			}
			return HashName(Text);
		}
	}

	bool FindSlot(FName Field, ESlot& Out)
	{
		const FString Text = Field.ToString();
		for (int32 Index = 0; Index < int32(ESlot::Num); ++Index)
		{
			if (Text.Equals(SlotNames[Index], ESearchCase::IgnoreCase))
			{
				Out = ESlot(Index);
				return true;
			}
		}
		return false;
	}

	TSharedPtr<FDWCompiledPlan> FDWRuleCompiler::Compile(const FDWBuildSource& Build, FString& OutError,
		const FDWCompileOptions& Options)
	{
		OutError.Reset();
		const TArray<const FDWTraitSource*> Traits = ComposeTraits(Build, OutError);
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}
		TSharedPtr<FDWCompiledPlan> Plan = MakeShared<FDWCompiledPlan>();
        Plan->ActiveId=Build.ActiveId;
		for (int32 Index = 0; Index < int32(EBuiltinEvent::Num); ++Index)
		{
			Plan->EventNames.Add(FName(BuiltinEventName(EBuiltinEvent(Index))));
		}
		for (uint32 Index = 0; Index < Tags::NumBuiltin; ++Index)
		{
			Plan->TagNames.Add(FName(BuiltinTagName(Index)));
		}
		Plan->Hash = HashBuild(Traits);
		FCompiler Compiler(*Plan, Options);

		// Pass 1: every modifier from every enabled source, then resolve stats once.
		for (int32 Index = 0; Index < Traits.Num(); ++Index)
		{
			Compiler.Trait = Traits[Index];
			Compiler.TraitIndex = uint16(Index);
			Plan->TraitIds.Add(Traits[Index]->Id);
			if (!Compiler.CompileModifiers())
			{
				OutError = Compiler.Error;
				return nullptr;
			}
		}
		Plan->Stats.Finalize();

		// Pass 2: rules; stat reads fold against the finalized block.
		TArray<FDWRuleDef> Unsorted;
		for (int32 Index = 0; Index < Traits.Num(); ++Index)
		{
			Compiler.Trait = Traits[Index];
			Compiler.TraitIndex = uint16(Index);
			for (const FDWRuleSource& Rule : Traits[Index]->Rules)
			{
				if (!Compiler.CompileRule(Rule, Unsorted))
				{
					OutError = Compiler.Error;
					return nullptr;
				}
			}
		}
		if (Unsorted.Num() > MAX_uint16)
		{
			OutError = TEXT("too many rules");
			return nullptr;
		}
		Unsorted.StableSort([](const FDWRuleDef& A, const FDWRuleDef& B) { return A.Event < B.Event; });
		Plan->Rules = MoveTemp(Unsorted);
		Plan->EventRuleFirst.SetNumZeroed(Plan->EventNames.Num());
		Plan->EventRuleCount.SetNumZeroed(Plan->EventNames.Num());
		for (int32 Index = Plan->Rules.Num() - 1; Index >= 0; --Index)
		{
			const uint16 Event = Plan->Rules[Index].Event;
			Plan->EventRuleFirst[Event] = uint16(Index);
			++Plan->EventRuleCount[Event];
		}
		return Plan;
	}
}
