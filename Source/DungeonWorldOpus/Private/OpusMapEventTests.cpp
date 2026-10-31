#if WITH_DEV_AUTOMATION_TESTS
#include "OpusMapEvents.h"
#include "DWRuleStateReader.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusMapEventTest,"Opus.MapEvents.ItemTransactions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpusMapEventTest::RunTest(const FString&)
{
 using namespace DWRules;
 auto* World=NewObject<UWorld>();auto* R=NewObject<UDWRuleWorldSubsystem>(World);R->Configure(nullptr);FString Error;
 if(!TestTrue(TEXT("load shipping tower rules"),R->LoadRuleLibrary(TEXT("RulesTower"),Error))){AddError(Error);return false;}
 R->RegisterOwner(1,0);if(!R->EnablePlayer(0,Error)||!R->StartPlayerProgression(Error)){AddError(Error);return false;}R->RespawnPlayer();
 const auto& Lib=R->Library();FDWEquipmentRandom Random(7121);auto Profile=R->PlayerInventory();
 const FDWCollection *A=nullptr,*B=nullptr;
 for(const auto& P:Lib.Equipment)if(P.Value.Slot==TEXT("weapon")&&P.Value.Legendary.IsSet()) {if(!A)A=&P.Value;else if(P.Key!=A->Id){B=&P.Value;break;}}
 if(!TestTrue(TEXT("two authored weapon legendaries"),A&&B))return false;
 FDWEquipmentInstance Left,Right;
 if(!FDWEquipmentRules::Roll(Lib,*A,TEXT("legendary"),[&](){return Random.Next();},TEXT("event_test_left"),Left,Error,2)||!FDWEquipmentRules::Roll(Lib,*B,TEXT("legendary"),[&](){return Random.Next();},TEXT("event_test_right"),Right,Error,7)){AddError(Error);return false;}
 Profile.Loadout.Instances.Append({Left,Right});Profile.Loadout.Owned.Add(TEXT("quad_affix_orb"),7);
 if(!TestTrue(TEXT("load test backpack"),R->ImportPlayerState(FDWRuleStateReader::Write(Profile,Lib),Error))){AddError(Error);return false;}
 const FString Token=TEXT("test-fusion"),Saved=R->ExportPlayerState();
 TestTrue(TEXT("lock left"),R->LockPlayerEventItem(Left.Uid,Token,Error));TestTrue(TEXT("lock right"),R->LockPlayerEventItem(Right.Uid,Token,Error));
 TestFalse(TEXT("locked gear cannot equip"),R->EquipPlayerItem(Left.Slot,FName(*Left.Uid),Error));
 TestFalse(TEXT("locked gear cannot discard"),R->DiscardPlayerEquipment(Left.Uid,Error));
 int64 Price=0;TestFalse(TEXT("locked gear cannot sell"),R->SellPlayerEquipment(Left.Uid,Price,Error));
 TestFalse(TEXT("locked profile cannot be replaced"),R->ImportPlayerState(Saved,Error));
 TestFalse(TEXT("wrong token cannot unlock"),R->UnlockPlayerEventItem(Left.Uid,TEXT("wrong"),Error));
 TestFalse(TEXT("ordinary crafting cannot bypass altar"),R->CraftPlayerEquipment(Left.Uid,TEXT("quad_affix_orb"),NAME_None,-1,Error));
 TestEqual(TEXT("rejected actions preserve inventory"),R->ExportPlayerState(),Saved);
 auto* Invalid=NewObject<UOpusEventOffer>();TestFalse(TEXT("one UID cannot be used twice"),Invalid->PrepareFusion(R,Left.Uid,Left.Uid,Token,{0},{0},Error));
 auto* WrongToken=NewObject<UOpusEventOffer>();TestTrue(TEXT("stage immutable wrong-token request"),WrongToken->PrepareFusion(R,Left.Uid,Right.Uid,TEXT("wrong"),{0},{0},Error));TestFalse(TEXT("commit checks both lock tokens"),WrongToken->Claim({0},Error));TestEqual(TEXT("rejected fusion leaves both inputs intact"),R->ExportPlayerState(),Saved);
 auto* O=NewObject<UOpusEventOffer>();
 TestTrue(TEXT("both legendary lines and ordinary lines can merge"),O->PrepareFusion(R,Left.Uid,Right.Uid,Token,{0,Left.Affixes.Num()},{1,Right.Affixes.Num()},Error));
 TestTrue(TEXT("consume two and grant one atomically"),O->Claim({0},Error));
 TestFalse(TEXT("cannot claim fusion twice"),O->Claim({0},Error));
 auto Entries=O->Entries();if(!TestEqual(TEXT("one result"),Entries.Num(),1))return false;
 const auto* Result=R->PlayerInventory().Loadout.Instances.FindByPredicate([&](const auto& I){return I.Uid==Entries[0].Id;});
 if(!TestNotNull(TEXT("new item exists"),Result))return false;
 TestTrue(TEXT("both originals consumed"),!R->PlayerInventory().Loadout.Instances.ContainsByPredicate([&](const auto& I){return I.Uid==Left.Uid||I.Uid==Right.Uid;}));
 TestEqual(TEXT("left base retained"),Result->BaseId,Left.BaseId);TestEqual(TEXT("left T retained"),Result->Tier,Left.Tier);
 if(!TestEqual(TEXT("two arbitrary ordinary lines"),Result->Affixes.Num(),2)||!TestEqual(TEXT("multiple fixed legendary lines"),Result->AdditionalLegendary.Num(),1)||!TestTrue(TEXT("primary legendary present"),Result->Legendary.IsSet()))return false;
 TestEqual(TEXT("source tier retained exactly"),Result->Affixes[1].Tier,Right.Affixes[1].Tier);TestTrue(TEXT("source numeric values retained exactly"),Result->Affixes[1].Traits[0].Params.OrderIndependentCompareEqual(Right.Affixes[1].Traits[0].Params));
 TestEqual(TEXT("left fixed trait"),Result->Legendary->Traits[0].Id,Left.Legendary->Traits[0].Id);TestEqual(TEXT("right fixed trait"),Result->AdditionalLegendary[0].Traits[0].Id,Right.Legendary->Traits[0].Id);
 int32 TraitCount=Result->Legendary->Traits.Num();for(const auto& Affix:Result->Bases)TraitCount+=Affix.Traits.Num();for(const auto& Affix:Result->Affixes)TraitCount+=Affix.Traits.Num();for(const auto& Affix:Result->AdditionalLegendary)TraitCount+=Affix.Traits.Num();TestEqual(TEXT("all fixed lines reach ordinary rule document"),FDWEquipmentRules::Document(*Result,&Lib).Traits.Num(),TraitCount);
 const FString ResultUid=Result->Uid;const auto Extra=Result->AdditionalLegendary;
 const FString Roundtrip=R->ExportPlayerState();TestTrue(TEXT("fusion save reload"),R->ImportPlayerState(Roundtrip,Error));TestEqual(TEXT("save content stable after reload"),R->ExportPlayerState(),Roundtrip);
 TestTrue(TEXT("fusion can be equipped through existing compiler"),R->EquipPlayerItem(Left.Slot,FName(*ResultUid),Error));TestTrue(TEXT("unequip before altar"),R->EquipPlayerItem(Left.Slot,NAME_None,Error));
 TestTrue(TEXT("lock result for free altar"),R->LockPlayerEventItem(ResultUid,TEXT("free"),Error));
 const auto RealCurrency=R->PlayerInventory().Loadout.Owned;const auto RealBelt=R->PlayerInventory().Belt;
 TestFalse(TEXT("wrong token cannot craft"),R->CraftPlayerEventItem(ResultUid,TEXT("other"),TEXT("quad_affix_orb"),-1,Error));
 for(int32 I=0;I<120;I++)if(!R->CraftPlayerEventItem(ResultUid,TEXT("free"),TEXT("quad_affix_orb"),-1,Error)){AddError(Error);return false;}
 TestTrue(TEXT("infinite altar never spends or mints currency"),R->PlayerInventory().Loadout.Owned.OrderIndependentCompareEqual(RealCurrency));TestTrue(TEXT("free currency does not alter equipped consumables"),R->PlayerInventory().Belt==RealBelt);
 const auto* After=R->PlayerInventory().Loadout.Instances.FindByPredicate([&](const auto& I){return I.Uid==ResultUid;});
 TestEqual(TEXT("normal whole-item reroll retains inherited legendary"),After->AdditionalLegendary.Num(),1);TestEqual(TEXT("inherited secondary trait retained"),After->AdditionalLegendary[0].Traits[0].Id,Extra[0].Traits[0].Id);
 TestTrue(TEXT("collect unlocks result"),R->UnlockPlayerEventItem(ResultUid,TEXT("free"),Error));
 // Zero lines from either side is legal, including a formerly legendary left base.
 auto Fresh=R->PlayerInventory();Fresh.Loadout.Instances.Append({Left,Right});
 TestTrue(TEXT("prepare another pair"),R->ImportPlayerState(FDWRuleStateReader::Write(Fresh,Lib),Error));R->LockPlayerEventItem(Left.Uid,Token,Error);R->LockPlayerEventItem(Right.Uid,Token,Error);
 auto* Empty=NewObject<UOpusEventOffer>();TestTrue(TEXT("zero-line draw allowed"),Empty->PrepareFusion(R,Left.Uid,Right.Uid,Token,{}, {},Error));TestTrue(TEXT("zero-line commit"),Empty->Claim({0},Error));
 const auto ZeroUid=Empty->Entries()[0].Id;TestTrue(TEXT("zero-line save reload"),R->ImportPlayerState(R->ExportPlayerState(),Error));
 const auto* Zero=R->PlayerInventory().Loadout.Instances.FindByPredicate([&](const auto& I){return I.Uid==ZeroUid;});TestTrue(TEXT("template legendary is not regenerated on fusion reload"),Zero&&Zero->bSynthesized&&!Zero->Legendary&&Zero->AdditionalLegendary.IsEmpty()&&Zero->Affixes.IsEmpty());
 auto* Reward=NewObject<UOpusEventOffer>();if(!TestTrue(TEXT("three real legendary candidates"),Reward->Generate(R,UOpusMapEvents::EquipmentCandidates(R),71,3,TEXT("legendary"),3,1,Error))){AddError(Error);return false;}
 const auto Candidates=Reward->Entries();TestEqual(TEXT("exactly three candidates"),Candidates.Num(),3);const auto Count=R->PlayerInventory().Loadout.Instances.Num();
 TestFalse(TEXT("invalid selection rejected"),Reward->Claim({-1},Error));TestEqual(TEXT("invalid selection grants nothing"),R->PlayerInventory().Loadout.Instances.Num(),Count);TestEqual(TEXT("candidate remains frozen"),Reward->Entries()[0].Id,Candidates[0].Id);
 TestTrue(TEXT("choose exactly one legendary"),Reward->Claim({1},Error));TestEqual(TEXT("one reward granted"),R->PlayerInventory().Loadout.Instances.Num(),Count+1);TestFalse(TEXT("reward cannot duplicate"),Reward->Claim({2},Error));
 FOpusMapEventConfig Config;TestTrue(TEXT("shipping event config"),UOpusMapEvents::ReadConfig(UOpusMapEvents::LocalConfig(),Config,Error));TestEqual(TEXT("four event kinds"),Config.Definitions.Num(),4);
 return true;
}
#endif
