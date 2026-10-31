#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "OpusDropService.generated.h"
class UDWRuleWorldSubsystem;

// Transport/catalogue boundary only. Rule evaluation stays in DWRulesCore.
UCLASS(ClassGroup=(Opus),meta=(BlueprintSpawnableComponent))
class DUNGEONWORLDOPUS_API UOpusDropService : public UActorComponent
{
 GENERATED_BODY()
public:
 UOpusDropService();
 UPROPERTY(BlueprintReadOnly) bool Initialized=false;
 UPROPERTY(BlueprintReadOnly) bool Ready=false;
 UPROPERTY(BlueprintReadOnly) bool Failed=false;
 UPROPERTY(BlueprintReadOnly) bool Offline=false;
 UPROPERTY(BlueprintReadOnly) FString Error;
 UPROPERTY(BlueprintReadOnly) FString CatalogVersion;
 UPROPERTY(BlueprintReadOnly) FString PoolVersion;
 UPROPERTY(BlueprintReadOnly) FString EventConfigJson;
 UPROPERTY(BlueprintReadOnly) TArray<FName> EquipmentIds;
 UPROPERTY(BlueprintReadOnly) int32 Requests=0;
 UPROPERTY(BlueprintReadOnly) int32 EquipmentCount=0;
 UPROPERTY(BlueprintReadOnly) int32 ItemCount=0;
 UFUNCTION(BlueprintCallable) void Initialize(UDWRuleWorldSubsystem* WorldRules);
 UFUNCTION(BlueprintCallable) void RequestPool(FName Core,int32 Floor,int32 Tier,const FString& MapItemId=TEXT(""));
 UFUNCTION(BlueprintCallable) void Retry();
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<UDWRuleWorldSubsystem> Rules;
 TSharedPtr<IHttpRequest,ESPMode::ThreadSafe> Pending;
 FString BaseUrl,ActiveMapItem;
 FName MapCore;
 int32 MapFloor=1,MaxTier=1,Generation=0,Attempts=0;
 double RetryAt=0;
 void Send();
 void Rejected(const FString& Reason,bool Transient);
 bool Apply(const FString& Json,FString& Reason);
 static FString LocalVersion();
};
