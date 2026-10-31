#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SvgCanvasSoundVerifyCommandlet.generated.h"
UCLASS()
class USvgCanvasSoundVerifyCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USvgCanvasSoundVerifyCommandlet();
    virtual int32 Main(const FString& Params) override;
};
