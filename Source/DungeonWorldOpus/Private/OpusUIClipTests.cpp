#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Components/Image.h"
#include "OpusSvgBinding.h"
#include "SvgCanvasDocument.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusDynamicImageClipTest, "DungeonWorldOpus.UI.DynamicImageClip",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOpusDynamicImageClipTest::RunTest(const FString& Parameters)
{
 auto* Document=LoadObject<USvgCanvasDocument>(nullptr,TEXT("/Game/UICreator/DA_CurrentCanvas.DA_CurrentCanvas"));
 if(!TestNotNull(TEXT("Saved UICreator document exists"),Document))return false;
 TSharedPtr<FJsonObject> Project;
 if(!TestTrue(TEXT("Saved authoring data parses"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Document->ProjectJson),Project)))return false;
 TSharedPtr<FJsonObject> Icon;
 TFunction<void(TSharedPtr<FJsonObject>)> Find=[&](TSharedPtr<FJsonObject> Value){if(Value->GetStringField(TEXT("id"))==TEXT("hud-ability-flame"))Icon=Value;const TArray<TSharedPtr<FJsonValue>>* Children=nullptr;if(Value->TryGetArrayField(TEXT("children"),Children))for(const auto& Child:*Children)Find(Child->AsObject());};
 Find(Project->GetObjectField(TEXT("prefabs"))->GetObjectField(TEXT("hud-prefab-ability"))->GetObjectField(TEXT("root")));
 if(!TestTrue(TEXT("Saved skill image node exists"),Icon.IsValid()))return false;
 auto* Node=NewObject<USvgCanvasNode>();Node->DesignRect=FVector4(0,0,72,72);Node->DesignScale=1;
 FJsonSerializer::Serialize(Icon.ToSharedRef(),TJsonWriterFactory<>::Create(&Node->SourcePropertiesJson));
 const FString First=FPaths::ProjectContentDir()/TEXT("OpusIcons/skill/poison_flask.png");
 const FString Second=FPaths::ProjectContentDir()/TEXT("OpusIcons/skill/frost_lance.png");
 OpusSvg::Image(Node,First);
 UImage* Image=Cast<UImage>(Node->GetChildAt(0));
 if(!TestNotNull(TEXT("Live icon exists"),Image))return false;
 TestNotNull(TEXT("Actual skill texture loaded"),Image->GetBrush().GetResourceObject());
 TestTrue(TEXT("Authored clip uses a rounded texture brush"),Image->GetBrush().DrawAs==ESlateBrushDrawType::RoundedBox);
 TestEqual(TEXT("Authored radius is retained"),Image->GetBrush().OutlineSettings.CornerRadii.X,11.5);
 UObject* FirstTexture=Image->GetBrush().GetResourceObject();OpusSvg::Image(Node,Second);
 TestTrue(TEXT("Replacing the skill changes the texture"),Image->GetBrush().GetResourceObject()!=FirstTexture);
 TestTrue(TEXT("Replacement retains its mask"),Image->GetBrush().DrawAs==ESlateBrushDrawType::RoundedBox);
 TestEqual(TEXT("One live image is reused"),Node->GetChildrenCount(),1);
 Node->DesignScale=.5;OpusSvg::Image(Node,Second);
 TestEqual(TEXT("Radius follows the authored layout scale"),Image->GetBrush().OutlineSettings.CornerRadii.X,5.75);
 Node->SourcePropertiesJson=TEXT("{\"style\":{\"clip\":false,\"radius\":11.5}}");OpusSvg::Image(Node,Second);
 TestTrue(TEXT("Unclipped glyphs keep the normal image brush"),Image->GetBrush().DrawAs==ESlateBrushDrawType::Image);
 OpusSvg::Image(Node,TEXT(""));TestTrue(TEXT("An empty slot hides the live image"),Image->GetVisibility()==ESlateVisibility::Collapsed);
 return true;
}
#endif
