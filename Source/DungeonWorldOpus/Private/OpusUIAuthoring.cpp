#include "OpusUI.h"
#include "SvgCanvasMotionNode.h"
#include "SvgCanvasImage.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif
USvgCanvasNode* UOpusUI::CreateEditorNode(UObject* Asset,const FString& Name)
{
#if WITH_EDITOR
 auto* BP=Cast<UWidgetBlueprint>(Asset);if(!IsRunningCommandlet()||!BP||!BP->WidgetTree)return nullptr;
 auto* N=BP->WidgetTree->ConstructWidget<USvgCanvasMotionNode>(USvgCanvasMotionNode::StaticClass(),FName(Name));
 N->Surface=BP->WidgetTree->ConstructWidget<USvgCanvasImage>(USvgCanvasImage::StaticClass(),FName(Name+TEXT("_SVG")));
 N->AddChildToCanvas(N->Surface)->SetZOrder(-10000);N->bIsVariable=true;return N;
#else
 return nullptr;
#endif
}
bool UOpusUI::FinalizeEditorHUD(UObject* Asset)
{
#if WITH_EDITOR
 auto* BP=Cast<UWidgetBlueprint>(Asset);if(!IsRunningCommandlet()||!BP||!BP->WidgetTree)return false;
 BP->WidgetTree->ForEachWidget([&](UWidget* W)
 {
  if(!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName()))BP->OnVariableAdded(W->GetFName());
  if(auto* N=Cast<USvgCanvasNode>(W))N->RememberImportedDefaults();
 });
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return BP->GeneratedClass&&BP->Status!=BS_Error;
#else
 return false;
#endif
}
