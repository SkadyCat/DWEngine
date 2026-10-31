#include "DWRuleWorld.h"

namespace DWRules
{
void FDWRuleWorld::LiveStatusSnapshot(uint32 Target,TArray<FDWStatus>& Out) const
{
    Out.Reset();for(const auto& Pair:Statuses)if(Pair.Value.TargetId==Target&&Pair.Value.Expire>Clock&&Pair.Value.Stacks>0)Out.Add(Pair.Value);
}
void FDWRuleWorld::MaintainedSnapshot(int32 Owner,TArray<FDWMaintainedView>& Out) const
{
    Out.Reset();for(const auto& E:MaintainedEmitters)if(E.Owner==Owner){const int32 Stacks=FMath::FloorToInt(MaintainedLayers(Owner,E.StateKey));if(Stacks>0)Out.Add({E.StateKey,E.Root,Stacks,E.Cap,E.DecayAt,E.DecayInterval});}
}
bool FDWRuleWorld::RunConsumeStatus(const FDWActionDef& Action,FContext& C,int32 Depth)
{
    const uint32 Id=Action.Origin==EOrigin::Owner?Owners[C.Root.Owner].StatusId:C.Event.TargetId;
    const uint64 MapKey=Mix64(Action.StateKey^(uint64(Id)<<24));
    FDWStatus* Status=Statuses.Find(MapKey);
    if(!Status||Status->Expire<=Clock||Status->Stacks<=0)return false;
    const int32 Need=(Action.Flags&FDWActionDef::ConsumeAll)?Status->Stacks:FMath::FloorToInt(SlotOr(Action,ESlot::Value,C,0));
    if(Need<=0||Status->Stacks<Need)return false;
    Status->Stacks-=Need;if(!Status->Stacks)Statuses.Remove(MapKey);
    if(Extension)Extension->StatusConsumed(Id,Action.StateKey,Need);
    const float Saved=C.Stored;C.Stored=float(Need);
    const bool Result=RunActions(Action.ThenFirst,Action.ThenCount,C,Depth+1);C.Stored=Saved;
    return Result;
}

bool FDWRuleWorld::RunRepeatSkill(FContext& C)
{
    // A repeat has an independent normal hit batch, no manual tag or extra payment.
    const int32 Root=OpenRoot(C.Root.Plan,C.Root.Owner,Mix64(C.Root.Seed^uint64(C.Root.NextSequence++)^0xEC401));
    if(Root==INDEX_NONE)return false;
    Roots[Root].TargetPoint=C.Root.TargetPoint;
    FDWEvent E=C.Event;E.Type=uint16(EBuiltinEvent::Cast);E.Root=Root;E.Sequence=Roots[Root].NextSequence++;
    E.Generation=0;E.Lineage=0;E.SourceId=InvalidId;E.SourceIndex=INDEX_NONE;
    E.Tags=(E.Tags&~NonInheritedTags)|TagBit(Tags::Root);
    if(Extension)Extension->DerivedRoot(Root,C.RootIndex,true);
    PushEvent(E);ReleaseHeldRoot(Root);return true;
}

float FDWRuleWorld::MaintainedLayers(int32 Owner,uint64 Key) const
{
    return OwnerState.FindRef(Mix64(Key^(uint64(Owner)<<40)));
}

bool FDWRuleWorld::RunMaintainEmitter(const FDWActionDef& A,FContext& C)
{
    if(A.Scope!=EScope::Owner)return false;
    auto* Existing=MaintainedEmitters.FindByPredicate([&](const auto& E){return E.StateKey==A.StateKey&&E.Owner==C.Root.Owner;});
    const double Value=SlotOr(A,ESlot::Value,C,0);
    if(!FMath::IsFinite(Value)||Value<=0)return false;
    if(A.bFeed)
    {
        if(!Existing||ReadState(EScope::Owner,A.StateKey,C)<1)return false;
        Existing->Level=FMath::Min(Existing->Cap,FMath::FloorToInt(ReadState(EScope::Owner,A.StateKey,C))+FMath::Max(1,FMath::FloorToInt(Value)));
        Existing->DecayAt=Clock+Existing->Duration;WriteState(EScope::Owner,A.StateKey,C,float(Existing->Level));return true;
    }
    const double Cap=SlotOr(A,ESlot::Cap,C,0),Duration=SlotOr(A,ESlot::Duration,C,0),Decay=SlotOr(A,ESlot::DecayInterval,C,0),Interval=SlotOr(A,ESlot::Interval,C,0),Radius=SlotOr(A,ESlot::Radius,C,24);
    if(!FMath::IsFinite(Cap)||!FMath::IsFinite(Duration)||!FMath::IsFinite(Decay)||!FMath::IsFinite(Radius)||Cap<1||Duration<0||Decay<=0||Radius<0||(!A.Slot(ESlot::Rate).bSet&&Interval<=0))return false;
    if(!Existing&&MaintainedEmitters.Num()>=128){++FrameStats.CapacityDrops;return false;}
    const bool New=!Existing;const int32 Previous=Existing?Existing->Root:INDEX_NONE;
    FMaintainedEmitter& E=Existing?*Existing:MaintainedEmitters.AddDefaulted_GetRef();
    const bool WasRandom=E.Rate.bSet;
    ++C.Root.Live;E.Root=C.RootIndex;E.Owner=C.Root.Owner;E.StateKey=A.StateKey;E.Cap=FMath::Clamp(FMath::FloorToInt(Cap),1,512);
    E.Level=FMath::Min(E.Cap,FMath::Max(0,FMath::FloorToInt(ReadState(EScope::Owner,A.StateKey,C)))+FMath::Max(1,FMath::FloorToInt(Value)));
    E.Duration=Duration;E.DecayInterval=FMath::Max(.05,Decay);E.DecayAt=Clock+Duration;E.Radius=FMath::Min(1000.,Radius);
    E.Event=A.Event;E.Targeting=A.Targeting;E.Power=C.Event.Amount;E.Tags=(C.Event.Tags&~NonInheritedTags)|A.AddTags;E.Seed=C.Root.Seed;
    E.Interval=FMath::Max(.05,Interval);E.Rate=A.Slot(ESlot::Rate);
    if(New){E.RateAt=Clock;E.NextFire=Clock+E.Interval;E.ScheduleSeed=C.Root.Seed;E.Hazard=-FMath::Loge(FMath::Max(1.e-7,1.-double(Random01(E.ScheduleSeed,++E.Sample,0x504F4953))));}
    if(!WasRandom&&E.Rate.bSet)E.RateAt=Clock;
    if(WasRandom&&!E.Rate.bSet)E.NextFire=Clock+E.Interval;
    WriteState(EScope::Owner,A.StateKey,C,float(E.Level));
    if(Previous!=INDEX_NONE)ReleaseHeldRoot(Previous);
    return true;
}

void FDWRuleWorld::TickMaintainedEmitters()
{
    for(int32 I=MaintainedEmitters.Num()-1;I>=0;--I)
    {
        auto& E=MaintainedEmitters[I];const auto* Owner=GetOwner(E.Owner);
        const int32 Body=Owner?TargetIndexOf(Owner->StatusId):INDEX_NONE;
        const bool Valid=Owner&&IsRootValid(E.Root)&&(Body==INDEX_NONE||TargetData.Health[Body]>0);
        if(Valid)E.Level=FMath::Clamp(FMath::FloorToInt(MaintainedLayers(E.Owner,E.StateKey)),0,E.Cap);else E.Level=0;
        auto Save=[&](){const uint64 Key=Mix64(E.StateKey^(uint64(E.Owner)<<40));OwnerState.Add(Key,float(E.Level));OwnerStateKeys.FindOrAdd(E.Owner).Add(Key);};
        auto Fire=[&]()
        {
            const int32 Root=OpenRoot(Roots[E.Root].Plan,E.Owner,Mix64(E.Seed^uint64(++E.Pulse)));if(Root==INDEX_NONE)return;
            int32 Target=INDEX_NONE;TArray<int32> Candidates;double Best=E.Radius*E.Radius;
            if(E.Targeting!=TEXT("aim"))for(int32 K=0;K<TargetData.Num();++K)
            {
                if(TargetData.Health[K]<=0||!IsHostile(E.Owner,K))continue;
                const double D=FVector2f::DistSquared(FVector2f(TargetData.Position[K].X,TargetData.Position[K].Y),FVector2f(Owner->Position.X,Owner->Position.Y));
                if(D>E.Radius*E.Radius)continue;
                if(E.Targeting==TEXT("random"))Candidates.Add(K);
                else if(D<Best||(D==Best&&(Target==INDEX_NONE||TargetData.ExternalId[K]<TargetData.ExternalId[Target]))){Best=D;Target=K;}
            }
            if(Candidates.Num())Target=Candidates[FMath::Min(Candidates.Num()-1,FMath::FloorToInt(Random01(E.Seed,E.Pulse,0xE117)*Candidates.Num()))];
            FDWEvent Event;Event.Root=Root;Event.Type=E.Event;Event.Sequence=Roots[Root].NextSequence++;Event.Element=3;
            Event.Position=Owner->Position;Event.Direction=Target!=INDEX_NONE?(TargetData.Position[Target]-Owner->Position).GetSafeNormal():Owner->Aim;
            Event.TargetIndex=Target;Event.TargetId=Target==INDEX_NONE?InvalidId:TargetData.ExternalId[Target];
            Event.Amount=float(E.Level);Event.Stored=float(E.Level);Event.Damage=float(E.Power);Event.Tags=E.Tags;
            if(Extension)Extension->DerivedRoot(Root,E.Root,false);
            PushEvent(Event);ReleaseHeldRoot(Root);
        };
        auto Rate=[&](){FDWEvent Event;Event.Amount=float(E.Level);Event.Damage=float(E.Power);Event.Sequence=E.Pulse;
            FContext C{Event,*Plans[Roots[E.Root].Plan],Roots[E.Root],E.Root,-1,float(E.Level),NAME_None};const double R=Eval(E.Rate,C);return FMath::IsFinite(R)?FMath::Clamp(R,0.,1024.):0.;};
        if(E.Level&&E.Rate.bSet)
        {
            double From=E.RateAt;int32 Work=0;
            while(E.Level&&From<Clock)
            {
                const double Until=FMath::Min(double(Clock),E.DecayAt);double Budget=Rate()*FMath::Max(0.,Until-From);
                while(Budget>=E.Hazard&&Work<64){Budget-=E.Hazard;Fire();++Work;E.Hazard=-FMath::Loge(FMath::Max(1.e-7,1.-double(Random01(E.ScheduleSeed,++E.Sample,0x504F4953))));}
                if(Work==64&&Budget>=E.Hazard){++FrameStats.CapacityDrops;Budget=0;}
                E.Hazard-=Budget;From=Until;
                if(From+1.e-5>=E.DecayAt){--E.Level;E.DecayAt+=E.DecayInterval;Save();}
            }
            E.RateAt=Clock;
        }
        else if(E.Level)
        {
            if(Clock+1.e-5>=E.DecayAt){const int32 Lost=FMath::FloorToInt((Clock+1.e-5-E.DecayAt)/E.DecayInterval)+1;E.Level=FMath::Max(0,E.Level-Lost);E.DecayAt+=Lost*E.DecayInterval;Save();}
            if(E.Level&&Clock+1.e-5>=E.NextFire){E.NextFire+=(FMath::FloorToDouble((Clock+1.e-5-E.NextFire)/E.Interval)+1)*E.Interval;Fire();}
        }
        if(!E.Level){Save();ReleaseHeldRoot(E.Root);MaintainedEmitters.RemoveAtSwap(I);}
    }
}
}
