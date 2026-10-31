#include "DWRuleModernCombat.h"
#include "DWRuleAilments.h"
#include "DWRuleProjectileAffixes.h"
#include "DWRuleZones.h"

namespace DWRules
{
namespace
{
double Rand(uint32& Seed){Seed=Seed*1664525u+1013904223u;return double(Seed)/4294967296.;}
bool Surface(FDWRuleWorld& W,const FVector3d& At,FVector3d& Point,FVector3d& Normal)
{
    Point=At;Point.Z=0;Normal=FVector3d::UpVector;
    if(!W.ProjectGround)return true;
    FVector3f P,N;if(!W.ProjectGround(FVector3f(At),P,N))return false;
    Point=FVector3d(P);Normal=FVector3d(N).GetSafeNormal();return !Normal.IsNearlyZero();
}
bool GroundVisible(FDWRuleWorld& W,const FVector3d& At,const FDWModernGround& G)
{
    if(!W.SweepGeometry)return true;
    const FVector3d A=At-G.Normal*FVector3d::DotProduct(At-G.Position,G.Normal)+G.Normal*.05;
    const FVector3d B=G.Position+G.Normal*.05;float Time=1;
    return !W.SweepGeometry(FVector3f(A),FVector3f(B),.005f,Time);
}
TOptional<double> GroundContact(const FVector3d& From,const FVector3d& To,double Radius,const FDWModernGround& G)
{
    const FVector3d D=To-From,V=From-G.Position;
    const double H=FVector3d::DotProduct(V,G.Normal),DH=FVector3d::DotProduct(D,G.Normal);
    const FVector3d FlatD=D-G.Normal*DH,FlatV=V-G.Normal*H;
    const double Reach=Radius+G.Radius,A=FlatD.SizeSquared(),B=FVector3d::DotProduct(FlatV,FlatD),C=FlatV.SizeSquared()-Reach*Reach;
    double Enter=0,Exit=1;
    if(A>1.e-7){const double Discriminant=B*B-A*C;if(Discriminant<0)return {};Enter=FMath::Max(0.,(-B-FMath::Sqrt(Discriminant))/A);Exit=FMath::Min(1.,(-B+FMath::Sqrt(Discriminant))/A);}
    else if(C>1.e-7)return {};
    const double Height=Radius+.15;
    if(FMath::Abs(DH)<1.e-7){if(FMath::Abs(H)>Height)return {};}
    else {const double A0=(-Height-H)/DH,A1=(Height-H)/DH;Enter=FMath::Max(Enter,FMath::Min(A0,A1));Exit=FMath::Min(Exit,FMath::Max(A0,A1));}
    return Enter<=Exit+1.e-7&&Exit>=0&&Enter<=1?TOptional<double>(FMath::Max(0.,Enter)):TOptional<double>();
}
FVector3d Bezier(const TArray<FVector3d>& P,double T)
{const double U=1-T;return P[0]*(U*U*U)+P[1]*(3*U*U*T)+P[2]*(3*U*T*T)+P[3]*(T*T*T);}
}
const FDWSheet& FDWModernCombat::Sheet(int32 Plan)
{
    auto* Existing=Sheets.Find(Plan);if(Existing)return **Existing;
    if(Sheets.Num()>=512)Sheets.Reset();
    auto Value=MakeUnique<FDWSheet>();TArray<FDWHeldTrait> Held;
    if(const auto* Source=R.Host.PlanSource(Plan))for(const auto& T:Source->Traits){FDWHeldTrait H;H.Source=T;Held.Add(MoveTemp(H));}
    FString Error;const bool Ok=Value->Compute(Held,Error);checkf(Ok,TEXT("Compiled plan sheet rejected: %s"),*Error);
    auto* Result=Value.Get();Sheets.Add(Plan,MoveTemp(Value));return *Result;
}
void FDWModernCombat::Spawn(int32 I,int32 Root)
{
    auto& W=R.World;auto& C=W.Carriers;
    Gate(I,C.Id[I]);
    if(W.GetRoot(Root).Owner!=R.Hero.Owner)return;
    const auto& S=Sheet(W.GetRoot(Root).Plan);FDWModernShot St;St.Previous=St.Origin=FVector3d(C.Position[I]);
    St.Speed0=St.RoamingSpeed=FVector2f(C.Velocity[I].X,C.Velocity[I].Y).Size();St.VisualSeed=C.Id[I];
    St.bContact=S.Stat(TEXT("proj.ground_contact"))>0||R.S(TEXT("proj.ground_contact"))>0;
    St.bTint=S.Stat(TEXT("proj.ground_tint"))>0||R.S(TEXT("proj.ground_tint"))>0;
    for(const auto& T:S.Triggers)if(T.Name==TEXT("proj_roam")&&!(C.Flags[I]&FDWProjectiles::Parabolic))
    {St.bRoaming=true;St.Range=FMath::Max(.1,T.Param(TEXT("range")));St.Interval=FMath::Max(.05,T.Param(TEXT("interval")));St.TurnLeft=St.Interval;}
    if(St.bRoaming)St.Seed=uint32(R.Random()*4294967296.);
    if(St.bContact||W.GetPlan(W.GetRoot(Root).Plan).KindNames[C.Kind[I]]==TEXT("whirlwind"))St.bGrounded=Surface(W,St.Origin,St.GroundPoint,St.GroundNormal);
    if(const auto* Capture=PendingCaptures.Find(Root);Capture&&Capture->Identity==W.GetRoot(Root).Identity)
    {
        FVector3d Dir(C.Velocity[I]);Dir.Z=0;Dir.Normalize();const FVector3d Side(-Dir.Y,Dir.X,0);
        St.Curve={St.Origin,St.Origin+Dir*Capture->Bend,Capture->Target+Side*Capture->Bend,Capture->Target};
        FVector3d Last=St.Origin;for(int32 K=1;K<=24;++K){const auto P=Bezier(St.Curve,K/24.);St.CurveLength+=(P-Last).Size();Last=P;}St.CurveLength=FMath::Max(.01,St.CurveLength);
    }
    Shots.Add(C.Id[I],MoveTemp(St));
}
bool FDWModernCombat::Motion(int32 I,double Dt)
{
    auto& W=R.World;auto& C=W.Carriers;auto* St=Shots.Find(C.Id[I]);
    const auto& Root=W.GetRoot(C.Root[I]);
    if(Root.Owner!=R.Hero.Owner&&!(C.Flags[I]&FDWProjectiles::Parabolic)&&Dt>0)
    {
        const auto& S=Sheet(Root.Plan);const double Range=S.Stat(TEXT("proj.homing_range")),Turn=S.Stat(TEXT("proj.homing_turn"));
        if(Range>0||Turn>0)
        {
            const auto* Owner=W.GetOwner(Root.Owner);const int32 Target=Owner?W.NearestTarget(C.Position[I],float(4*(1+Range/100)),I,Root,Owner->Team):INDEX_NONE;
            if(Target>=0){const double Speed=FVector2f(C.Velocity[I].X,C.Velocity[I].Y).Size(),Angle=FMath::Atan2(C.Velocity[I].Y,C.Velocity[I].X);const auto To=W.TargetData.Position[Target]-C.Position[I];const double Want=FMath::Atan2(To.Y,To.X),Limit=PI*(1+Turn/100)*Dt,A=Angle+FMath::Clamp(FMath::UnwindRadians(Want-Angle),-Limit,Limit);C.Velocity[I].X=float(FMath::Cos(A)*Speed);C.Velocity[I].Y=float(FMath::Sin(A)*Speed);return true;}
        }
        return false;
    }
    if(!St&&W.GetRoot(C.Root[I]).Owner==R.Hero.Owner){Spawn(I,C.Root[I]);St=Shots.Find(C.Id[I]);}
    if(!St||Dt<=0)return false;
    if(St->Curve.Num()==4)
    {
        double Speed=St->Speed0;auto* Affix=R.Projectiles->States.Find(C.Id[I]);
        const double Age=Affix?(*Affix)->Age+Dt:C.Age[I];if(Affix)(*Affix)->Age=Age;
        if(R.S(TEXT("proj.accel")))Speed*=1+R.S(TEXT("proj.accel"))/100*Age;
        if(R.S(TEXT("proj.decel")))Speed=FMath::Max(St->Speed0*.2,Speed*(1-.4*Age));
        St->Travel+=Speed*Dt;const auto P=Bezier(St->Curve,FMath::Min(1.,St->Travel/St->CurveLength));
        C.Velocity[I]=FVector3f((P-FVector3d(C.Position[I]))/Dt);
        if(Affix)(*Affix)->Distance+=(P-FVector3d(C.Position[I])).Size();
        if(St->Travel>=St->CurveLength)St->Curve.Reset();return true;
    }
    auto* Affix=R.Projectiles->States.Find(C.Id[I]);
    if(!St->bRoaming||(Affix&&(*Affix)->bReturning)||R.S(TEXT("proj.orbit"))||(C.Flags[I]&FDWProjectiles::Parabolic))return false;
    if(Affix)(*Affix)->Age+=Dt;
    // The carrier velocity below encodes net displacement for the world sweep.
    // A reflection/turn can shorten that chord without reducing physical speed.
    const bool Continued=St->bHeading&&C.Velocity[I].Equals(St->LastVelocity,.001f);
    double Speed=Continued?St->RoamingSpeed:FVector2f(C.Velocity[I].X,C.Velocity[I].Y).Size();
    if(R.S(TEXT("proj.accel")))Speed*=1+R.S(TEXT("proj.accel"))/100*Dt;
    if(R.S(TEXT("proj.decel")))Speed=FMath::Max(St->Speed0*.2,Speed*(1-.4*Dt));
    FVector3d Position(C.Position[I]);const FVector3d Start=Position;
    double Angle=Continued?St->Heading:FMath::Atan2(C.Velocity[I].Y,C.Velocity[I].X),Left=FMath::Max(0.,FMath::Min(Dt,double(C.Life[I]-C.Age[I])));
    while(Left>1.e-9)
    {
        if(St->TurnLeft<=1.e-9){Angle+=(Rand(St->Seed)<.5?-1:1)*(PI/3+Rand(St->Seed)*PI/3);St->TurnLeft=St->Interval*(.75+Rand(St->Seed)*.5);}
        const double Step=FMath::Min3(Left,St->TurnLeft,.5/FMath::Max(.01,Speed));FVector3d Dir(FMath::Cos(Angle),FMath::Sin(Angle),0),Next=Position+Dir*Speed*Step;
        FVector3d Away=Next-St->Origin;Away.Z=0;const double Reach=Away.Size();
        if(Reach>St->Range){const auto N=Away/Reach;Next=St->Origin+N*St->Range;Next.Z=Position.Z;Dir-=N*(2*FVector3d::DotProduct(Dir,N));Angle=FMath::Atan2(Dir.Y,Dir.X);}
        float At=1;const float Radius=FMath::Min(.5f,C.Radius[I]);
        if(W.SweepGeometry&&W.SweepGeometry(FVector3f(Position),FVector3f(Next),Radius,At))
        {
            const FVector3d Proposed=Next;Next=Position+(Next-Position)*FMath::Max(0.f,At-.001f);
            float TX=1,TY=1;const bool BX=W.SweepGeometry(FVector3f(Position),FVector3f(Proposed.X,Position.Y,Position.Z),Radius,TX);
            const bool BY=W.SweepGeometry(FVector3f(Position),FVector3f(Position.X,Proposed.Y,Position.Z),Radius,TY);
            Angle=FMath::Atan2(BY||!BX?-Dir.Y:Dir.Y,BX||!BY?-Dir.X:Dir.X);St->TurnLeft=FMath::Min(St->TurnLeft,St->Interval*.25);
        }
        if(Affix)(*Affix)->Distance+=(Next-Position).Size();Position=Next;Left-=Step;St->TurnLeft-=Step;
    }
    C.Velocity[I]=FVector3f((Position-Start)/Dt);
    St->Heading=Angle;St->RoamingSpeed=Speed;St->bHeading=true;St->LastVelocity=C.Velocity[I];
    auto& MutableRoot=W.Roots[C.Root[I]];auto& Hits=C.Hits[I];
    for(auto It=St->Rehit.CreateIterator();It;++It)if(It.Value()<=R.Time+1.e-7)
    {for(int32 K=0;K<Hits.Num;++K)if(Hits.Ids[K]==It.Key()){Hits.Ids[K]=Hits.Ids[--Hits.Num];break;}MutableRoot.OverflowHits.Remove((uint64(C.Id[I])<<32)|It.Key());It.RemoveCurrent();}
    return true;
}
bool FDWModernCombat::Wall(int32 I)
{
    auto& C=R.World.Carriers;auto* St=Shots.Find(C.Id[I]);if(!St||!St->bRoaming)return false;
    C.Velocity[I].X=-C.Velocity[I].X;C.Velocity[I].Y=-C.Velocity[I].Y;St->Heading=St->bHeading?FMath::UnwindRadians(St->Heading+PI):FMath::Atan2(C.Velocity[I].Y,C.Velocity[I].X);St->LastVelocity=C.Velocity[I];St->bHeading=true;St->TurnLeft=FMath::Min(St->TurnLeft,St->Interval*.25);return true;
}
bool FDWModernCombat::Contact(int32 I,int32 Target)
{
    auto& C=R.World.Carriers;auto* St=Shots.Find(C.Id[I]);if(!St||!St->bRoaming)return false;
    St->Rehit.Add(R.World.TargetData.ExternalId[Target],R.Time+.5*FMath::Max(.05,1-R.S(TEXT("proj.rehit_interval"))/100));return true;
}
void FDWModernCombat::Hit(const FDWCombatHit& H)
{
    if(H.bDot||H.bMinion||H.bSilent||H.Amount<=0)return;const auto* Enemy=R.Enemy(H.Id);if(!Enemy)return;
    for(auto& T:R.Character.Sheet.Triggers)
    {
        FName Type;int32 Element=-1;
        if(T.Name==TEXT("hit_burning_ground")){Type=TEXT("burn");Element=0;}
        else if(T.Name==TEXT("hit_chilled_ground")){Type=TEXT("chill");Element=1;}
        else if(T.Name==TEXT("hit_shocked_ground")){Type=TEXT("shock");Element=2;}
        else if(T.Name==TEXT("hit_poison_ground")){Type=TEXT("poison");Element=4;}
        else if(T.Name==TEXT("hit_mirror_ground"))Type=TEXT("mirror");else continue;
        const FString Key=T.Trait.ToString()+TEXT("|")+T.Label;
        if(R.Time+1.e-7<GroundReady.FindRef(Key)||!R.Chance(T.Param(TEXT("chance"))))continue;
        GroundReady.Add(Key,R.Time+FMath::Max(.05,T.Param(TEXT("cooldown"))));
        FDWModernGround Next;if(!Surface(R.World,Enemy->Position,Next.Position,Next.Normal))continue;
        if(Grounds.Num()>=128)Grounds.RemoveAt(0);
        auto& G=Grounds.Add_GetRef(MoveTemp(Next));G.Id=NextGroundId++;G.Type=Type;G.Element=Element;
        G.Radius=FMath::Clamp(T.Param(TEXT("radius")),.1,20.);G.Left=FMath::Clamp(T.Param(TEXT("duration")),.05,60.);G.Strength=FMath::Max(0.,T.Param(TEXT("amount")));G.Born=R.Time;
    }
}
void FDWModernCombat::Tick(double Dt)
{
    auto& W=R.World;
    for(auto& G:Grounds)
    {
        G.Tick+=FMath::Min(Dt,FMath::Max(0.,G.Left));G.Left-=Dt;
        while(G.Tick+1.e-7>=.5)
        {
            G.Tick-=.5;if(G.Type==TEXT("mirror"))continue;
            for(uint32 Id:R.EnemiesNear(G.Position,G.Radius))
            {
                if(G.Type==TEXT("burn"))W.HostDot(R.Hero.Owner,Id,float(G.Strength*.5),0);
                else if(G.Type==TEXT("poison")){FDWAilmentApplication A;A.Amount=G.Strength*5;A.Element=4;A.Layers=1;A.Duration=2;R.Ailments->ApplyPoison(Id,A);}
                else W.ApplyStatusKey(Id,StatusKey(G.Type.ToString()),float(G.Strength),.75f,R.Hero.Owner);
            }
        }
    }
    Grounds.RemoveAll([](const auto& G){return G.Left<=1.e-7;});
    TSet<uint32> Live;for(int32 I=0;I<W.Carriers.Num;++I)if(!(W.Carriers.Flags[I]&FDWProjectiles::Dead))Live.Add(W.Carriers.Id[I]);
    for(auto It=Shots.CreateIterator();It;++It)if(!Live.Contains(It.Key()))It.RemoveCurrent();
    for(auto It=PendingCaptures.CreateIterator();It;++It)if(!W.IsRootValid(It.Key())||W.GetRoot(It.Key()).Identity!=It.Value().Identity)It.RemoveCurrent();
}
void FDWModernCombat::AfterMotion()
{
    auto& W=R.World;auto& C=W.Carriers;TArray<FDWModernGround> Contacts=Grounds;
    // Existing real ground producers participate too; time/position keys remain
    // stable across compacting their arrays and do not consume combat randomness.
    auto Add=[&](FName Type,const FVector3d& P,double Radius,double Remaining,int32 Element,uint64 Salt)
    {if(Remaining<=0)return;FDWModernGround G;if(!Surface(W,P,G.Position,G.Normal))return;G.Id=uint32(Mix64(Salt^uint64(FMath::RoundToInt64(P.X*1000))^(uint64(FMath::RoundToInt64(P.Y*1000))<<1)^uint64(FMath::RoundToInt64((R.Time+Remaining)*1000))))|0x80000000u;G.Type=Type;G.Radius=Radius;G.Element=Element;Contacts.Add(G);};
    for(const auto& F:R.Ailments->Fields)Add(F.Type,F.Position,F.Radius,F.Left,F.Type==TEXT("burn")?0:F.Type==TEXT("chill")?1:4,0xA110);
    for(const auto& F:R.Projectiles->FieldViews())Add(NAME_None,F.Position,F.Radius,F.Remaining,F.Element,0xF1E1);
    for(const auto& Z:R.Zones->Zones)if(Z->bGround&&!Z->bEnded&&Z->Shape==TEXT("circle")&&Z->Time>=Z->NextAt)Add(NAME_None,Z->Position,Z->CurrentRadius?Z->CurrentRadius:Z->Radius,Z->Life-Z->Time,Z->Element,uint64(Z->Id)|0x200000000ull);
    const int32 Count=C.Num;
    for(int32 I=0;I<Count;++I)
    {
        auto* St=Shots.Find(C.Id[I]);if(!St||!St->bContact||(C.Flags[I]&FDWProjectiles::Dead))continue;
        const FVector3d To(C.Position[I]),From=St->Previous;TSet<uint64> Inside;
        struct FEntry{int32 Field;double At;};TArray<FEntry> Entries;
        for(int32 K=0;K<Contacts.Num();++K){const auto& G=Contacts[K];if(GroundContact(To,To,C.Radius[I]+1.e-5,G).IsSet()&&(St->Inside.Contains(G.Id)||GroundVisible(W,To,G)))Inside.Add(G.Id);auto At=GroundContact(From,To,C.Radius[I],G);if(At.IsSet()&&!St->Inside.Contains(G.Id)&&GroundVisible(W,From+(To-From)*At.GetValue(),G))Entries.Add({K,At.GetValue()});}
        Entries.Sort([&](const auto& A,const auto& B){return FMath::Abs(A.At-B.At)<=1.e-7?Contacts[A.Field].Id<Contacts[B.Field].Id:A.At<B.At;});
        for(const auto& Entry:Entries)
        {
            const auto& G=Contacts[Entry.Field];
            if(G.Type!=TEXT("mirror")){if(St->bTint)St->GroundTint=G.Element;continue;}
            const int32 Parent=C.Root[I],Root=R.OpenRoot(W.GetRoot(Parent).Plan);if(Root==INDEX_NONE)continue;
            W.Extension->DerivedRoot(Root,Parent,false);const int32 Child=R.Projectiles->Clone(I,Root);W.ReleaseHeldRoot(Root);if(Child==INDEX_NONE)continue;
            C.Age[Child]=C.Age[I];C.Life[Child]=C.Life[I];C.Position[Child]=FVector3f(From+(To-From)*Entry.At);
            if(auto* State=R.Projectiles->States.Find(C.Id[Child])){auto* ParentState=R.Projectiles->States.Find(C.Id[I]);if(ParentState){(*State)->Age=(*ParentState)->Age;(*State)->Distance=(*ParentState)->Distance;}(*State)->Batch=MakeShared<TMap<uint32,int32>>();}
            FDWModernShot ChildState=*St;ChildState.Previous=FVector3d(C.Position[Child]);ChildState.Rehit.Reset();ChildState.Inside.Reset();
            if(ChildState.bRoaming)
            {
                // Like a newly cloned web carrier, begin an independent roaming
                // walk at the actual entry point. Sharing the parent's RNG and
                // turn clock kept every descendant on top of the original shot.
                ChildState.Origin=ChildState.Previous;
                const double Heading=St->bHeading?St->Heading:FMath::Atan2(C.Velocity[I].Y,C.Velocity[I].X);
                const double Speed=St->bHeading?St->RoamingSpeed:FVector2f(C.Velocity[I].X,C.Velocity[I].Y).Size();
                C.Velocity[Child]=FVector3f(float(FMath::Cos(Heading)*Speed),float(FMath::Sin(Heading)*Speed),C.Velocity[Child].Z);
                ChildState.Speed0=ChildState.RoamingSpeed=Speed;
                ChildState.TurnLeft=ChildState.Interval;
                ChildState.Seed=uint32(R.Random()*4294967296.);
                ChildState.bHeading=false;ChildState.Heading=0;ChildState.LastVelocity=FVector3f::ZeroVector;
            }
            if(ChildState.bContact)ChildState.bGrounded=Surface(W,ChildState.Previous,ChildState.GroundPoint,ChildState.GroundNormal);
            for(const auto& F:Contacts)if(GroundContact(ChildState.Previous,ChildState.Previous,C.Radius[Child]+1.e-5,F).IsSet()&&GroundVisible(W,ChildState.Previous,F))ChildState.Inside.Add(F.Id);
            ChildState.Inside.Add(G.Id);Gate(Child,ChildState.VisualSeed);Shots.Add(C.Id[Child],MoveTemp(ChildState));St=Shots.Find(C.Id[I]);++Mirrors;
        }
        St->Inside=MoveTemp(Inside);St->Previous=To;
    }
}
bool FDWModernCombat::Intercept(int32 I,int32 Target,const FVector3f& At)
{
    auto& W=R.World;auto& C=W.Carriers;const int32 OldRoot=C.Root[I];
    if(W.TargetData.ExternalId[Target]!=W.GetOwnerStatusId(R.Hero.Owner)||W.GetRoot(OldRoot).Owner==R.Hero.Owner)return false;
    const auto Specs=R.T(TEXT("projectile_capture"));if(Specs.IsEmpty())return false;
    const FDWSheetTrigger* Spec=Specs[0];for(const auto* T:Specs)if(T->Param(TEXT("chance"))>Spec->Param(TEXT("chance")))Spec=T;
    if(!R.Chance(Spec->Param(TEXT("chance"))))return false;
    const auto& EnemyPlan=W.GetPlan(W.GetRoot(OldRoot).Plan);const FName Kind=EnemyPlan.KindNames[C.Kind[I]];
    const float Speed=FVector2f(C.Velocity[I].X,C.Velocity[I].Y).Size();
    FString Key=FString::Printf(TEXT("%s|%d|%.3f|%.3f|%.3f"),*Kind.ToString(),C.Element[I],Speed,C.Life[I],C.Radius[I]);
    FDWBuildSource Source;const auto* Parent=R.Host.PlanSource(R.HeroPlan());
    TArray<FDWTraitSource> Supports;if(Parent)for(const auto& T:Parent->Traits)if(T.Id!=Parent->ActiveId)
    {auto Copy=T;Copy.Rules.RemoveAll([](const auto& Rule){return Rule.Event==TEXT("cast");});Key+=TEXT("|")+TraitSourceKey(Copy);Supports.Add(MoveTemp(Copy));}
    int32 Base=INDEX_NONE;
    if(const auto* Cached=CapturePlans.Find(Key))Base=*Cached;
    else
    {
        FDWTraitSource Active;Active.Id=FName(*FString::Printf(TEXT("__captured_%llx"),HashName(Key)));Source.ActiveId=Active.Id;
        FDWRuleSource Rule;Rule.Id=TEXT("captured_cast");Rule.Event=TEXT("cast");FDWActionSource Emit;
        Emit.Op=EOp::Emit;Emit.Kind=Kind;Emit.Origin=EOrigin::Event;Emit.Element=C.Element[I];
        Emit.Set(TEXT("count"),1).Set(TEXT("speed"),FMath::RoundToDouble(Speed*1000)/1000).Set(TEXT("lifetime"),FMath::RoundToDouble(C.Life[I]*1000)/1000).Set(TEXT("radius"),FMath::RoundToDouble(C.Radius[I]*1000)/1000).SetExpr(TEXT("damage"),FDWExprSource::Event(TEXT("amount")));
        for(int32 K=0;K<EnemyPlan.TagNames.Num()&&K<64;++K)if(((C.Tags[I]&~NonInheritedTags)&TagBit(K))&&EnemyPlan.TagNames[K]!=TEXT("summon"))Emit.Tags.Add(EnemyPlan.TagNames[K]);
        Rule.Actions.Add(Emit);Active.Rules.Add(Rule);Source.Traits.Add(Active);Source.Traits.Append(Supports);
        FString Error;Base=R.Host.CompileSource(Source,Error);if(Base<0){UE_LOG(LogTemp,Warning,TEXT("DWRules capture plan rejected: %s"),*Error);return false;}if(CapturePlans.Num()>=256)CapturePlans.Reset();CapturePlans.Add(Key,Base);
    }
    TArray<FDWTraitSource> Extras;FDWTraitSource Offense,Shape;
    if(R.Character.Offense().MakeSource(W.GetPlan(Base).Stats,Offense))Extras.Add(Offense);
    if(R.PlanSource(Shape,false,true))Extras.Add(Shape);
    FString Error;const int32 Plan=Extras.IsEmpty()?Base:R.Host.PlanWithSources(Base,Extras,Error);if(Plan<0)return false;
    const auto* Attacker=W.GetOwner(W.GetRoot(OldRoot).Owner);if(!Attacker)return false;
    const int32 Root=R.OpenRoot(Plan);if(Root==INDEX_NONE)return false;
    FDWCaptureSpec Capture;Capture.Identity=W.GetRoot(Root).Identity;Capture.Target=FVector3d(Attacker->Position);Capture.Bend=FMath::Max(.1,Spec->Param(TEXT("bend")));PendingCaptures.Add(Root,Capture);
    FDWEvent E;E.Root=Root;E.Type=uint16(EBuiltinEvent::Cast);E.Sequence=W.Roots[Root].NextSequence++;E.Position=At;E.Direction=C.Velocity[I].GetSafeNormal();E.Amount=C.Damage[I];E.Tags=TagBit(Tags::Root);
    auto* Info=R.RootInfo(Root,true);Info->Kind=TEXT("cast");Info->bManual=false;
    R.Projectiles->OnCast(Root,Plan);W.PushEvent(E);W.ReleaseHeldRoot(Root);W.EndProjectile(I,EEndReason::Capacity,true);++Captured;return true;
}
void FDWModernCombat::Gate(int32 I,uint32 VisualSeed)
{
    const auto& W=R.World;const auto& C=W.Carriers;
    const FName Kind=W.GetPlan(W.GetRoot(C.Root[I]).Plan).KindNames[C.Kind[I]];if(Kind!=TEXT("treasury_weapon")&&Kind!=TEXT("treasury_ea"))return;
    if(R.Effects.Num()>=2048)R.Effects.RemoveAt(0);auto& E=R.Effects.AddDefaulted_GetRef();E.Kind=Kind==TEXT("treasury_ea")?TEXT("__treasury_ea_gate"):TEXT("__treasury_gate");E.Position=FVector3d(C.Position[I]);E.End=FVector3d(C.Velocity[I].GetSafeNormal());E.Radius=C.Radius[I];E.From=VisualSeed;
}
bool FDWCombatRuntime::Intercept(int32 Index,int32 Target,const FVector3f& At){return Modern->Intercept(Index,Target,At);}
void FDWCombatRuntime::AfterMotion(){Modern->AfterMotion();}
float FDWCombatRuntime::WallRadius(int32 Index,float Radius)
{
    const auto* St=Modern->Shots.Find(World.Carriers.Id[Index]);return St&&St->bRoaming?FMath::Min(.5f,Radius):Radius;
}
void FDWCombatRuntime::ProjectileHit(int32 Index,const FVector3f& At)
{
    const auto& C=World.Carriers;if(Index<0||Index>=C.Num)return;
    if(Effects.Num()>=2048)Effects.RemoveAt(0);
    auto& E=Effects.AddDefaulted_GetRef();E.Kind=TEXT("__projectile_hit");E.Position=FVector3d(At);E.Element=C.Element[Index];E.Radius=.65;
    if(C.Element[Index]==1&&World.GetPlan(World.GetRoot(C.Root[Index]).Plan).KindNames[C.Kind[Index]]==TEXT("ice_lance"))E.Kind=TEXT("__projectile_hit_ice");
}
bool FDWCombatRuntime::SpawnMonster(FName Kind,int32 Count,float Duration,float Radius,bool Invulnerable,int32 Owner)
{
    if(Owner!=Hero.Owner||Character.bDead||!Host.Library.Monsters.Contains(Kind)||!FMath::IsFinite(Duration)||!FMath::IsFinite(Radius)||Duration<=0||Radius<0)return false;
    Count=FMath::Min(Count,FMath::Max(0,128-TemporaryMonsters));if(Count<=0)return false;
    FDWTemporaryMonsterRequest E;E.Kind=Kind;E.Count=Count;E.Duration=Duration;E.Radius=Radius;E.bInvulnerable=Invulnerable;
    E.Origin=HeroPosition();E.Angle=Random()*2*UE_DOUBLE_PI;TemporaryRequests.Add(E);TemporaryMonsters+=Count;return true;
}
}
