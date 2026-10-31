#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SvgWbpLabGameMode.generated.h"

UCLASS()
class ASvgWbpLabGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASvgWbpLabGameMode();
    virtual void StartPlay() override;
};
