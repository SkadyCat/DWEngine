#include "DWRuleCombat.h"
#include "DWRuleCombatTriggers.h"
#include "DWRuleMinions.h"
#include "DWRuleProjectileAffixes.h"

namespace DWRules
{
void FDWCombatRuntime::RuleEvent(FName Name,uint32 Target,double Amount)
{
    const int32 Plan=HeroPlan();if(!World.Plans.IsValidIndex(Plan))return;
    const int32 Event=World.GetPlan(Plan).FindEvent(Name);if(Event<0)return;
    const int32 Root=OpenRoot(Plan);if(Root<0)return;
    FDWEvent E;E.Root=Root;E.Type=uint16(Event);E.TargetId=Target;E.Amount=float(Amount);E.Sequence=World.Roots[Root].NextSequence++;
    E.Position=FVector3f(HeroPosition());FVector3d P;if(TargetPosition(Target,P))E.Position=FVector3f(P);
    E.Tags=TagBit(Tags::Root);World.PushEvent(E);World.ReleaseHeldRoot(Root);
}
void FDWCombatRuntime::DerivedRoot(int32 Root,int32 Source,bool Repeat)
{
    World.Roots[Root].DamageMultiplier=World.Roots[Source].DamageMultiplier;
    auto* From=RootInfo(Source);auto* To=RootInfo(Root,true);
    if(From){To->Kind=TEXT("cast");To->More=From->More;To->Increased=From->Increased;To->CriticalBonus=From->CriticalBonus;To->bTriggered=!Repeat&&From->bTriggered;To->bMinion=!Repeat&&From->bMinion;To->MinionId=!Repeat?From->MinionId:0;}
    To->bManual=false;if(Repeat)Projectiles->OnCast(Root,World.GetRoot(Root).Plan);
}
FDWSkillFamily FDWCombatRuntime::SkillFamily(int32 Index) const
{
	FDWSkillFamily F;
	Index = Index == INDEX_NONE ? Hero.Plan : Index;
	if (!World.Plans.IsValidIndex(Index))
		return F;
	const auto &P = World.GetPlan(Index);
	for (const auto &A : P.Actions)
	{
		F.bProjectile |= A.Op == EOp::Emit;
		F.bSummon |= A.Op == EOp::Summon;
		F.bFrost |= A.Element == 1;
		F.bFire |= A.Element == 0;
		F.bVoid |= A.Element == 5;
		F.bLightning |= A.Element == 2;
		F.bPoison |= A.Element == uint8(EElement::Poison);
	}
	for (const auto &Rule : P.Rules)
		if (Rule.Event == uint16(EBuiltinEvent::Cast) || Rule.Event == uint16(EBuiltinEvent::ProjectileEnd))
			for (int32 I = Rule.ActionFirst; I < Rule.ActionFirst + Rule.ActionCount; ++I)
				F.bArea |= P.Actions[I].Op == EOp::DamageArea;
	F.bFrost |= P.TagNames.Contains(TEXT("cold")) || P.TagNames.Contains(TEXT("frost"));
	F.bFire |= P.TagNames.Contains(TEXT("fire"));
	F.bVoid |= P.TagNames.Contains(TEXT("void"));
	F.bLightning |= P.TagNames.Contains(TEXT("lightning"));
	F.bPoison |= P.TagNames.Contains(TEXT("poison"));
	return F;
}
bool FDWCombatRuntime::IsAttack(int32 Index) const
{
	Index = Index == INDEX_NONE ? Hero.Plan : Index;
	return World.Plans.IsValidIndex(Index) && World.GetPlan(Index).TagNames.Contains(TEXT("melee"));
}
double FDWCombatRuntime::CooldownFor(int32 Index) const
{
	Index = Index == INDEX_NONE ? Hero.Plan : Index;
	if (!World.Plans.IsValidIndex(Index))
		return 0;
	double Base = 0;
	for (FName Name : World.GetPlan(Index).TraitIds)
	{
		FString Id = Name.ToString();
		int32 Suffix;
		if (Id.FindChar('~', Suffix))
			Id.LeftInline(Suffix);
		const auto *Entry = Host.Library.Traits.Find(FName(*Id));
		const FString Path = Entry ? Entry->Path.Replace(TEXT("\\"), TEXT("/")) : FString();
		if (Entry && Entry->Source.Rules.ContainsByPredicate([](const auto& R){return R.Event==TEXT("cast");}))
			Base = FMath::Max(Base, Entry->Source.Params.FindRef(TEXT("cooldown")));
	}
	const auto F = SkillFamily(Index);
	const double Recovery = (F.bArea ? S(TEXT("area.cooldown")) : 0) + (F.bFrost ? S(TEXT("frost.cooldown")) : 0) +
							(F.bVoid ? S(TEXT("void.cooldown")) : 0) + (F.bSummon ? S(TEXT("minion.cooldown")) : 0);
	return Base / FMath::Max(.1, 1 + Recovery / 100);
}
double FDWCombatRuntime::CostScale(int32 Index) const
{
	const auto F = SkillFamily(Index);
	double Reduction = 0;
	if (F.bArea)
		Reduction += S(TEXT("area.cost")) + FMath::Min(90., S(TEXT("area.repeats")) * S(TEXT("area.repeat_cost")));
	if (F.bProjectile)
		Reduction += S(TEXT("proj.cost"));
	if (F.bSummon)
		Reduction += S(TEXT("minion.cost"));
	if (F.bVoid)
		Reduction += S(TEXT("void.cost"));
	return FMath::Max(.1, 1 - FMath::Min(90., Reduction) / 100);
}
double FDWCombatRuntime::CastSpeed(int32 Index) const
{
	const auto F = SkillFamily(Index);
	return (F.bArea ? S(TEXT("area.cast_speed")) : 0) + (F.bLightning ? S(TEXT("lightning.cast_speed")) : 0) +
		   (F.bSummon ? S(TEXT("minion.summon_speed")) : 0);
}
bool FDWCombatRuntime::PlanSource(FDWTraitSource &Out,bool Minion,bool ProjectileOnly) const
{
	Out = {};
	Out.Id = TEXT("__combat_plan");
	auto Add = [&](const TCHAR *Stat, EModOp Op, double Value, const TCHAR *Key) {
		if (FMath::Abs(Value) <= 1.e-9)
			return;
		Out.Params.Add(Key, FMath::FloorToDouble(Value * 1000 + .5) / 1000);
		FDWModifierSource M;
		M.Stat = Stat;
		M.Op = Op;
		M.Value = FDWExprSource::Param(Key);
		Out.Modifiers.Add(M);
	};
	Add(TEXT("projectile.count"), EModOp::Add, S(TEXT("proj.count"))+(Minion?S(TEXT("minion.proj_count")):0), TEXT("count"));
	Add(TEXT("projectile.speed"), EModOp::IncreasedPercent, S(TEXT("proj.speed"))+(Minion?S(TEXT("minion.proj_speed")):0), TEXT("speed"));
	Add(TEXT("projectile.lifetime"), EModOp::IncreasedPercent, S(TEXT("proj.lifetime")) + S(TEXT("proj.distance")),
		TEXT("life"));
	Add(TEXT("projectile.radius"), EModOp::IncreasedPercent, S(TEXT("proj.radius")), TEXT("radius"));
	Add(TEXT("projectile.pierce"), EModOp::Add, S(TEXT("proj.pierce")), TEXT("pierce"));
	Add(TEXT("projectile.spread"), EModOp::Add, S(TEXT("proj.spread_more")) - S(TEXT("proj.spread_less")),
		TEXT("spread"));
	if(!ProjectileOnly){
    Add(TEXT("area.radius"), EModOp::Add, S(TEXT("area.base_radius")), TEXT("area_base"));
	Add(TEXT("area.radius"), EModOp::IncreasedPercent, (AreaScale() - 1) * 100, TEXT("area"));}
	return Out.Modifiers.Num() > 0;
}
void FDWCombatRuntime::OnHeroCast(int32 Root, const FDWCastPayment &Payment)
{
	if (!World.IsRootValid(Root))
		return;
	const int32 Plan = World.GetRoot(Root).Plan;
	auto *Info = RootInfo(Root, true);
	Info->Kind = TEXT("cast");
	Info->bManual = true;
	Info->Skill = Hero.Skill;
	Info->CriticalBonus = Payment.CritBonus;
	Cooldown = CooldownFor(Plan);
    SkillReadyAt.Add(Hero.Skill,Time+Cooldown);
	Projectiles->OnCast(Root, Plan);
	Minions->OnHeroCast();
	Triggers->OnCast(IsAttack(Plan), Payment);
}
} // namespace DWRules
