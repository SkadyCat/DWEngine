#include "OpusInventoryNotices.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace {FString NoticePath(){return FPaths::ProjectSavedDir()/TEXT("RuleStudioInventoryNotices.json");}}
UOpusInventoryNotices* UOpusInventoryNotices::Create(bool Persistent)
{
 auto* State=NewObject<UOpusInventoryNotices>();State->SaveEnabled=Persistent;
 if(!Persistent)return State;
 FString Json;TSharedPtr<FJsonObject> Root;
 if(FFileHelper::LoadFileToString(Json,*NoticePath())&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)&&Root)
 {
  const TSharedPtr<FJsonObject>* Counts;const TArray<TSharedPtr<FJsonValue>>* Unseen;
  if(Root->TryGetObjectField(TEXT("counts"),Counts)&&Root->TryGetArrayField(TEXT("unseen"),Unseen))
  {
   for(const auto& Pair:(*Counts)->Values){double Count;if(Pair.Value->TryGetNumber(Count)&&Count>0&&Count<=MAX_int32)State->Counts.Add(FString(Pair.Key.ToView()),int32(Count));}
   for(const auto& Value:*Unseen){FString Id;if(Value->TryGetString(Id)&&State->Counts.Contains(Id))State->Unseen.Add(Id);}
   State->Initialized=true;
  }
 }
 return State;
}
void UOpusInventoryNotices::Observe(const TArray<FOpusEntry>& Entries)
{
 TMap<FString,int32> Next;
 for(const auto& E:Entries)if((E.Kind==TEXT("equipment")||E.Kind==TEXT("item"))&&E.Count>0)Next.Add(E.Id,E.Count);
 bool Changed=!Initialized||Next.Num()!=Counts.Num();
 for(const auto& Pair:Next){const int32 Before=Counts.FindRef(Pair.Key);if(Initialized&&Pair.Value>Before)Unseen.Add(Pair.Key);Changed|=Before!=Pair.Value;}
 for(auto It=Unseen.CreateIterator();It;++It)if(!Next.Contains(*It))It.RemoveCurrent();
 Counts=MoveTemp(Next);Initialized=true;if(Changed)Save();
}
bool UOpusInventoryNotices::MarkSeen(const FString& Id){if(!Unseen.Remove(Id))return false;Save();return true;}
void UOpusInventoryNotices::Save() const
{
 if(!SaveEnabled)return;
 auto Root=MakeShared<FJsonObject>(),Values=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Pending;
 for(const auto& Pair:Counts)Values->SetNumberField(Pair.Key,Pair.Value);
 for(const auto& Id:Unseen)Pending.Add(MakeShared<FJsonValueString>(Id));
 Root->SetObjectField(TEXT("counts"),Values);Root->SetArrayField(TEXT("unseen"),Pending);
 FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*NoticePath(),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
