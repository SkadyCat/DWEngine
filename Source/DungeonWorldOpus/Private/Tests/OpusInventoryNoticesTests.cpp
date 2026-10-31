#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpusInventoryNotices.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusInventoryNoticesTest,"Opus.UI.InventoryNotices",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpusInventoryNoticesTest::RunTest(const FString&)
{
 TStrongObjectPtr<UOpusInventoryNotices> State(UOpusInventoryNotices::Create(false));
 FOpusEntry Old;Old.Id=TEXT("old");Old.Kind=TEXT("equipment");Old.Count=1;
 State->Observe({Old});TestFalse(TEXT("old inventory initializes without false prompts"),State->IsNew(Old.Id));
 FOpusEntry New;New.Id=TEXT("new");New.Kind=TEXT("item");New.Count=2;State->Observe({Old,New});TestTrue(TEXT("acquisition prompts"),State->IsNew(New.Id));
 TestTrue(TEXT("hover consumes"),State->MarkSeen(New.Id));State->Observe({Old,New});TestFalse(TEXT("same data never re-arms"),State->IsNew(New.Id));
 New.Count=1;State->Observe({Old,New});TestFalse(TEXT("consumption does not prompt"),State->IsNew(New.Id));
 New.Count=2;State->Observe({Old,New});TestTrue(TEXT("stack increase re-arms"),State->IsNew(New.Id));
 State->MarkSeen(New.Id);Old.Rarity=TEXT("legendary");Old.Equipped=true;State->Observe({Old,New});TestFalse(TEXT("equipment edits do not prompt"),State->IsNew(Old.Id));
 State->Observe({Old});TestFalse(TEXT("removed items prune notices"),State->IsNew(New.Id));
 return true;
}
#endif
