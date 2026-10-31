#include "OpusTalentBridge.h"
#include "DWRuleWorldSubsystem.h"
#include "DWRuleProgramReader.h"
#include "DWRuleSession.h"
#include "SvgCanvasScreen.h"
#include "SvgCanvasNode.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
namespace {
using FObject=TSharedPtr<FJsonObject>;
FString Str(const FObject& O,const TCHAR* K){FString V;O->TryGetStringField(K,V);return V;}
FObject LoadCatalog(){FString S;FObject O;FFileHelper::LoadFileToString(S,*(FPaths::ProjectContentDir()/TEXT("Rules/Talents/constellation.json")));FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O);return O;}
FOpusTalentRecord Record(const FObject& O){FOpusTalentRecord R;R.Id=Str(O,TEXT("id"));R.Name=Str(O,TEXT("name"));R.Kind=Str(O,TEXT("kind"));R.Description=Str(O,TEXT("description"));R.Requirement=Str(O,TEXT("requirement"));R.Category=Str(O,TEXT("category"));R.Sector=Str(O,TEXT("sector"));R.Color=Str(O,TEXT("color"));const FObject* B=nullptr;if(O->TryGetObjectField(TEXT("sourceBuild"),B)){R.SourceBuild=Str(*B,TEXT("id"));R.SourceName=Str(*B,TEXT("name"));}const TArray<TSharedPtr<FJsonValue>>* Sources=nullptr;if(O->TryGetArrayField(TEXT("sources"),Sources))for(const auto& V:*Sources){const auto P=V->AsObject();R.Sources+=Str(P,TEXT("path"));if(Str(P,TEXT("path")).IsEmpty())R.Sources+=TEXT("Presets/")+Str(P,TEXT("preset"))+TEXT(".json · ")+Str(P,TEXT("slot"))+TEXT(" / ")+Str(P,TEXT("affix"));R.Sources+=TEXT("\n");}double V;if(O->TryGetNumberField(TEXT("value"),V))R.Value=int32(V);if(O->TryGetNumberField(TEXT("cost"),V))R.Cost=int32(V);if(O->TryGetNumberField(TEXT("x"),V))R.Position.X=V;if(O->TryGetNumberField(TEXT("y"),V))R.Position.Y=V;const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(O->TryGetArrayField(TEXT("traits"),A))FJsonSerializer::Serialize(*A,TJsonWriterFactory<>::Create(&R.Traits));return R;}
FString StatePath(){return FPaths::ProjectSavedDir()/TEXT("Opus/talent-constellation.txt");}
}
TArray<FOpusTalentRecord> UOpusTalentBridge::Catalog(){TArray<FOpusTalentRecord> Out;const auto O=LoadCatalog();if(!O)return Out;TMap<FString,int32> Index;for(const auto& V:O->GetArrayField(TEXT("nodes"))){auto R=Record(V->AsObject());for(const auto& Sector:O->GetArrayField(TEXT("sectors"))){const auto SO=Sector->AsObject();if(Str(SO,TEXT("id"))==R.Sector){R.SectorName=Str(SO,TEXT("name"));R.Color=Str(SO,TEXT("color"));}}Index.Add(R.Id,Out.Num());Out.Add(R);}for(const auto& V:O->GetArrayField(TEXT("edges"))){const auto& E=V->AsArray();if(E.Num()!=2)continue;const auto* A=Index.Find(E[0]->AsString());const auto* B=Index.Find(E[1]->AsString());if(A&&B){Out[*A].Neighbors.Add(*B);Out[*B].Neighbors.Add(*A);}}return Out;}
TArray<FOpusTalentRecord> UOpusTalentBridge::Attributes(){TArray<FOpusTalentRecord> Out;const auto O=LoadCatalog();if(O)for(const auto& V:O->GetArrayField(TEXT("attributes")))Out.Add(Record(V->AsObject()));return Out;}
FString UOpusTalentBridge::TraitError(UDWRuleWorldSubsystem* Rules,const FString& Text){FString Error;if(!Rules)return TEXT("no rule host");TArray<TSharedPtr<FJsonValue>> Entries;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Entries))return TEXT("invalid trait entries");for(const auto& V:Entries){DWRules::FDWTraitEntry E;DWRules::FDWTraitSource Checked;if(!FDWRuleProgramReader::ReadTraitEntry(V,E,Error)||!Rules->Library().SourceFromEntry(E,Checked,Error))return Error;}return Error;}
bool UOpusTalentBridge::ApplyParts(UDWRuleWorldSubsystem* Rules,const TArray<FString>& Ids,const TArray<FString>& Traits,FString& Error){if(!Rules||Ids.Num()!=Traits.Num()){Error=TEXT("invalid host collections");return false;}TArray<DWRules::FDWCollectionPart> Parts;for(int32 I=0;I<Ids.Num();I++){TArray<TSharedPtr<FJsonValue>> Entries;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Traits[I]),Entries)){Error=TEXT("invalid host trait entries");return false;}DWRules::FDWCollectionPart P;P.Collection.Id=FName(*(TEXT("talent_")+Ids[I]));P.Collection.Name=Ids[I];P.Label=TEXT("星盘 · ")+Ids[I];P.bEquipment=false;for(const auto& V:Entries){DWRules::FDWTraitEntry E;DWRules::FDWTraitSource Checked;if(!FDWRuleProgramReader::ReadTraitEntry(V,E,Error)||!Rules->Library().SourceFromEntry(E,Checked,Error))return false;P.Collection.Traits.Add(MoveTemp(E));}Parts.Add(MoveTemp(P));}return Rules->SetPlayerPassiveParts(Parts,Error);}
FString UOpusTalentBridge::ReadState(){FString S;FFileHelper::LoadFileToString(S,*StatePath());return S;}
bool UOpusTalentBridge::WriteState(const FString& S){const FString P=StatePath();IFileManager::Get().MakeDirectory(*FPaths::GetPath(P),true);const FString Temp=P+TEXT(".tmp");return FFileHelper::SaveStringToFile(S,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)&&IFileManager::Get().Move(*P,*Temp,true,true);}
void UOpusTalentBridge::CopyRaster(USvgCanvasScreen* S,const FString& Target,const FString& Source){auto* T=S?S->FindSourceNode(Target):nullptr;auto* F=S?S->FindSourceNode(Source):nullptr;if(!T||!F)return;const FVector2D Scale(T->DesignRect.Z/FMath::Max(1.,F->DesignRect.Z),T->DesignRect.W/FMath::Max(1.,F->DesignRect.W));T->Normal=F->Normal;T->Normal.PixelOrigin=FVector2D(T->DesignRect.X,T->DesignRect.Y)+(F->Normal.PixelOrigin-FVector2D(F->DesignRect.X,F->DesignRect.Y))*Scale;T->Normal.PixelSize*=Scale;T->Hovered=T->Pressed=T->Disabled=T->Normal;T->ApplyRaster(T->Normal);}
void UOpusTalentBridge::Tint(USvgCanvasScreen* S,const FString& Id,FLinearColor C){if(auto* N=S?S->FindSourceNode(Id):nullptr)if(N->Surface)N->Surface->SetColorAndOpacity(C);}
void UOpusTalentBridge::PlaceNode(USvgCanvasScreen* S,const FString& Id,FVector2D P){if(auto* N=S?S->FindSourceNode(Id):nullptr)if(auto* C=Cast<UCanvasPanelSlot>(N->Slot))C->SetPosition(P);}
void UOpusTalentBridge::Clip(USvgCanvasScreen* S,const FString& Id){if(auto* N=S?S->FindSourceNode(Id):nullptr)N->SetClipping(EWidgetClipping::ClipToBoundsAlways);}
void UOpusTalentBridge::SmoothRaster(USvgCanvasScreen* S,const FString& Id){if(auto* N=S?S->FindSourceNode(Id):nullptr)if(auto* T=N->Normal.Texture.Get();T&&T->Filter!=TF_Bilinear){T->Filter=TF_Bilinear;T->UpdateResource();}}
double UOpusTalentBridge::Stat(UDWRuleWorldSubsystem* R,FName Key){return R&&R->PlayerSession()?R->PlayerSession()->Character.Sheet.Stat(Key):0;}

void UOpusTalentBridge::FitScreen(USvgCanvasScreen* S,APlayerController* P){if(!S||!P)return;int32 X,Y;P->GetViewportSize(X,Y);if(X<=0||Y<=0)return;const float DPI=UWidgetLayoutLibrary::GetViewportScale(P),Scale=FMath::Min(X/1600.f,Y/900.f)/FMath::Max(.01f,DPI);S->SetDesiredSizeInViewport(FVector2D(1600,900));S->SetRenderTransformPivot(FVector2D::ZeroVector);S->SetRenderScale(FVector2D(Scale,Scale));S->SetPositionInViewport(FVector2D((X-1600*Scale*DPI)/2,(Y-900*Scale*DPI)/2));}

FVector2D UOpusTalentBridge::Pointer(USvgCanvasScreen* S,APlayerController* P){float X=0,Y=0;if(!S||!P||!P->GetMousePosition(X,Y))return FVector2D(800,450);const auto View=UWidgetLayoutLibrary::GetViewportWidgetGeometry(P);const float DPI=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(P));return S->GetCachedGeometry().AbsoluteToLocal(View.LocalToAbsolute(FVector2D(X,Y)/DPI));}
