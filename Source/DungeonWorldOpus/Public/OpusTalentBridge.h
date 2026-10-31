#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpusTalentBridge.generated.h"
class UDWRuleWorldSubsystem;
class USvgCanvasScreen;
USTRUCT(BlueprintType)
struct FOpusTalentRecord
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Id;
 UPROPERTY(BlueprintReadOnly) FString Name;
 UPROPERTY(BlueprintReadOnly) FString Kind;
 UPROPERTY(BlueprintReadOnly) FString Description;
 UPROPERTY(BlueprintReadOnly) FString Traits;
 UPROPERTY(BlueprintReadOnly) FString Requirement;
 UPROPERTY(BlueprintReadOnly) FString Category;
 UPROPERTY(BlueprintReadOnly) FString Sector;
 UPROPERTY(BlueprintReadOnly) FString SectorName;
 UPROPERTY(BlueprintReadOnly) FString Color;
 UPROPERTY(BlueprintReadOnly) FString SourceBuild;
 UPROPERTY(BlueprintReadOnly) FString SourceName;
 UPROPERTY(BlueprintReadOnly) FString Sources;
 UPROPERTY(BlueprintReadOnly) int32 Cost=1;
 UPROPERTY(BlueprintReadOnly) int32 Value=0;
 UPROPERTY(BlueprintReadOnly) FVector2D Position=FVector2D::ZeroVector;
 UPROPERTY(BlueprintReadOnly) TArray<int32> Neighbors;
};
// Data/serialization and generic raster adapters. Allocation policy belongs to AS.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusTalentBridge : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static FLinearColor CssColor(const FString& Value);
 UFUNCTION(BlueprintCallable) static void CopyFrom(USvgCanvasScreen* Target,const FString& Id,USvgCanvasScreen* Source,const FString& SourceId);
 UFUNCTION(BlueprintPure) static float Measure(USvgCanvasScreen* Screen,const FString& Id,const FString& Text,float Width);
 UFUNCTION(BlueprintCallable) static TArray<FOpusTalentRecord> Sectors();
 UFUNCTION(BlueprintCallable) static TArray<FOpusTalentRecord> Builds(UDWRuleWorldSubsystem* Rules);
 UFUNCTION(BlueprintPure) static FString Effects(UDWRuleWorldSubsystem* Rules,const FString& Traits);
 UFUNCTION(BlueprintPure) static FString Preview(UDWRuleWorldSubsystem* Rules,const FString& Build,const TArray<FString>& Ids,const TArray<FString>& Traits);
 UFUNCTION(BlueprintCallable) static void Rect(USvgCanvasScreen* Screen,const FString& Id,FVector4 Value);
 UFUNCTION(BlueprintCallable) static void InputValue(USvgCanvasScreen* Screen,const FString& Id,const FString& Value);
 UFUNCTION(BlueprintCallable) static void Options(USvgCanvasScreen* Screen,const FString& Id,const TArray<FString>& Labels,const TArray<FString>& Values,const FString& Current);
 UFUNCTION(BlueprintPure) static FString Option(USvgCanvasScreen* Screen,const FString& Id);
 UFUNCTION(BlueprintCallable) static void Scroll(USvgCanvasScreen* Screen,const FString& Id);
 UFUNCTION(BlueprintCallable) static FString PlanJson(const FString& Text,bool Encode);
 UFUNCTION(BlueprintCallable) static FString PlanFile(const FString& Text,bool Save);
 UFUNCTION(BlueprintCallable) static void CircleState(USvgCanvasScreen* Screen,const FString& Id,int32 Mask,FLinearColor Color,float Zoom=1);
 UFUNCTION(BlueprintCallable) static void LineState(USvgCanvasScreen* Screen,const FString& Id,const TArray<FString>& Lit,const TArray<FString>& Route,float Zoom=1);
 UFUNCTION(BlueprintCallable) static TArray<FOpusTalentRecord> Catalog();
 UFUNCTION(BlueprintCallable) static TArray<FOpusTalentRecord> Attributes();
 UFUNCTION(BlueprintCallable) static bool ApplyParts(UDWRuleWorldSubsystem* Rules,const TArray<FString>& Ids,const TArray<FString>& Traits,FString& Error);
 UFUNCTION(BlueprintPure) static FString TraitError(UDWRuleWorldSubsystem* Rules,const FString& Traits);
 UFUNCTION(BlueprintCallable) static FString ReadState();
 UFUNCTION(BlueprintCallable) static bool WriteState(const FString& State);
 UFUNCTION(BlueprintCallable) static void CopyRaster(USvgCanvasScreen* Screen,const FString& Target,const FString& Source);
 UFUNCTION(BlueprintCallable) static void Tint(USvgCanvasScreen* Screen,const FString& Id,FLinearColor Color);
 UFUNCTION(BlueprintCallable) static void PlaceNode(USvgCanvasScreen* Screen,const FString& Id,FVector2D Position);
 UFUNCTION(BlueprintCallable) static void Clip(USvgCanvasScreen* Screen,const FString& Id);
 UFUNCTION(BlueprintCallable) static void SmoothRaster(USvgCanvasScreen* Screen,const FString& Id);
 UFUNCTION(BlueprintCallable) static void FitScreen(USvgCanvasScreen* Screen,class APlayerController* Player);
 UFUNCTION(BlueprintPure) static FVector2D Pointer(USvgCanvasScreen* Screen,class APlayerController* Player);
 UFUNCTION(BlueprintPure) static double Stat(UDWRuleWorldSubsystem* Rules,FName Key);
};
