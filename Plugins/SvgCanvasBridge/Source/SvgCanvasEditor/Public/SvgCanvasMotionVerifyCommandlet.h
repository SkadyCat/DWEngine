#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SvgCanvasMotionVerifyCommandlet.generated.h"

UCLASS()
class USvgCanvasMotionVerifyCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USvgCanvasMotionVerifyCommandlet();
    virtual int32 Main(const FString& Params) override;
};
