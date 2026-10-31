#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Clean3CControlLibrary.generated.h"
class APlayerController;
class UWidgetComponent;
class UUserWidget;
UCLASS()
class FCS3C_API UClean3CControlLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
  public:
    UFUNCTION(BlueprintCallable, Category = "3C Input")
    static void SetupCursorInput(APlayerController *Controller);
    // UE's widget-class setter is not reflected for AngelScript in 5.8.
    UFUNCTION(BlueprintCallable, Category = "3C Presentation")
    static void SetWorldWidgetClass(UWidgetComponent* Component, TSubclassOf<UUserWidget> WidgetClass);
};
