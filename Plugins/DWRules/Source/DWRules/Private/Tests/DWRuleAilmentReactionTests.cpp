#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleCombat.h"
#include "DWRuleAilments.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWAilmentReactions,"DWRules.Opus.AilmentReactions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDWAilmentReactions::RunTest(const FString&)
{
    const TArray<FName> Ids={TEXT("chill_consume_burst"),TEXT("shock_consume_strike"),TEXT("freeze_consume_shards")};
    const TArray<FName> Keys={TEXT("chill"),TEXT("shock"),TEXT("freeze")};
    for(int32 Index=0;Index<Ids.Num();++Index)
    {
        FDWRuleLibrary Library;FString Error;FDWRuleHost Host;
        if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),Library,Error)||!Host.SetLibrary(MoveTemp(Library),Error)){AddError(Error);return false;}
        const int32 Plan=Host.CompileTraits(Ids[Index],{Ids[Index]},Error);
        if(!TestTrue(TEXT("reaction compiles: ")+Error,Plan>=0))return false;
        const int32 Owner=Host.World.AddOwner(100,0);
        for(int32 Target=0;Target<5;++Target)Host.World.Targets().Add(FVector3f(5,float(Target)*1.4f,0),.05f,10000,1,uint32(Target+1));
        auto Hit=[&](){const int32 Root=Host.World.OpenRoot(Plan,Owner,123);FDWDamageInfo Info;Info.Position=FVector3f(5,0,0);Host.World.HostHit(Root,1,1,uint8(EElement::Physical),Info);Host.World.ReleaseHeldRoot(Root);Host.Step(1.f/60);};
        Hit();TestEqual(TEXT("absent status only deals initial hit"),Host.World.DamageLog().Num(),1);Host.World.DamageLog().Reset();
        Host.World.ApplyStatusKey(1,StatusKey(Keys[Index].ToString()),30,5,Owner);Hit();
        TestEqual(TEXT("status consumed"),Host.World.GetStatus(1,StatusKey(Keys[Index].ToString())),0.f);
        TestEqual(TEXT("small/medium/large affected target count"),Host.World.DamageLog().Num(),Index+3);
        Host.World.DamageLog().Reset();Hit();TestEqual(TEXT("no repeat without new status"),Host.World.DamageLog().Num(),1);
    }
    FDWRuleLibrary Library;FString Error;FDWRuleHost Host;
    if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),Library,Error)||!Host.SetLibrary(MoveTemp(Library),Error)){AddError(Error);return false;}
    const int32 Plan=Host.CompileTraits(Ids[0],Ids,Error);if(!TestTrue(TEXT("combined reactions compile: ")+Error,Plan>=0))return false;
    FDWRuleCharacter Character;FDWCombatRuntime Combat(Host,Character);Combat.Hero.Owner=Host.World.AddOwner(100,0);Combat.Hero.Plan=Plan;
    auto& Enemy=Combat.Enemies.AddDefaulted_GetRef();Enemy.Id=1;Enemy.Position=FVector3d(5,0,0);Enemy.Health=Enemy.MaxHealth=10000;
    Host.World.Targets().Add(FVector3f(5,0,0),.1f,10000,1,1);
    Combat.Ailments->AddShock(1,30,5);
    Host.World.ApplyStatusKey(1,StatusKey(TEXT("chill")),30,5,Combat.Hero.Owner);
    Host.World.ApplyStatusKey(1,StatusKey(TEXT("freeze")),1,5,Combat.Hero.Owner);
    const int32 Root=Combat.OpenRoot();FDWDamageInfo Info;Info.Position=FVector3f(5,0,0);
    Host.World.HostHit(Root,1,1,uint8(EElement::Physical),Info);Host.World.ReleaseHeldRoot(Root);Host.Step(1.f/60);Combat.ConsumeRows();
    TestEqual(TEXT("runtime shock layers consumed"),Combat.Ailments->ShockLayers(1),0);
    for(const FName Key:Keys)TestEqual(TEXT("all three statuses consumed"),Host.World.GetStatus(1,StatusKey(Key.ToString())),0.f);
    for(int32 Frame=0;Frame<60;++Frame){Host.World.DamageLog().Reset();Combat.BeforeStep(1./60);Host.Step(1.f/60);Combat.ConsumeRows();}
    TestEqual(TEXT("consumed shock never returns from mirror"),Host.World.GetStatus(1,StatusKey(TEXT("shock"))),0.f);
    return true;
}
#endif
