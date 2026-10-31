#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleSession.h"
#include "DWRuleMinions.h"
#include "DWRuleWorldSubsystem.h"
#include "DWRuleTargetComponent.h"
#include "Engine/World.h"
#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusControlCap,"DWRules.Opus.ControlCap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpusControlCap::RunTest(const FString&)
{
 FDWRuleHost H;FDWRuleLibrary L;FString Error;
 if(!TestTrue(TEXT("current whole library loads: ")+Error,FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),L,Error)))return false;
 H.SetLibrary(MoveTemp(L),Error);const int32 Owner=H.World.AddOwner(1,0);FDWRuleCharacter C;FDWCombatRuntime R(H,C);R.Hero.Owner=Owner;
 const int32 Sentinel=H.CompileNamedBuild(TEXT("sentinel"),{},Error);const int32 Spirit=H.CompileNamedBuild(TEXT("spirit_servant"),{},Error);
 if(!TestTrue(TEXT("summon plans compile"),Sentinel>=0&&Spirit>=0))return false;
 FDWSummon Source;Source.Root=H.World.OpenRoot(Sentinel,Owner,1);Source.Kind=0;Source.Interval=1;Source.Range=10;Source.Expire=20;Source.Id=900;
 auto* First=R.Minions->Copy(Source);if(!TestNotNull(TEXT("first unit"),First))return false;R.Minions->SummonOf(*First).NextFire=7;
 Source.Root=H.World.OpenRoot(Spirit,Owner,2);auto* Second=R.Minions->Copy(Source);if(!TestNotNull(TEXT("second kind"),Second))return false;
 TestEqual(TEXT("different kinds share one control budget"),R.Minions->Alive().Num(),1);
 TestTrue(TEXT("replacement preserves attack clock"),R.Minions->SummonOf(*Second).NextFire>=7);
 TestEqual(TEXT("replacement never emits a sacrifice"),R.Minions->Sacrificed,0);
 C.Sheet.Stats.Add(TEXT("minion.control_cap"),1);R.Minions->Copy(Source,FVector3d::ZeroVector,true,true);
 TestEqual(TEXT("temporary and empowered copies use budget"),R.Minions->Alive().Num(),2);
 C.Sheet.Stats.Add(TEXT("minion.control_cap"),0);R.Minions->EnforceControlCap();TestEqual(TEXT("lowering cap evicts oldest"),R.Minions->Alive().Num(),1);
 TestTrue(TEXT("one eligible unit is sacrificed"),R.Minions->Sacrifice());TestEqual(TEXT("exactly one event"),R.Minions->Sacrificed,1);
 TestFalse(TEXT("empty sacrifice does nothing"),R.Minions->Sacrifice());TestEqual(TEXT("no spurious event"),R.Minions->Sacrificed,1);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusGrantSkill,"DWRules.Opus.EquipmentGrantedSkill",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpusGrantSkill::RunTest(const FString&)
{
 FDWRuleLibrary L;FString Error;if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),L,Error)){AddError(Error);return false;}
 FDWRuleLoadout Loadout;FDWCollection Gear;Gear.Id=TEXT("test_granter");Gear.Slot=TEXT("weapon");FDWTraitEntry Entry;Entry.Id=TEXT("grant_sacrifice");Gear.Traits.Add(Entry);L.Equipment.Add(Gear.Id,Gear);Loadout.Equipment.Add(Gear.Slot,Gear.Id);
 auto Choices=Loadout.AvailableSkills(L);TestTrue(TEXT("equipped affix grants sacrifice"),Choices.ContainsByPredicate([](const auto& S){return S.Id==TEXT("sacrifice")&&S.bEquipmentGranted;}));
 TestTrue(TEXT("granted skill can enter normal slot"),Loadout.ChangeSkill(L,0,TEXT("sacrifice"),Error));Loadout.Equipment.Reset();Loadout.Validate(L);TestTrue(TEXT("unequip removes unavailable skill"),Loadout.Skills[0].IsNone());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusActorTargetLifecycle,"DWRules.Opus.ActorTargetLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpusActorTargetLifecycle::RunTest(const FString&)
{
 UWorld* World=NewObject<UWorld>();auto* R=NewObject<UDWRuleWorldSubsystem>(World);R->Configure(nullptr);FString Error;
 if(!TestTrue(TEXT("load rules"),R->LoadRuleLibrary(TEXT("Rules"),Error)))return false;
 auto* Target=NewObject<UDWRuleTargetComponent>(World);Target->ExternalId=1;Target->Team=0;R->RegisterActorTarget(Target);const int32 StableId=Target->TargetId;
 const int32 Owner=R->RegisterOwner(1,0);if(!TestTrue(TEXT("enable player with early component"),R->EnablePlayer(Owner,Error)))return false;
 TestEqual(TEXT("session binds stable actor target"),R->PlayerSession()->TargetId,uint32(StableId));
 R->RegisterActorTarget(Target);TestEqual(TEXT("later BeginPlay registration is idempotent"),Target->TargetId,StableId);
 TestEqual(TEXT("game id still maps to same target"),R->ResolveTargetId(1),uint32(StableId));
 R->StartPlayerProgression(Error);R->RespawnPlayer();Target->Health=0;R->RespawnPlayer();TestEqual(TEXT("native pools still publish to original target"),Target->Health,float(R->PlayerSession()->Character.MaxLife));
 return true;
}
#endif
