#pragma once
#include "CoreMinimal.h"
#include "OpusUI.h"
#include "OpusWorldUI.generated.h"
class USkeletalMeshComponent;

UCLASS()
class DUNGEONWORLDOPUS_API UOpusWorldScreen : public USvgCanvasScreen
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintReadOnly) FVector2D TemplateSize;
 UPROPERTY(BlueprintReadOnly) TMap<FString,float> FillValues;
 UPROPERTY(BlueprintReadOnly) bool Interactive=false;
 UPROPERTY() TMap<FString,FSvgCanvasRaster> BaseRasters;
 UPROPERTY() TObjectPtr<USvgCanvasDocument> ImportedAssets;
};

// Presentation adapter for UICreator's exported nodes, geometry and PNG brushes.
// Combat, visibility policy and pickup movement remain in AngelScript.
UCLASS()
class DUNGEONWORLDOPUS_API UOpusWorldUI : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) static UOpusWorldScreen* Create(APlayerController* Player,const FString& Template);
 UFUNCTION(BlueprintCallable) static UOpusWorldScreen* CreateTemplate(APlayerController* Player,const FString& Template,const FString& ContentDirectory,int32 Layer=120,bool Viewport=true);
 UFUNCTION(BlueprintCallable) static void SetHover(UOpusWorldScreen* Screen,const FString& Binding,const FString& Id);
 UFUNCTION(BlueprintCallable) static void Emphasize(UOpusWorldScreen* Screen,const FString& Binding,bool Enabled);
 UFUNCTION(BlueprintCallable) static void CopyBrush(UOpusWorldScreen* Target,const FString& Binding,UOpusWorldScreen* Source,const FString& SourceId);
 UFUNCTION(BlueprintCallable) static void MountRows(UOpusWorldScreen* Screen,const FString& Binding,const TArray<UOpusWorldScreen*>& Rows);
 UFUNCTION(BlueprintCallable) static void FitLabel(UOpusWorldScreen* Screen,const FString& Binding,const FString& Text,float Padding=18);
 UFUNCTION(BlueprintCallable) static void CopyTextColor(UOpusWorldScreen* Target,const FString& Binding,UOpusWorldScreen* Source,const FString& SourceBinding);
 UFUNCTION(BlueprintPure) static bool VisualReady(UOpusWorldScreen* Screen);
 UFUNCTION(BlueprintCallable) static void Fill(UOpusWorldScreen* Screen,const FString& Binding,float Ratio);
 UFUNCTION(BlueprintCallable) static bool Place(UOpusWorldScreen* Screen,APlayerController* Player,FVector Location,FVector2D Offset);
 UFUNCTION(BlueprintCallable) static void PlaceLabels(APlayerController* Player,const TArray<UOpusWorldScreen*>& Screens,const TArray<FVector>& Locations);
 UFUNCTION(BlueprintPure) static bool PointerBlocked(APlayerController* Player);
 UFUNCTION(BlueprintPure) static FVector HeadAnchor(USkeletalMeshComponent* Mesh);
 UFUNCTION(BlueprintPure) static double PresentationSeed(const FString& Key);
};
