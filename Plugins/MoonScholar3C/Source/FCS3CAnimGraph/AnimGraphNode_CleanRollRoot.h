#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimNode_CleanRollRoot.h"
#include "AnimGraphNode_CleanRollRoot.generated.h"

UCLASS()
class FCS3CANIMGRAPH_API UAnimGraphNode_CleanRollRoot : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = Settings) FAnimNode_CleanRollRoot Node;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override
    { return FText::FromString(TEXT("Clean Roll Root Continuity")); }
    virtual FText GetTooltipText() const override
    { return FText::FromString(TEXT("Preserve visual root position on roll entry. Place after Inertialization.")); }
    virtual FText GetMenuCategory() const override
    { return FText::FromString(TEXT("Clean 3C")); }
};
