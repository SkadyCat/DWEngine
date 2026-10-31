#include "DWRuleWorld.h"

namespace DWRules
{
void FDWRuleWorld::ReleaseHeldRoot(int32 Index)
{
	if (IsRootValid(Index))
		Roots[Index].Live = FMath::Max(0, Roots[Index].Live - 1);
}
int32 FDWRuleWorld::TargetIndexOf(uint32 Id) const
{
	return TargetData.ExternalId.IndexOfByKey(Id);
}
FDWDamageResult FDWRuleWorld::HostHit(int32 Index, uint32 Id, float Amount, uint8 Element,
									  const FDWDamageInfo &Delivery, bool Silent, uint8 Generation, uint16 EventType,
									  const FDWEvent *Template)
{
	FDWDamageResult Result;
	const int32 Target = TargetIndexOf(Id);
	if (!IsRootValid(Index) || Target == INDEX_NONE || !(Amount > 0) || !IsHostile(Roots[Index].Owner, Target))
		return Result;
	FDWRoot &Root = Roots[Index];
	FDWDamageInfo Info = Delivery;
	Info.RootIndex = Index;
	FDWEvent Base;
	if (Template)
	{
		Base = *Template;
		Base.SourceIndex = INDEX_NONE;
		Base.SourceId = InvalidId;
	}
	Base.Root = Index;
	Base.Element = Element;
	Base.Tags = Info.Tags;
	Base.Generation = Template ? Template->Generation : Generation;
	Base.Position = Info.Position;
	Base.Position.Z = TargetData.Position[Target].Z;
	Result.Applied = ResolveDamage(Root, Target, Amount, Element, Root.NextSequence++, Result.bCritical, Result.bKilled,
								   Info, &Result.bMissed);
	if (!Silent && !Result.bMissed)
		PushHitEvents(Root, Index, Base, Target, Result.Applied, Amount, Result.bCritical, Result.bKilled, EventType);
	return Result;
}
float FDWRuleWorld::HostDot(int32 Owner, uint32 Id, float Amount, uint8 Element, float Scale)
{
	const int32 Target = TargetIndexOf(Id);
	if (Target == INDEX_NONE || !(Amount > 0) || TargetData.Health[Target] <= 0 || InvulnerableTargets.Contains(Id))
		return 0;
	const float Effective = EffectiveResistance(Target, Element, 0);
	const float Dealt = float(ApplyDamageTaken(Target, FMath::Max(0.f, Amount * (1.f - Effective / 100.f))) * Scale);
	FDWDamageRecord Row;
	Row.TargetExternalId = Id;
	Row.OwnerExternalId = GetOwnerExternalId(Owner);
	Row.Amount = FMath::Min(TargetData.Health[Target], Dealt);
	Row.Element = Element;
	Row.bDot = true;
	TargetData.Health[Target] -= Dealt;
	Row.bKilled = TargetData.Health[Target] <= 0;
	Damage.Add(Row);
	++FrameStats.Hits;
	FrameStats.Kills += Row.bKilled;
	return Row.Amount;
}
void FDWRuleWorld::MirrorStatus(uint32 Id, uint64 Key, float Value, int32 Stacks, float Remaining, int32 Owner)
{
	if (!(Remaining > 0) || !(Value > 0) || Stacks <= 0)
	{
		ExpireStatus(Id, Key);
		return;
	}
	FDWStatus &Status = Statuses.FindOrAdd(Mix64(Key ^ (uint64(Id) << 24)));
	const int32 *DefIndex = StatusDefByKey.Find(Key);
	const FDWStatusDef *Def = DefIndex ? &StatusDefs[*DefIndex] : nullptr;
	if ((Status.TargetId != Id || Status.Expire <= Clock) && Def && Def->TickInterval > 0)
		Status.NextTick = Clock + Def->TickInterval;
	Status.TargetId = Id;
	Status.Key = Key;
	Status.Value = Value;
	Status.Stacks = Stacks;
	Status.Expire = Clock + Remaining;
	Status.Owner = Owner;
}
void FDWRuleWorld::ExpireStatus(uint32 Id, uint64 Key)
{
	if (FDWStatus *Status = Statuses.Find(Mix64(Key ^ (uint64(Id) << 24))))
		if (Status->TargetId == Id)
			Status->Expire = FMath::Min(Status->Expire, Clock);
}
} // namespace DWRules
