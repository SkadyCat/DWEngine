"""Prepare the native presentation patch while other tasks verify shared binaries."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
path=root/'Plugins/DWRules/Source/DWRules/Private/WebVFX/DWRuleWebVFXAdapter.cpp'
s=path.read_text(encoding='utf-8')
def replace(a,b):
 global s
 assert a in s,a[:90]
 s=s.replace(a,b)
replace('#include "DWRuleZones.h"','#include "DWRuleZones.h"\n#include "DWRuleModernCombat.h"')
replace('Element==1?TEXT("ground-chill"):Element==4?', 'Element==1?TEXT("ground-chill"):Element==2?TEXT("ground-shock"):Element==4?')
replace('const TCHAR* Id=Kind==TEXT("ice_lance")?', 'if(Kind==TEXT("whirlwind")||Kind==TEXT("treasury_weapon")||Kind==TEXT("treasury_ea"))continue;\n\t\tconst TCHAR* Id=Kind==TEXT("fireball")?TEXT("projectile-fireball"):Kind==TEXT("ice_lance")?')
replace('WebEffects->Draw(Variant(Id,Carriers.Element[I]),Clock,','WebEffects->Draw(Variant(Id,Carriers.Element[I]),Carriers.Age[I],')
replace('if(Kind!=TEXT("sentinel")&&Kind!=TEXT("spirit"))continue;', 'const FName Prefab(*(TEXT("summon-")+Kind.ToString().Replace(TEXT("_"),TEXT("-"))));if(!WebEffects->HasPrefab(Prefab))continue;')
replace('WebEffects->Draw(Kind==TEXT("spirit")?TEXT("summon-spirit"):TEXT("summon-sentinel"),Clock,','WebEffects->Draw(Prefab,Clock,')
replace('if(Hit.bDot||Hit.Amount<=0)continue;', 'if(Hit.bDot||Hit.Amount<=0||Hit.Info.Kind==TEXT("projectile"))continue;')
replace('if(LiveEffects.Num()>=96)', 'if(LiveEffects.Num()>=2048)')
replace('P.Z=D.Kind==TEXT("__web_hit")?P.Z:0;', 'if(D.Kind!=TEXT("__web_hit")&&D.Kind!=TEXT("__projectile_hit")&&D.Kind!=TEXT("__projectile_hit_ice")&&D.Kind!=TEXT("__treasury_gate")&&D.Kind!=TEXT("__treasury_ea_gate")&&D.Kind!=TEXT("__treasury_ea_impact"))P.Z=0;')
replace('if(D.Shape==TEXT("beam"))', '''if(D.Kind==TEXT("__treasury_ea_gate"))
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
        if(D.Shape==TEXT("beam"))''')
replace('\tint32 StatusCount=0;', '''    if(Combat&&Combat->Modern)
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
\tint32 StatusCount=0;''')
replace('Params.Clock=WebClock;','Params.Clock=WebClock;Params.BillboardScale=float(FMath::Sqrt(Radius));')
out=root/'Tools/Integration/Staged/DWRuleWebVFXAdapter.cpp';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(s,encoding='utf-8')
print(out)
