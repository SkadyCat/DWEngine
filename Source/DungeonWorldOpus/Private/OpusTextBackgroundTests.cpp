#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpusSvgBinding.h"
#include "SvgCanvasDocument.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusTextBackgroundTest, "DungeonWorldOpus.UI.TextBackground",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOpusTextBackgroundTest::RunTest(const FString& Parameters)
{
 auto* Document=LoadObject<USvgCanvasDocument>(nullptr,TEXT("/Game/UICreator/DA_CurrentCanvas.DA_CurrentCanvas"));
 if(!TestNotNull(TEXT("Saved authoring document exists"),Document))return false;
 TSharedPtr<FJsonObject> Project;
 if(!TestTrue(TEXT("Saved authoring JSON parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Document->ProjectJson),Project)))return false;
 auto Root=Project->GetObjectField(TEXT("prefabs"))->GetObjectField(TEXT("battle-prefab-item-cell"))->GetObjectField(TEXT("root"));
 for(const FString Id:{TEXT("battle-cell-mark"),TEXT("battle-cell-count")})
 {
  TSharedPtr<FJsonObject> Source;
  TFunction<void(TSharedPtr<FJsonObject>)> Find=[&](TSharedPtr<FJsonObject> Value){if(Value->GetStringField(TEXT("id"))==Id)Source=Value;const TArray<TSharedPtr<FJsonValue>>* Children=nullptr;if(Value->TryGetArrayField(TEXT("children"),Children))for(const auto& Child:*Children)Find(Child->AsObject());};Find(Root);
  if(!TestTrue(Id+TEXT(" source exists"),Source.IsValid()))return false;
  bool Fit=false;Source->GetObjectField(TEXT("style"))->TryGetBoolField(TEXT("textBackgroundFit"),Fit);
  if(!TestTrue(Id+TEXT(" authoring enables content width"),Fit))return false;
  const auto Offsets=Source->GetObjectField(TEXT("slot"))->GetObjectField(TEXT("offsets"));
  const double Full=Offsets->GetNumberField(TEXT("right")),Height=Offsets->GetNumberField(TEXT("bottom"));
  TStrongObjectPtr<USvgCanvasNode> Node(NewObject<USvgCanvasNode>());Node->DesignRect=FVector4(0,0,Full,Height);Node->SourceId=Id;
  FJsonSerializer::Serialize(Source.ToSharedRef(),TJsonWriterFactory<>::Create(&Node->SourcePropertiesJson));
  const auto Slate=Node->TakeWidget();const bool Count=Id.EndsWith(TEXT("count"));
  OpusSvg::Text(Node.Get(),Count?TEXT("3"):TEXT("✓"));UImage* Background=nullptr;
  for(auto* Child:Node->GetAllChildren())if(Child->GetFName()==TEXT("OpusLiveTextBackground"))Background=Cast<UImage>(Child);
  if(!TestNotNull(Id+TEXT(" native background exists"),Background))return false;
  auto* Slot=CastChecked<UCanvasPanelSlot>(Background->Slot);const double Short=Slot->GetSize().X;
  TestTrue(Id+TEXT(" short text shrinks the background"),Short>0&&Short<Full);
  if(Count)TestTrue(TEXT("Count remains attached to the right edge"),FMath::IsNearlyEqual(Slot->GetPosition().X+Short,Full,.01));
  OpusSvg::Text(Node.Get(),Count?TEXT("123"):TEXT("已配"));
  TestTrue(Id+TEXT(" longer text grows within the original label bounds"),Slot->GetSize().X>Short&&Slot->GetSize().X<=Full);
  OpusSvg::Text(Node.Get(),TEXT(""));TestTrue(Id+TEXT(" empty text hides the background"),Background->GetVisibility()==ESlateVisibility::Collapsed);
  OpusSvg::Text(Node.Get(),TEXT("   "));TestTrue(Id+TEXT(" whitespace hides the background"),Background->GetVisibility()==ESlateVisibility::Collapsed);
  OpusSvg::Text(Node.Get(),Count?TEXT("2"):TEXT("✓"));TestTrue(Id+TEXT(" text restores the background"),Background->GetVisibility()!=ESlateVisibility::Collapsed&&Slot->GetSize().X<Full);
 }
 return true;
}
#endif
