#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpusTraitText.h"
#include "OpusSvgBinding.h"
#include "SvgCanvasNode.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusInspectorTextTest,"Opus.UI.Inspector.ConcreteParameters",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpusInspectorTextTest::RunTest(const FString&)
{
 DWRules::FDWRuleLibrary Library;DWRules::FDWLibraryTrait Def;
 Def.Path=FPaths::ProjectContentDir()/TEXT("RulesTower/Affixes/life_recoup_elemental.json");Def.Source.Params.Add(TEXT("percent"),15);Def.Source.Params.Add(TEXT("seconds"),4);
 Library.Traits.Add(TEXT("life_recoup_elemental"),Def);DWRules::FDWTraitEntry Entry;Entry.Id=TEXT("life_recoup_elemental");Entry.Params.Add(TEXT("percent"),39);Entry.Params.Add(TEXT("seconds"),7);
 TestEqual(TEXT("both legacy slots use the concrete rolled values"),OpusTraitText::Format(TEXT("受到元素命中伤害的n%在接下来n秒内恢复为气血"),Entry,Library),FString(TEXT("受到元素命中伤害的39%在接下来7秒内恢复为气血")));
 Def.Path=FPaths::ProjectContentDir()/TEXT("RulesTower/Affixes/shield_per_intelligence.json");Def.Source.Params.Reset();Def.Source.Params.Add(TEXT("amount"),2);Def.Source.Params.Add(TEXT("per"),5);Library.Traits.Add(TEXT("shield_per_intelligence"),Def);
 Entry.Id=TEXT("shield_per_intelligence");Entry.Params.Reset();Entry.Params.Add(TEXT("per"),13);Entry.Params.Add(TEXT("amount"),42);
 TestEqual(TEXT("authoring slot order survives unordered parameter storage"),OpusTraitText::Format(TEXT("每n点智力获得基础护盾 +n点"),Entry,Library),FString(TEXT("每13点智力获得基础护盾 +42点")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpusInspectorRasterTest,"Opus.UI.Inspector.MovedInteractionBrushes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpusInspectorRasterTest::RunTest(const FString&)
{
 auto* Root=NewObject<USvgCanvasNode>();Root->DesignRect=FVector4(100,200,400,700);
 auto* Row=NewObject<USvgCanvasNode>(Root);Root->AddChildToCanvas(Row);Row->DesignRect=FVector4(114,336,332,34);
 Row->Surface=NewObject<UImage>(Row);Row->AddChildToCanvas(Row->Surface);Row->Input=NewObject<UButton>(Row);Row->AddChildToCanvas(Row->Input);
 Row->Normal.Texture=NewObject<UTexture2D>();Row->Normal.PixelOrigin=FVector2D(114,336);Row->Normal.PixelSize=FVector2D(332,34);Row->Hovered=Row->Pressed=Row->Normal;Row->BindInput();
 auto* Child=NewObject<USvgCanvasNode>(Row);Row->AddChildToCanvas(Child);Child->DesignRect=FVector4(122,340,40,26);Child->Normal=Row->Normal;Child->Normal.PixelOrigin=FVector2D(122,340);
 OpusSvg::PlaceNode(Row,14,160,332,60);
 TestTrue(TEXT("moving a row rebases its descendants"),FVector2D(Child->DesignRect.X,Child->DesignRect.Y).Equals(FVector2D(122,364),.01));
 for(int Pass=0;Pass<3;Pass++)
 {
  OpusSvg::PlaceNode(Row,14,160+Pass*37,332,60);
  Row->Input->OnHovered.Broadcast();Row->Input->OnPressed.Broadcast();Row->Input->OnReleased.Broadcast();Row->Input->OnUnhovered.Broadcast();
  const auto* Slot=Cast<UCanvasPanelSlot>(Row->Surface->Slot);
  TestTrue(TEXT("all interaction brushes still fill the moved row"),Slot&&Slot->GetAnchors().Maximum==FVector2D(1,1)&&Slot->GetOffsets()==FMargin(0));
  TestTrue(TEXT("brush and control retain the same size"),FVector2D(Row->Surface->GetBrush().ImageSize).Equals(FVector2D(332,60),.01));
 }
 return true;
}
#endif
