#include "Clean3CControlLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
void UClean3CControlLibrary::SetWorldWidgetClass(UWidgetComponent* Component, TSubclassOf<UUserWidget> WidgetClass)
{
    if (Component)
    {
        Component->SetWidgetClass(WidgetClass);
        Component->InitWidget();
    }
}
void UClean3CControlLibrary::SetupCursorInput(APlayerController *Controller)
{
    if (!Controller || !Controller->IsLocalController())
        return;
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    Controller->SetInputMode(Mode);
    Controller->bShowMouseCursor = true;
}
