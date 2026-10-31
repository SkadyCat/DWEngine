#include "Misc/AutomationTest.h"
#include "DWRuleFloorRewards.h"
using namespace DWRules;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWInventoryDropTest,"DWO.Inventory.ContextDrop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDWInventoryDropTest::RunTest(const FString&)
{
 FDWRuleLibrary Library;FDWCollection Gear;Gear.Id=TEXT("test_ring");Gear.Slot=TEXT("ring");Gear.Name=TEXT("指环");Gear.Rarity=TEXT("rare");Gear.Tier=1;Library.Equipment.Add(Gear.Id,Gear);
 FDWCollection Money;Money.Id=TEXT("test_money");Money.Name=TEXT("通货");Money.Category=TEXT("currency");Library.Items.Add(Money.Id,Money);
 FDWRuleProfile Profile;Profile.bProgression=true;FDWEquipmentInstance Item;Item.Uid=TEXT("kept_uid");Item.BaseId=Gear.Id;Item.Name=TEXT("完整词条装备");Item.Slot=Gear.Slot;Item.Rarity=Gear.Rarity;Item.Tier=1;
 FDWEquipmentAffix A;A.Id=TEXT("preserved_affix");A.Name=TEXT("保留数值");Item.Affixes.Add(A);Item.ReforgeTargets.Add(0);Profile.Loadout.Instances.Add(Item);Profile.Loadout.Equipment.Add(Item.Slot,FName(*Item.Uid));
 FDWRuleFloorRewards Rewards;FString Key,Error;
 TestTrue(TEXT("equipped instance transfers"),Rewards.DropInventory(Library,Profile,TEXT("equipment"),Item.Uid,1,FVector2d(10,20),false,Key,Error));
 TestEqual(TEXT("inventory removed once"),Profile.Loadout.Instances.Num(),0);TestFalse(TEXT("unequipped"),Profile.Loadout.Equipment.Contains(Item.Slot));
 TestEqual(TEXT("same UID"),Rewards.Ground[0].Item.Uid,Item.Uid);TestEqual(TEXT("affixes retained"),Rewards.Ground[0].Item.Affixes[0].Id,A.Id);TestEqual(TEXT("reforge binding retained"),Rewards.Ground[0].Item.ReforgeTargets[0],0);
 TestTrue(TEXT("can pick up"),Rewards.Pickup(Key,Profile,FVector2d(10,20),false,Error));TestEqual(TEXT("UID restored"),Profile.Loadout.Instances[0].Uid,Item.Uid);TestEqual(TEXT("re-pickup is not a new earned drop"),Profile.Drops,int64(0));
 TestFalse(TEXT("cannot pick twice"),Rewards.Pickup(Key,Profile,FVector2d(10,20),false,Error));TestEqual(TEXT("no duplicate"),Profile.Loadout.Instances.Num(),1);
 Profile.OwnedEquipment.Add(Gear.Id,2);TestTrue(TEXT("template stock transfers"),Rewards.DropInventory(Library,Profile,TEXT("equipment"),Gear.Id.ToString(),2,FVector2d::ZeroVector,false,Key,Error));
 TestTrue(TEXT("template stock returns"),Rewards.Pickup(Key,Profile,FVector2d::ZeroVector,false,Error));TestEqual(TEXT("template count retained"),Profile.OwnedEquipment.FindRef(Gear.Id),2);
 Profile.Loadout.Owned.Add(Money.Id,8);Profile.Belt.Add(Money.Id);
 TestTrue(TEXT("whole item stack transfers"),Rewards.DropInventory(Library,Profile,TEXT("item"),Money.Id.ToString(),8,FVector2d::ZeroVector,false,Key,Error));
 TestFalse(TEXT("empty item removed"),Profile.Loadout.Owned.Contains(Money.Id));TestFalse(TEXT("empty belt cleared"),Profile.Belt.Contains(Money.Id));
 TestTrue(TEXT("item stack returns"),Rewards.Pickup(Key,Profile,FVector2d::ZeroVector,false,Error));TestEqual(TEXT("stack count retained"),Profile.Loadout.Owned.FindRef(Money.Id),8);
 const int32 Count=Rewards.Ground.Num();TestFalse(TEXT("dead cannot drop"),Rewards.DropInventory(Library,Profile,TEXT("item"),Money.Id.ToString(),1,FVector2d::ZeroVector,true,Key,Error));TestFalse(TEXT("negative amount rejected"),Rewards.DropInventory(Library,Profile,TEXT("item"),Money.Id.ToString(),-1,FVector2d::ZeroVector,false,Key,Error));
 TestEqual(TEXT("failures do not add drops"),Rewards.Ground.Num(),Count);TestEqual(TEXT("failures do not spend items"),Profile.Loadout.Owned.FindRef(Money.Id),8);return true;
}
