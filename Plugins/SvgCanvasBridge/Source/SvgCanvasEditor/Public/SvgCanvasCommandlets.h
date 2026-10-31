#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SvgCanvasCommandlets.generated.h"
UCLASS()
class USvgCanvasImportCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USvgCanvasImportCommandlet();
    virtual int32 Main(const FString& Params) override;
};
UCLASS()
class USvgCanvasVerifyCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USvgCanvasVerifyCommandlet();
    virtual int32 Main(const FString& Params) override;
};
