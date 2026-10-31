#pragma once
#include "CoreMinimal.h"
#include "OpusWorldUI.h"
#include "OpusData.h"
#include "OpusHoverUI.generated.h"

// Text measurement and rendering of the frozen UICreator item-tooltip prefab.
// The hover/pin/action state machine is in Script/Opus/ItemHover.as.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusHoverUI : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) static void Populate(UOpusWorldScreen* Screen,FOpusEntry Item,const FString& Meta,const TArray<FOpusHoverLine>& Lines,bool Pinned,const TArray<FOpusEntry>& Materials,FOpusCraftState Craft,const FString& Currency,const FString& Omen,const FString& Feedback,bool ReadOnly=false);
 UFUNCTION(BlueprintCallable) static void PopulateDetail(UOpusWorldScreen* Screen,const FString& Title,const FString& Meta,const FString& Description,const FString& Icon,const FString& Hint,bool Pinned);
 UFUNCTION(BlueprintPure) static FVector4 AnchorRect(UWidget* Anchor,APlayerController* Player);
 UFUNCTION(BlueprintPure) static FVector2D ViewSize(APlayerController* Player);
 UFUNCTION(BlueprintCallable) static void Place(UOpusWorldScreen* Screen,FVector2D Position,float Scale);
 // Isolated transient verification through the real Slate event route.
 UFUNCTION(BlueprintCallable) static bool TestPointerButton(UWidget* Target,FKey Button,bool Pressed);
};
