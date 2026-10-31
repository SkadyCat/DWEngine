#include "Modules/ModuleManager.h"
#include "SvgWbpLabGameMode.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "RuntimeVerification.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, SvgWbpLab, "SvgWbpLab");
ASvgWbpLabGameMode::ASvgWbpLabGameMode() { DefaultPawnClass = nullptr; }
void ASvgWbpLabGameMode::StartPlay()
{
    Super::StartPlay();
    APlayerController* Player = GetWorld()->GetFirstPlayerController();
    UClass* Screen = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UICreator/WBP_CurrentCanvas.WBP_CurrentCanvas_C"));
    if (Player && Screen)
    {
        Player->bShowMouseCursor = true;
        UUserWidget* Widget = CreateWidget<UUserWidget>(Player, Screen);
        Widget->AddToViewport();
        FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Widget->TakeWidget()); Player->SetInputMode(Mode);
        StartSvgRuntimeVerification(Widget);
    }
}
