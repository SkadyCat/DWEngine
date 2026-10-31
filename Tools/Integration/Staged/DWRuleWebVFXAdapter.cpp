#include "DWRuleVisualHost.h"
#include "DWWebVFXComponent.h"
#include "DWRuleAilments.h"
#include "DWRuleLibrary.h"
#include "DWRuleProjectileAffixes.h"
#include "DWRuleWorld.h"
#include "DWRuleZones.h"
#include "DWRuleModernCombat.h"

namespace
{
FName Variant(const TCHAR* Id,uint8 Element){return FName(*FString::Printf(TEXT("%s@%d"),Id,FMath::Min<int32>(Element,5)));}
FTransform At(const FVector3d& P,const FVector& Scale=FVector::OneVector,double Direction=0)
{return FTransform(FRotator(0,FMath::RadiansToDegrees(Direction),0),FVector(P)*100,Scale);}
const TCHAR* Ground(uint8 Element){return Element==0?TEXT("ground-burning"):Element==1?TEXT("ground-chill"):Element==2?TEXT("ground-shock"):Element==4?TEXT("ground-poison"):nullptr;}
const TCHAR* Shape(FName Name){return Name==TEXT("ring")?TEXT("area-ring"):Name==TEXT("sector")?TEXT("area-sector"):Name==TEXT("line")?TEXT("area-earth-wave"):TEXT("area-circle");}
}

void ADWRuleVisualHost::ClearWebPresentation()
{
	if (!bUsingWebEffects) return;
	LiveEffects.Reset();
	IceCarrierSeen.Reset();
	WebEffects->ClearUEVFX();
	WebEffects->BeginFrame(WebClock);
	WebEffects->EndFrame();
	bUsingWebEffects = false;
}

bool ADWRuleVisualHost::SyncWebCarriers(const DWRules::FDWRuleWorld& World)
{
	if(!WebEffects->LoadPrefabs())return false;
	const float Clock=World.GetClock();if(Clock<WebClock)IceCarrierSeen.Reset();WebDelta=FMath::Max(0.f,Clock-WebClock);WebClock=Clock;
	for(auto It=IceCarrierSeen.CreateIterator();It;++It)if(Clock-It.Value()>1.2f)It.RemoveCurrent();
	WebEffects->BeginFrame(Clock);
	const auto& Carriers=World.Projectiles();
	for(int32 I=0;I<Carriers.Num;++I)
	{
		if(Carriers.Flags[I]&DWRules::FDWProjectiles::Dead)continue;
		const auto& Plan=World.GetPlan(World.GetRoot(Carriers.Root[I]).Plan);const FName Kind=Plan.KindNames[Carriers.Kind[I]];
		if(Kind==TEXT("whirlwind")||Kind==TEXT("treasury_weapon")||Kind==TEXT("treasury_ea"))continue;
		const TCHAR* Id=Kind==TEXT("fireball")?TEXT("projectile-fireball"):Kind==TEXT("ice_lance")?TEXT("projectile-ice-lance"):Kind==TEXT("poison_flask")?TEXT("projectile-poison-flask"):TEXT("projectile-void-bolt");
		const FVector Direction=FVector(Carriers.Velocity[I]).GetSafeNormal();
		const bool Flask=Kind==TEXT("poison_flask"), Ice=Kind==TEXT("ice_lance");
		// Match the source factories' radius floors. Bottles keep their authored
		// tilt and horizontal facing as the rule carrier follows its real arc.
		const float Size=Flask?FMath::Max(.7f,Carriers.Radius[I]/.16f)/1.5f:FMath::Max(Ice?.1f:.12f,Carriers.Radius[I])/.24f;
		FRotator Facing=Direction.Rotation();if(!Ice)Facing.Pitch=0;
		FDWWebVFXParameters Params;Params.InstanceId=(uint64(1)<<60)|uint64(Carriers.Id[I]);
		if(Ice&&Carriers.Element[I]==1){Params.MappingId=TEXT("ice_lance.flight");IceCarrierSeen.Add(Carriers.Id[I],Clock);}
		WebEffects->Draw(Variant(Id,Carriers.Element[I]),Carriers.Age[I],FTransform(Facing,FVector(Carriers.Position[I])*100,FVector(Size)),Params);
	}
	for(const auto& Summon:World.GetSummons())
	{
		if(Summon.bStorm||Summon.bDead)continue;
		const auto& Plan=World.GetPlan(World.GetRoot(Summon.Root).Plan);const FName Kind=Plan.KindNames[Summon.Kind];
		const FName Prefab(*(TEXT("summon-")+Kind.ToString().Replace(TEXT("_"),TEXT("-"))));if(!WebEffects->HasPrefab(Prefab))continue;
		const float Fade=FMath::Clamp((Summon.Expire-Clock)/.3f,.01f,1.f);
		FVector P=FVector(Summon.Position)*100;P.Z=0;
		FRotator Facing=FVector(Summon.Direction).Rotation();Facing.Yaw+=90;
		WebEffects->Draw(Prefab,Clock,FTransform(Facing,P,FVector(Fade)));
	}
	return true;
}

