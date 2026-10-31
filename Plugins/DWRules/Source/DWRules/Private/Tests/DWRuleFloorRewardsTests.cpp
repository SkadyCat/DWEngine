#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "DWRuleFloorRewards.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleStateReader.h"
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWFloorRewardsTest,"DWRules.Tower.FloorRewards",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDWFloorRewardsTest::RunTest(const FString& Parameters)
{
 FDWRuleLibrary Library; FString Error;
 if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),Library,Error)) { AddError(Error); return false; }
 FDWRuleProfile Profile; TestTrue(TEXT("fresh starter profile"),Profile.StartProgression(Library,Error));
 const uint32 ProfileRng=Profile.Random.State;
 FDWRuleFloorRewards Coins(71,1,1,1),SameCoins(71,1,1,1); for(int32 I=0;I<100;++I) { const int64 Paid=Coins.KillStones(100000,1); TestTrue(TEXT("level one kill gives 5-10 regardless health"),Paid>=5&&Paid<=10); TestEqual(TEXT("coin stream repeatable"),Paid,SameCoins.KillStones(1,1)); }
 TestEqual(TEXT("higher monster level retains health formula"),Coins.KillStones(400,2),int64(12));
 FDWRuleFloorRewards Rewards(71,1,1,1); Rewards.OnKill(Library,FVector2d(1000,2000),true);
 TestEqual(TEXT("boss drops an equipment and an item"),Rewards.Ground.Num(),2);
 TestEqual(TEXT("ground generation does not mutate profile RNG"),Profile.Random.State,ProfileRng);
 TestEqual(TEXT("ground drops do not immediately enter inventory"),Profile.Loadout.Instances.Num(),1);
 if(Rewards.Ground.Num()<2) return false;
 const auto Drop=Rewards.Ground[0]; TestFalse(TEXT("ground equipment awaits identification"),Drop.Item.bIdentified);
 TestEqual(TEXT("map T1 caps drop tier"),Drop.Tier,1); TestTrue(TEXT("pending drop has no rolled base values"),Drop.Item.Bases.IsEmpty());
 TestFalse(TEXT("pickup beyond 200 cm rejected"),Rewards.Pickup(Drop.Key,Profile,Drop.Position+FVector2d(201,0),false,Error));
 TestEqual(TEXT("rejected pickup is atomic"),Profile.Loadout.Instances.Num(),1);
 TestFalse(TEXT("dead hero cannot pick up"),Rewards.Pickup(Drop.Key,Profile,Drop.Position,true,Error));
 TestTrue(TEXT("nearby pickup accepted"),Rewards.Pickup(Drop.Key,Profile,Drop.Position,false,Error));
 TestEqual(TEXT("progression drop count increases on pickup"),Profile.Drops,int64(1));
 TestFalse(TEXT("duplicate pickup rejected"),Rewards.Pickup(Drop.Key,Profile,Drop.Position,false,Error));
 TestFalse(TEXT("pending drop must be identified before equipping"),Profile.Equip(Library,Drop.Item.Slot,FName(*Drop.Item.Uid),Error));
 const FString Saved=FDWRuleStateReader::Write(Profile,Library); FDWRuleProfile Reloaded;
 TestTrue(TEXT("save/reload accepts rolled drops"),FDWRuleStateReader::Read(Saved,Library,Reloaded,Error));
 if(const auto* I=Reloaded.Loadout.Instances.FindByPredicate([&](const auto& V){return V.Uid==Drop.Item.Uid;})) { TestFalse(TEXT("save preserves identification state"),I->bIdentified);TestEqual(TEXT("pickup and reload keep the same affix count"),I->Affixes.Num(),Drop.Item.Affixes.Num()); }
 auto A=Rewards,B=Rewards; A.MerchantProbability=1; A.Complete(Library); B.MerchantProbability=1; B.Complete(Library);
 TestTrue(TEXT("forced merchant available"),A.bHasMerchant); TestEqual(TEXT("merchant has two equipment and four items"),A.Stock.Num(),6);
 const int32 StockCount=A.Stock.Num(); const uint32 BeforeRandom=Profile.Random.State; Profile.Stones=100000;
 if(A.Stock.Num()) { const auto Offer=A.Stock[0]; TestTrue(TEXT("merchant equipment is identified once"),Offer.Item.bIdentified);
  TestTrue(TEXT("T1 merchant price follows existing 20-50 range"),Offer.Price>=20&&Offer.Price<=50);
  TestTrue(TEXT("first purchase accepted"),A.Buy(Offer.Key,Profile,Error)); const int64 After=Profile.Stones;
  TestFalse(TEXT("offer cannot be purchased twice"),A.Buy(Offer.Key,Profile,Error)); TestEqual(TEXT("duplicate purchase keeps wallet"),Profile.Stones,After);
  const auto* Bought=Profile.Loadout.Instances.FindByPredicate([&](const auto& V){return V.Uid==Offer.Item.Uid;}); TestTrue(TEXT("buy grants same rolled instance"),Bought&&Bought->Affixes.Num()==Offer.Item.Affixes.Num()&&Bought->BaseId==Offer.Item.BaseId);
 }
 A.Complete(Library); TestEqual(TEXT("completion is idempotent"),A.Stock.Num(),StockCount); TestEqual(TEXT("merchant never advances profile crafting RNG"),Profile.Random.State,BeforeRandom);
 A.OnKill(Library,FVector2d(0,0),true); B.OnKill(Library,FVector2d(0,0),true);
 TestEqual(TEXT("merchant random stream does not change later equipment drops"),A.Ground.Last().Id,B.Ground.Last().Id);
 FName Consumable; for(const auto& Pair:Library.Items) if(Pair.Value.Category==TEXT("consumable")) { Consumable=Pair.Key; break; }
 if(!Consumable.IsNone()) { Profile.Loadout.Owned.Add(Consumable,1); TestTrue(TEXT("equip belt slot"),Profile.SetBeltSlot(Library,2,Consumable,Error)); Profile.Consume(Consumable); TestFalse(TEXT("depleted consumable removed as web belt does"),Profile.Belt.Contains(Consumable)); }
 FDWRuleProfile Sale; Sale.StartProgression(Library,Error); int64 Price=0; TestTrue(TEXT("starter can be quoted"),Sale.QuoteSale(Library,TEXT("starter_weapon"),Price,Error)); TestEqual(TEXT("common T1 sale rate"),Price,int64(1));
 TestTrue(TEXT("sell equipped starter atomically"),Sale.Sell(Library,TEXT("starter_weapon"),Price,Error)); TestEqual(TEXT("sale credits wallet"),Sale.Stones,int64(1)); TestTrue(TEXT("sale removes unavailable bound skill"),Sale.Loadout.Skills[0].IsNone());
 return true;
}
#endif
