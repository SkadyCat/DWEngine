#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/Widget.h"
#include "DWRuleWorldSubsystem.h"
#include "OpusWorldUI.h"
#include "OpusPlayerEffects.generated.h"

USTRUCT(BlueprintType)
struct FOpusPlayerEffect
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Id;
 UPROPERTY(BlueprintReadOnly) FString Name;
 UPROPERTY(BlueprintReadOnly) FString Icon;
 UPROPERTY(BlueprintReadOnly) FString Source;
 UPROPERTY(BlueprintReadOnly) FString Description;
 UPROPERTY(BlueprintReadOnly) FString Timer=TEXT("剩余");
 UPROPERTY(BlueprintReadOnly) int32 Stacks=0;
 UPROPERTY(BlueprintReadOnly) int32 Cap=0;
 UPROPERTY(BlueprintReadOnly) double Remaining=0;
 UPROPERTY(BlueprintReadOnly) double ExpiresAt=0;
 UPROPERTY(BlueprintReadOnly) FLinearColor Color=FLinearColor::White;
};

// Generic Slate presentation of UICreator's rounded outline countdown contract.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusOutlineProgress : public UWidget
{
 GENERATED_BODY()
public:
 float Ratio=1;
 FLinearColor Color=FLinearColor::White;
 void SetValue(float InRatio,FLinearColor InColor);
 virtual void ReleaseSlateResources(bool ReleaseChildren) override;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 TSharedPtr<class SOpusOutlineProgress> Outline;
};

UCLASS()
class DUNGEONWORLDOPUS_API UOpusPlayerEffects : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static TArray<FOpusPlayerEffect> Snapshot(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static double Clock(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintCallable) static void Countdown(UOpusWorldScreen* Screen,float Ratio,FLinearColor Color);
 UFUNCTION(BlueprintCallable) static void FitTooltip(UOpusWorldScreen* Screen);
 UFUNCTION(BlueprintPure) static bool VerifySnapshot(UDWRuleWorldSubsystem* Rules);
};
