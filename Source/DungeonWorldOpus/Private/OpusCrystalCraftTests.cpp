#include "OpusData.h"
#include "DWRuleWorldSubsystem.h"
#include "DWRuleStateReader.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusCrystalCraftTest, "DWO.Crafting.Crystals",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOpusCrystalCraftTest::RunTest(const FString& Parameters)
{
 using namespace DWRules;
 auto* World=NewObject<UWorld>();auto* Rules=NewObject<UDWRuleWorldSubsystem>(World);Rules->Configure(nullptr);
 FString Error;
 if(!TestTrue(TEXT("load authored currency catalogue"),Rules->LoadRuleLibrary(TEXT("Rules"),Error))){AddError(Error);return false;}
 const auto& L=Rules->Library();
 const FName Remove=TEXT("annulment_crystal"),Add=TEXT("wish_crystal");
 if(!TestTrue(TEXT("both currency definitions exist"),L.Items.Contains(Remove)&&L.Items.Contains(Add)))return false;
 TestEqual(TEXT("remove operation"),L.Items[Remove].CraftOp,FName(TEXT("remove_affixes")));
 TestEqual(TEXT("one removed line"),L.Items[Remove].CraftCount,1);
 TestEqual(TEXT("append operation"),L.Items[Add].CraftOp,FName(TEXT("add_affixes")));
 TestEqual(TEXT("one appended line"),L.Items[Add].CraftCount,1);
 FDWEquipmentRandom Random(43);FDWEquipmentInstance Item;
 if(!TestTrue(TEXT("real legendary fixture"),FDWEquipmentRules::Roll(L,L.Equipment.FindChecked(TEXT("frostthorn_staff")),TEXT("legendary"),[&](){return Random.Next();},TEXT("crystal_item"),Item,Error,3))){AddError(Error);return false;}
 auto Signature=[&](const FDWEquipmentInstance& Value){FDWRuleProfile P;P.bInitialized=true;P.Loadout.Instances.Add(Value);return FDWRuleStateReader::Write(P,L);};
 auto Protected=[&](const FDWEquipmentInstance& Before,const FDWEquipmentInstance& After){auto Value=After;Value.Affixes=Before.Affixes;Value.ReforgeTargets=Before.ReforgeTargets;return Signature(Value)==Signature(Before);};
 auto Fresh=[&](){FDWRuleProfile P;P.bInitialized=true;P.Loadout.Instances.Add(Item);P.Loadout.Equipment.Add(TEXT("weapon"),FName(*Item.Uid));P.Loadout.Skills[0]=TEXT("frost_lance");P.Loadout.Owned.Add(Remove,20);P.Loadout.Owned.Add(Add,20);P.Stones=123;return P;};
 for(int32 Index=0;Index<Item.Affixes.Num();++Index)
 {
  FDWEquipmentInstance Out;const double Pick=(Index+.1)/Item.Affixes.Num();
  if(!TestTrue(TEXT("each existing line can be removed"),FDWEquipmentRules::RemoveAffixes(Item,1,[&](){return Pick;},Out,Error)))return false;
  TestEqual(TEXT("exactly one fewer line"),Out.Affixes.Num(),Item.Affixes.Num()-1);
  TestTrue(TEXT("identity, bases, fixed legendary and all other fields survive"),Protected(Item,Out));
  auto Expected=Item;Expected.Affixes.RemoveAt(Index);TestEqual(TEXT("all surviving line values match"),Signature(Out),Signature(Expected));
 }
 {
  auto Bound=Item;Bound.ReforgeTargets={3,1,4};FDWEquipmentInstance Out;
  TestTrue(TEXT("remove unbound line"),FDWEquipmentRules::RemoveAffixes(Bound,1,[](){return .01;},Out,Error));
  TestTrue(TEXT("unbound deletion remaps indices without changing grades"),Out.ReforgeTargets==TArray<int32>({2,0,3}));
  TestTrue(TEXT("remove middle grade line"),FDWEquipmentRules::RemoveAffixes(Bound,1,[](){return .21;},Out,Error));
  TestTrue(TEXT("only valid earlier grade remains"),Out.ReforgeTargets==TArray<int32>({2}));
  TestTrue(TEXT("remove first grade line"),FDWEquipmentRules::RemoveAffixes(Bound,1,[](){return .61;},Out,Error));
  TestTrue(TEXT("first grade deletion clears sequential binding"),Out.ReforgeTargets.IsEmpty());
  TestTrue(TEXT("remove third grade line"),FDWEquipmentRules::RemoveAffixes(Bound,1,[](){return .81;},Out,Error));
  TestTrue(TEXT("first two grades keep their targets"),Out.ReforgeTargets==TArray<int32>({3,1}));
 }
 auto P=Fresh();const int32 Original=Item.Affixes.Num();
 if(!TestTrue(TEXT("paid remove transaction"),P.Craft(L,Item.Uid,Remove,NAME_None,INDEX_NONE,Error))){AddError(Error);return false;}
 TestEqual(TEXT("remove costs one"),P.Loadout.Owned.FindRef(Remove),19);
 TestEqual(TEXT("transaction removes one"),P.Loadout.Instances[0].Affixes.Num(),Original-1);
 auto BeforeAdd=P.Loadout.Instances[0];
 if(!TestTrue(TEXT("paid wish transaction"),P.Craft(L,Item.Uid,Add,NAME_None,INDEX_NONE,Error))){AddError(Error);return false;}
 TestEqual(TEXT("wish costs one"),P.Loadout.Owned.FindRef(Add),19);
 TestEqual(TEXT("wish appends exactly one"),P.Loadout.Instances[0].Affixes.Num(),Original);
 TestTrue(TEXT("wish keeps identity and fixed fields"),Protected(BeforeAdd,P.Loadout.Instances[0]));
 for(int32 I=0;I<BeforeAdd.Affixes.Num();++I)TestEqual(TEXT("wish does not replace existing lines"),BeforeAdd.Affixes[I].Id,P.Loadout.Instances[0].Affixes[I].Id);
 TestEqual(TEXT("crystals do not spend spirit stones"),P.Stones,int64(123));
 {
  FDWRuleProfile Reload;const FString Saved=FDWRuleStateReader::Write(P,L);
  if(!TestTrue(TEXT("crafted state reloads"),FDWRuleStateReader::Read(Saved,L,Reload,Error))){AddError(Error);return false;}
  TestEqual(TEXT("rolled equipment persists"),Signature(Reload.Loadout.Instances[0]),Signature(P.Loadout.Instances[0]));
 }
 for(int32 Kind=0;Kind<6;++Kind)
 {
  auto Bad=Fresh();FName Currency=Remove,Omen=NAME_None;int32 Index=INDEX_NONE;
  if(Kind==0)Bad.Loadout.Instances[0].Affixes.Reset();
  if(Kind==1)Bad.Loadout.Instances[0].bIdentified=false;
  if(Kind==2)Bad.Loadout.Instances[0].Slot=TEXT("tower");
  if(Kind==3)Bad.Loadout.Owned.Remove(Remove);
  if(Kind==4)Index=0;
  if(Kind==5){Omen=TEXT("omen_elemental");Bad.Loadout.Owned.Add(Omen,1);}
  const FString Before=FDWRuleStateReader::Write(Bad,L);const uint32 State=Bad.Random.State;
  TestFalse(TEXT("invalid removal is rejected"),Bad.Craft(L,Item.Uid,Currency,Omen,Index,Error));
  TestEqual(TEXT("failed crafting changes no inventory or equipment"),FDWRuleStateReader::Write(Bad,L),Before);
  TestEqual(TEXT("failed crafting does not advance random stream"),Bad.Random.State,State);
 }
 {
  auto Exhausted=Fresh();Exhausted.Loadout.Instances[0].Affixes=FDWEquipmentRules::Pool(L);
  const FString Before=FDWRuleStateReader::Write(Exhausted,L);const uint32 State=Exhausted.Random.State;
  TestFalse(TEXT("wish rejects exhausted affix pool"),Exhausted.Craft(L,Item.Uid,Add,NAME_None,INDEX_NONE,Error));
  TestEqual(TEXT("failed wish preserves state"),FDWRuleStateReader::Write(Exhausted,L),Before);
  TestEqual(TEXT("failed wish preserves RNG"),Exhausted.Random.State,State);
 }
 if(!TestTrue(TEXT("enable native player"),Rules->EnablePlayer(Rules->RegisterOwner(1,0),Error)))return false;
 P=Fresh();
 if(!TestTrue(TEXT("import inventory through native player gateway"),Rules->ImportPlayerState(FDWRuleStateReader::Write(P,L),Error))){AddError(Error);return false;}
 auto View=UOpusData::CraftState(Rules,Item.Uid,Remove,NAME_None);
 TestTrue(TEXT("UICreator inspector exposes random remove action"),View.Operation==TEXT("remove_affixes")&&View.Count==1&&View.Blocked.IsEmpty()&&View.EligibleAffixes.IsEmpty());
 TestTrue(TEXT("native equipped gear transaction"),Rules->CraftPlayerEquipment(Item.Uid,Remove,NAME_None,INDEX_NONE,Error));
 TestTrue(TEXT("UICreator wish action is available"),UOpusData::CraftState(Rules,Item.Uid,Add,NAME_None).Blocked.IsEmpty());
 TestTrue(TEXT("native wish transaction"),Rules->CraftPlayerEquipment(Item.Uid,Add,NAME_None,INDEX_NONE,Error));
 Rules->Deinitialize();
 auto* Runtime=NewObject<UDWRuleWorldSubsystem>(World);Runtime->Configure(nullptr);
 if(!TestTrue(TEXT("runtime catalogue supports both crystals"),Runtime->LoadRuleLibrary(TEXT("RulesTower"),Error))){AddError(Error);return false;}
 TestTrue(TEXT("runtime drop item IDs include both currencies"),Runtime->Library().Items.Contains(Remove)&&Runtime->Library().Items.Contains(Add));
 Runtime->Deinitialize();return true;
}
#endif