void ADWRuleVisualHost::SyncWebCombat(const DWRules::FDWRuleWorld& World,const DWRules::FDWCombatRuntime* Combat,const DWRules::FDWRuleLibrary& Library)
{
	if(!bUsingWebEffects)return;
	const auto& Targets=World.Targets();
	auto Position=[&](uint32 Id,FVector3d& P){const int32 I=World.TargetIndexOf(Id);if(!Targets.Position.IsValidIndex(I))return false;P=FVector3d(Targets.Position[I]);P.Z=.7;return true;};
	FVector3d Last=FVector3d::ZeroVector;bool HasLast=false;
	for(const auto& Hit:World.DamageLog())
	{
		if(Hit.bDot||Hit.Amount<=0||Hit.Info.Kind==TEXT("projectile"))continue;
		FVector3d Target;if(!Position(Hit.TargetExternalId,Target))continue;
		if(LiveEffects.Num()>=2048)LiveEffects.RemoveAt(0);
		{
			auto& E=LiveEffects.AddDefaulted_GetRef();E.VisualId=NextEffectVisualId++;E.Data.Kind=TEXT("__web_hit");E.Data.Position=Target;E.Data.Element=Hit.Element;E.Data.Radius=.65;
			if(Hit.Info.Kind==TEXT("projectile")&&Hit.Element==1)
			{
				const bool KnownCarrier=IceCarrierSeen.Contains(Hit.Info.SourceId);
				const bool NewIceRoot=World.IsRootValid(Hit.Info.RootIndex)&&World.GetPlan(World.GetRoot(Hit.Info.RootIndex).Plan).KindNames.Contains(TEXT("ice_lance"));
				if(KnownCarrier||NewIceRoot)E.MappingId=TEXT("ice_lance.impact");
			}
		}
		if(Hit.Info.Strike==TEXT("arc")||Hit.Info.Strike==TEXT("chain"))
		{
			FVector3d Origin=Last;
			if(HasLast||Position(Hit.OwnerExternalId,Origin))
			{
				if(LiveEffects.Num()>=2048)LiveEffects.RemoveAt(0);
				auto& E=LiveEffects.AddDefaulted_GetRef();E.VisualId=NextEffectVisualId++;E.Data.Kind=TEXT("__web_chain");E.Data.Shape=TEXT("beam");E.Data.Position=Origin;E.Data.End=Target;E.Data.Element=2;
			}
		}
		Last=Target;HasLast=true;
	}
	for(const auto& E:LiveEffects)
	{
		const auto& D=E.Data;FVector3d P=D.Position;if(D.Kind!=TEXT("__web_hit")&&D.Kind!=TEXT("__projectile_hit")&&D.Kind!=TEXT("__projectile_hit_ice")&&D.Kind!=TEXT("__treasury_gate")&&D.Kind!=TEXT("__treasury_ea_gate")&&D.Kind!=TEXT("__treasury_ea_impact"))P.Z=0;
		FDWWebVFXParameters EventParams;EventParams.InstanceId=(uint64(3)<<60)|E.VisualId;EventParams.MappingId=E.MappingId;
		if(D.Kind==TEXT("__treasury_ea_gate"))
        {WebEffects->Draw(TEXT("treasury-ea-gate"),E.Age,FTransform(FVector(D.End).Rotation(),FVector(P)*100,FVector::OneVector),EventParams);continue;}
        if(D.Kind==TEXT("__treasury_ea_impact"))
        {WebEffects->Draw(TEXT("treasury-ea-impact"),E.Age,At(P,FVector(D.Radius)),EventParams);continue;}
        if(D.Kind==TEXT("__treasury_gate"))
        {
            const uint32 Style=(uint32(D.From)*1103515245u)%3u;
            WebEffects->Draw(FName(*FString::Printf(TEXT("royal-treasury-gate@style%u"),Style)),E.Age,FTransform(FVector(D.End).Rotation(),FVector(P)*100,FVector(FMath::Max(.01,D.Radius)/.2)),EventParams);continue;
        }
        if(D.Kind==TEXT("__projectile_hit")||D.Kind==TEXT("__projectile_hit_ice"))
        {
            if(D.Kind==TEXT("__projectile_hit_ice"))EventParams.MappingId=TEXT("ice_lance.impact");
            WebEffects->Draw(FName(*FString::Printf(TEXT("projectile-hit-%d"),D.Element)),E.Age,At(P,FVector(D.Radius/.65)),EventParams);continue;
        }
        if(D.Shape==TEXT("beam"))
		{
			P=D.Position;const FVector Delta=FVector(D.End-P);
			WebEffects->Draw(TEXT("lightning-chain"),E.Age,FTransform(Delta.Rotation(),FVector(P)*100,FVector(Delta.Size(),1,1)),EventParams);continue;
		}
		const double Radius=FMath::Max(.01,D.Radius);
		if(D.Kind==TEXT("death_lightning"))WebEffects->Draw(TEXT("death-lightning"),E.Age,At(P,FVector(Radius,Radius,1)),EventParams);
		else if(D.Kind==TEXT("poison_flask_burst"))WebEffects->Draw(TEXT("poison-flask-burst"),E.Age,At(P,FVector(Radius,Radius,1)),EventParams);
		else if(D.Kind==TEXT("__web_hit"))WebEffects->Draw(FName(*FString::Printf(TEXT("element-hit-%d"),D.Element)),E.Age,At(P,FVector(Radius,Radius,1)),EventParams);
		else
		{
			const TCHAR* Melee=D.Kind==TEXT("player_slash")?TEXT("melee-slash"):D.Kind==TEXT("monster_claw")?TEXT("melee-claw"):D.Kind==TEXT("monster_strike")?TEXT("melee-heavy-strike"):nullptr;
			if(D.Shape==TEXT("line")||D.Shape==TEXT("ring")||D.Shape==TEXT("sector")||Melee)
			{
				FDWWebVFXParameters Params=EventParams;Params.Inner=float(D.Inner/Radius);Params.Angle=float(D.Angle);
				const FVector Scale=D.Shape==TEXT("line")?FVector(D.Length/7,D.Width/1.2,1):FVector(Radius,Radius,1);
				const bool Shaped=D.Shape==TEXT("line")||D.Shape==TEXT("ring")||D.Shape==TEXT("sector");
				WebEffects->Draw(Variant(Shaped?Shape(D.Shape):Melee,D.Element),E.Age,At(P,Scale,D.Direction),Params);
			}
			else
			{
				WebEffects->Draw(FName(*FString::Printf(TEXT("element-explosion-%d"),D.Element)),E.Age,At(P,FVector(Radius,Radius,1)),EventParams);
				WebEffects->Draw(Variant(TEXT("area-circle"),D.Element),E.Age,At(P,FVector(Radius,Radius,1)),EventParams);
			}
		}
	}
    if(Combat&&Combat->Modern)
    {
        const auto& C=World.Projectiles();
        for(int32 I=0;I<C.Num;++I)
        {
            if(C.Flags[I]&DWRules::FDWProjectiles::Dead)continue;
            const auto Kind=World.GetPlan(World.GetRoot(C.Root[I]).Plan).KindNames[C.Kind[I]];
            if(Kind!=TEXT("whirlwind")&&Kind!=TEXT("treasury_weapon")&&Kind!=TEXT("treasury_ea"))continue;
            const auto* Shot=Combat->Modern->Shots.Find(C.Id[I]);
            FDWWebVFXParameters Params;Params.InstanceId=(uint64(1)<<60)|uint64(C.Id[I]);
            if(Kind==TEXT("whirlwind"))
            {
                FVector3d P(C.Position[I]);P.Z=0;
                const int32 Tint=Shot?Shot->GroundTint:-1;
                const FName Id=Tint>=0?Variant(TEXT("whirlwind"),uint8(Tint)):FName(TEXT("whirlwind"));
                WebEffects->Draw(Id,C.Age[I],At(P,FVector(C.Radius[I],C.Radius[I],1)),Params);
            }
            else if(Kind==TEXT("treasury_ea"))
            {
                const float Size=FMath::Clamp(C.Radius[I]/.6f,.7f,2.f);double Distance=C.Age[I]*FVector(C.Velocity[I]).Size();
                if(Shot)Distance=(FVector3d(C.Position[I])-Shot->Origin).Size();Params.Inner=float(Distance/Size);
                WebEffects->Draw(TEXT("projectile-treasury-ea"),C.Age[I],FTransform(FVector(C.Velocity[I]).Rotation(),FVector(C.Position[I])*100,FVector(Size)),Params);
            }
            else
            {
                const uint32 Seed=Shot?Shot->VisualSeed:C.Id[I],Style=(Seed*1103515245u)%3u;
                double Distance=C.Age[I]*FVector(C.Velocity[I]).Size();
                if(Combat->Projectiles)if(const auto* A=Combat->Projectiles->States.Find(C.Id[I]))Distance=(*A)->Distance;
                Params.Inner=C.Age[I]<.2f?float(Distance/FMath::Max(.01f,C.Radius[I]*3)):10000.f;
                WebEffects->Draw(FName(*FString::Printf(TEXT("projectile-treasury-weapon@style%u"),Style)),C.Age[I],FTransform(FVector(C.Velocity[I]).Rotation(),FVector(C.Position[I])*100,FVector(C.Radius[I]/.2)),Params);
            }
        }
        for(const auto& G:Combat->Modern->Grounds)if(G.Left>0)
        {
            FDWWebVFXParameters Params;Params.Clock=float(Combat->Time-G.Born);Params.Fade=float(FMath::Min(1.,G.Left/.3));Params.BillboardScale=float(FMath::Sqrt(G.Radius));
            const TCHAR* Name=G.Type==TEXT("mirror")?TEXT("ground-mirror"):Ground(uint8(G.Element));if(!Name)continue;
            FVector3d P=G.Position;P.Z=0;
            WebEffects->Draw(FName(Name),float(Combat->Time-G.Born),At(P,FVector(G.Radius,G.Radius,1)),Params);
        }
    }
	int32 StatusCount=0;
	for(int32 I=0;I<Targets.Num()&&StatusCount<96;++I)
	{
		if(Targets.Health[I]<=0)continue;
		FVector3d P(Targets.Position[I]);P.Z=0;const double Radius=FMath::Max(.2f,Targets.Radius[I]);
		for(const auto& Status:Library.Statuses)
		{
			if(World.GetStatusRemaining(Targets.ExternalId[I],Status.Key)<=0)continue;
			const FName Id(*(FString(TEXT("status-"))+Status.Id.ToString()));
			if(WebEffects->HasPrefab(Id)){WebEffects->Draw(Id,WebClock,At(P,FVector(Radius/.5,Radius/.5,FMath::Max(1.5,Radius*3.2)/1.6)));++StatusCount;}
		}
	}
	int32 Fields=0;
	auto Field=[&](const FVector3d& Position,double Radius,uint8 Element)
	{
		const TCHAR* Id=Ground(Element);if(!Id||Fields>=48)return;
		FVector3d P=Position;P.Z=0;FDWWebVFXParameters Params;Params.Clock=WebClock;Params.BillboardScale=float(FMath::Sqrt(Radius));
		WebEffects->Draw(Id,WebClock,At(P,FVector(Radius,Radius,1)),Params);++Fields;
	};
	if(Combat&&Combat->Ailments)for(const auto& F:Combat->Ailments->Fields)if(F.Left>0)Field(F.Position,F.Radius,F.Type==TEXT("burn")?0:F.Type==TEXT("chill")?1:4);
	if(Combat&&Combat->Projectiles)for(const auto& F:Combat->Projectiles->FieldViews())Field(F.Position,F.Radius,F.Element);
	if(Combat&&Combat->Zones)for(const auto& Z:Combat->Zones->Zones)
	{
		if(Fields>=48||Z->bEnded||(!Z->bPersistent&&!Z->bGround)||Z->Time<Z->NextAt)continue;
		const double Scale=Z->Radius>0?(Z->CurrentRadius!=0?Z->CurrentRadius:Z->Radius)/Z->Radius:1;
		const double Radius=(Z->Shape==TEXT("ring")?Z->Outer:Z->Shape==TEXT("sector")?Z->SectorRadius:Z->Radius)*Scale;
		FVector3d P=Z->Position;P.Z=0;FDWWebVFXParameters Params;Params.bPersistent=true;Params.Clock=WebClock;Params.BillboardScale=float(FMath::Sqrt(Radius));Params.Inner=float(Z->Inner*Scale/FMath::Max(.01,Radius));Params.Angle=float(Z->Angle);
		WebEffects->Draw(Variant(Shape(Z->Shape),Z->Element),float(Z->Time),At(P,Z->Shape==TEXT("line")?FVector(Z->Length/7,Z->Width/1.2,1):FVector(Radius,Radius,1),Z->Direction),Params);
		if(Z->bGround&&Z->Shape==TEXT("circle"))Field(P,Radius,Z->Element);else ++Fields;
	}
	WebEffects->EndFrame();
}
