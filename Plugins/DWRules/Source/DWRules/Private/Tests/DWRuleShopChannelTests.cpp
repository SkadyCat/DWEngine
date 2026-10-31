#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "DWRuleProfile.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleShop.h"
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleShopChannelTest,"DWRules.Tower.ShopChannels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDWRuleShopChannelTest::RunTest(const FString& Parameters)
{
 FDWRuleLibrary Library; FString Error;
 if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),Library,Error)) { AddError(Error); return false; }
 const auto Listings=FDWShopRules::Listings(Library);
 const auto* Weapon=Listings.FindByPredicate([&](const auto& L){const auto* D=Library.Equipment.Find(L.Id);return L.Kind==TEXT("equipment")&&D&&D->Slot==TEXT("weapon")&&D->Rarity==TEXT("common");});
 if(!Weapon) { AddError(TEXT("author a common weapon for shop channel validation")); return false; }
 FDWRuleProfile Profile; Profile.Reset(Library); Profile.Stones=0; const auto EquipmentBefore=Profile.Loadout.Equipment; const uint32 RandomBefore=Profile.Random.State; const int32 InstanceBefore=Profile.Loadout.Instances.Num();
 TestTrue(TEXT("reset uses existing progression starter"),Profile.bProgression&&InstanceBefore==1);
 TestTrue(TEXT("starter is a bound T1 instance"),InstanceBefore==1&&Profile.Loadout.Instances[0].Uid==TEXT("starter_weapon")&&Profile.Loadout.Instances[0].Tier==1&&Profile.Loadout.Equipment.FindRef(TEXT("weapon"))==TEXT("starter_weapon"));
 TestFalse(TEXT("normal catalogue channel still charges authored price"),Profile.Buy(Library,Weapon->Key,Error));
 TestEqual(TEXT("failed purchase does not advance roll RNG"),Profile.Random.State,RandomBefore);
 TestTrue(TEXT("explicit existing GM channel accepts zero price"),Profile.Buy(Library,Weapon->Key,Error,TOptional<int64>(0)));
 TestEqual(TEXT("GM never changes wallet"),Profile.Stones,int64(0)); TestEqual(TEXT("GM never adds spending"),Profile.Spent,int64(0));
 TestEqual(TEXT("equipment acquisition yields a rolled instance"),Profile.Loadout.Instances.Num(),InstanceBefore+1);
 TestTrue(TEXT("new equipment waits for player to equip"),Profile.Loadout.Equipment.OrderIndependentCompareEqual(EquipmentBefore));
 if(Profile.Loadout.Instances.Num()) { const auto& Item=Profile.Loadout.Instances.Last(); const auto& Doc=Library.Equipment[Weapon->Id]; TestEqual(TEXT("chosen base retained"),Item.BaseId,Doc.Id); TestTrue(TEXT("tier stays at or below authored max"),Item.Tier>=1&&Item.Tier<=Doc.Tier); TestTrue(TEXT("base values are rolled on acquisition"),Item.bIdentified&&!Item.Bases.IsEmpty()); }
 const int32 BeforeCount=Profile.Loadout.Instances.Num();const uint32 BeforeReject=Profile.Random.State;
 TestFalse(TEXT("negative override rejected"),Profile.Buy(Library,Weapon->Key,Error,TOptional<int64>(-1)));
 TestEqual(TEXT("rejected channel does not grant items"),Profile.Loadout.Instances.Num(),BeforeCount);TestEqual(TEXT("rejected channel keeps RNG"),Profile.Random.State,BeforeReject);
 return true;
}
#endif
