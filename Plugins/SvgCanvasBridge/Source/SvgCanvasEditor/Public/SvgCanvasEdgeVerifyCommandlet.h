#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SvgCanvasEdgeVerifyCommandlet.generated.h"
UCLASS()
class USvgCanvasEdgeVerifyCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 USvgCanvasEdgeVerifyCommandlet();
 virtual int32 Main(const FString& Params) override;
};
