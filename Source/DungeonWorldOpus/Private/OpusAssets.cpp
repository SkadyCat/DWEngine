#include "OpusAssets.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "SvgCanvasScreen.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
UOpusAssetCache* Cache(UObject* Context)
{
 auto* W=GEngine?GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
 auto* GI=W?W->GetGameInstance():nullptr;return GI?GI->GetSubsystem<UOpusAssetCache>():nullptr;
}
bool Compiling(UObject* Object)
{
 if(auto* Mesh=Cast<USkinnedAsset>(Object);Mesh&&Mesh->IsCompiling())return true;
 if(auto* Mesh=Cast<USkeletalMesh>(Object))for(const auto& Slot:Mesh->GetMaterials())if(Slot.MaterialInterface&&Slot.MaterialInterface->IsCompiling())return true;
 return false;
}
}
void UOpusAssetCache::Deinitialize(){Loading.Empty();Queue.Empty();Objects.Empty();HUD=nullptr;Super::Deinitialize();}
void UOpusAssets::Request(UObject* Context,const TArray<FString>& Paths)
{
 if(auto* C=Cache(Context))for(const auto& P:Paths)if(!P.IsEmpty()&&!C->Objects.Contains(P)&&!C->Loading.Contains(P)&&!C->Failed.Contains(P))C->Queue.AddUnique(P);
}
void UOpusAssets::Pump(UObject* Context)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(Opus_AssetPump);
 auto* C=Cache(Context);if(!C||C->LastPump==GFrameCounter)return;C->LastPump=GFrameCounter;
 TArray<FString> Done;
 for(const auto& It:C->Loading)if(!It.Value.IsValid()||It.Value->HasLoadCompleted())
 {
  if(auto* O=FSoftObjectPath(It.Key).ResolveObject())C->Objects.Add(It.Key,O);else C->Failed.Add(It.Key);
  Done.Add(It.Key);
 }
 for(const auto& P:Done)C->Loading.Remove(P);
 // One new package request per frame, at most two in flight. Never FlushAsyncLoading.
 if(C->Loading.Num()<2&&!C->Queue.IsEmpty())
 {
  FString P=C->Queue[0];C->Queue.RemoveAt(0);
  C->Loading.Add(P,C->Streamer.RequestAsyncLoad(FSoftObjectPath(P),FStreamableDelegate(),FStreamableManager::DefaultAsyncLoadPriority));
 }
}
bool UOpusAssets::Ready(UObject* Context,const TArray<FString>& Paths)
{
 auto* C=Cache(Context);if(!C)return false;
 for(const auto& P:Paths){auto* O=C->Objects.Find(P);if(!O||!O->Get()||Compiling(O->Get()))return false;}return true;
}
FString UOpusAssets::Error(UObject* Context,const TArray<FString>& Paths)
{
 auto* C=Cache(Context);if(!C)return TEXT("Missing game-instance asset cache");
 for(const auto& P:Paths)if(C->Failed.Contains(P))return TEXT("Failed to stream ")+P;return FString();
}
UObject* UOpusAssets::Find(UObject* Context,const FString& Path)
{
 auto* C=Cache(Context);auto* O=C?C->Objects.Find(Path):nullptr;return O?O->Get():nullptr;
}
UClass* UOpusAssets::HUDClass(UObject* Context)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(Opus_GetHUDClass);
 auto* C=Cache(Context);if(!C)return nullptr;
 if(!C->HUD)C->HUD=LoadClass<USvgCanvasScreen>(nullptr,TEXT("/Game/UICreator/WBP_CurrentCanvas.WBP_CurrentCanvas_C"));
 return C->HUD;
}
