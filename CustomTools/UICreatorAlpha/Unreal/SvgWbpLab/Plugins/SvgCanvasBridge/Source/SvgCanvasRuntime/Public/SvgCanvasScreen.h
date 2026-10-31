#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Blueprint/UserWidget.h"
#include "SvgCanvasDocument.h"
#include "SvgCanvasNode.h"
#include "SvgCanvasScreen.generated.h"
class USvgCanvasNode;

UCLASS(Blueprintable)
class SVGCANVASRUNTIME_API USvgCanvasScreen : public UUserWidget
{
    GENERATED_BODY()
public:
    USvgCanvasScreen(const FObjectInitializer& ObjectInitializer);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG") TObjectPtr<USvgCanvasDocument> SourceDocument;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SVG") FString InitialState = TEXT("canvas");
    UPROPERTY(BlueprintReadOnly, Category="SVG") FString CurrentState;
    UPROPERTY(BlueprintReadOnly, Category="SVG") int32 SelectedSlot = 0;
    UPROPERTY(BlueprintAssignable, Category="SVG Events") FSvgCanvasInvoked OnSourceAction;
    UFUNCTION(BlueprintCallable, Category="SVG") bool ApplySceneState(const FString& State);
    UFUNCTION(BlueprintCallable, Category="SVG") USvgCanvasNode* FindSourceNode(const FString& SourceId) const;
    UFUNCTION(BlueprintCallable, Category="SVG") bool HandleInputKey(FKey Key);
protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FString ClosedState() const;
    UFUNCTION() void HandleInvoke(USvgCanvasNode* Node);
    UPROPERTY() TMap<FString, TObjectPtr<USvgCanvasNode>> SourceNodes;
    void IndexNodes();
};
