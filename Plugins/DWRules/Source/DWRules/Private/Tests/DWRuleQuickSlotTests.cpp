#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "DWRuleLibraryLoader.h"
#include "DWRuleStateReader.h"
#include "DWRuleFloorRewards.h"
using namespace DWRules;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWQuickSlotTest,"DWRules.UI.QuickSlots",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDWQuickSlotTest::RunTest(const FString& Parameters)
{
 FDWRuleLibrary L;FString Error;if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("RulesTower"),L,Error)){AddError(Error);return false;}
 TArray<FName> Items;for(const auto& P:L.Items)if(P.Value.Category==TEXT("consumable"))Items.Add(P.Key);
 if(Items.Num()<2){AddError(TEXT("Need two real consumables"));return false;}
 const FName A=Items[0],B=Items[1];FDWRuleProfile P;P.StartProgression(L,Error);P.Belt.Reset();P.Loadout.Owned.Add(A,1);P.Loadout.Owned.Add(B,1);
 TestTrue(TEXT("assign T after empty Q/E"),P.SetBeltSlot(L,2,A,Error));TestEqual(TEXT("four fixed positions"),P.Belt.Num(),4);TestEqual(TEXT("T retained"),P.Belt[2],A);TestTrue(TEXT("Q remains empty"),P.Belt[0].IsNone());
 TestTrue(TEXT("assign G"),P.SetBeltSlot(L,3,B,Error));FDWRuleProfile Reloaded;
 TestTrue(TEXT("save/reload sparse belt"),FDWRuleStateReader::Read(FDWRuleStateReader::Write(P,L),L,Reloaded,Error));TestEqual(TEXT("reload keeps T"),Reloaded.Belt[2],A);TestEqual(TEXT("reload keeps G"),Reloaded.Belt[3],B);
 TestTrue(TEXT("clear T"),P.SetBeltSlot(L,2,NAME_None,Error));TestEqual(TEXT("clearing T does not move G"),P.Belt[3],B);
 TestTrue(TEXT("move duplicate from G to E"),P.SetBeltSlot(L,1,B,Error));TestEqual(TEXT("E gets B"),P.Belt[1],B);TestTrue(TEXT("old G empty"),P.Belt[3].IsNone());
 P.SetBeltSlot(L,3,A,Error);TestTrue(TEXT("consume E"),P.Consume(B));TestTrue(TEXT("depleted E clears"),P.Belt[1].IsNone());TestEqual(TEXT("consuming E keeps G"),P.Belt[3],A);
 P.Loadout.Owned.Add(B,1);P.SetBeltSlot(L,1,B,Error);FDWRuleFloorRewards Ground(1,1,1,1);FString Key;
 TestTrue(TEXT("drop the last E item"),Ground.DropInventory(L,P,TEXT("item"),B.ToString(),1,FVector2d::ZeroVector,false,Key,Error));TestTrue(TEXT("dropping clears E"),P.Belt[1].IsNone());TestEqual(TEXT("dropping E keeps G"),P.Belt[3],A);
 return true;
}
#endif
