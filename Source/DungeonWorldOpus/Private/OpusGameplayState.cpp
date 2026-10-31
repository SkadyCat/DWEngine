#include "OpusGameplayState.h"
#include "DWRuleStateReader.h"
#include "DWRulePawnEquipment.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
using namespace DWRules;
namespace {
bool Read(UDWRuleWorldSubsystem* R,const FString& J,FDWRuleProfile& P,FString& E){return R&&FDWRuleStateReader::Read(J,R->Library(),P,E);}
FString Write(UDWRuleWorldSubsystem* R,const FDWRuleProfile& P){return FDWRuleStateReader::Write(P,R->Library());}
bool Commit(UDWRuleWorldSubsystem* R,FDWRuleProfile P,FString& E)
{const auto Pools=R->GetPlayerPools();if(!R->ImportPlayerState(Write(R,P),E))return false;auto* S=R->PlayerSession();S->Character.Life=FMath::Min(Pools.Life,S->Character.MaxLife);S->Character.Shield=FMath::Min(Pools.Shield,S->Character.ShieldCap);S->Character.Mana=FMath::Min(Pools.Mana,S->Character.ManaCapacity());return true;}
}
bool UOpusGameplayStore::Decode(const FString& Profile,FOpusGameplayState& State,FString& Error)
{
 State=FOpusGameplayState();if(Profile.IsEmpty())return true;TSharedPtr<FJsonObject> O;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Profile),O)||!O){Error=TEXT("玩法存档JSON无效");return false;}
 if(!O->HasField(TEXT("opusGameplay")))return true;const TSharedPtr<FJsonObject>* Data=nullptr;
 if(!O->TryGetObjectField(TEXT("opusGameplay"),Data)||!FJsonObjectConverter::JsonObjectToUStruct((*Data).ToSharedRef(),&State,0,0)||State.Version!=1||State.AuraSlots.Num()>4||State.AuraEnabled.Num()>4||State.RewardCandidates.Num()>3){Error=TEXT("玩法存档结构无效");return false;}
 TSet<FString> Keys;for(const auto* Rows:{&State.Stash,&State.Goods})for(const auto& S:*Rows)
 {if(S.Key.IsEmpty()||S.Payload.IsEmpty()||S.Count<1||S.Cell<0||S.Cell>100000||Keys.Contains(S.Key)){Error=TEXT("仓库物品记录无效");return false;}Keys.Add(S.Key);}return true;
}
FString UOpusGameplayStore::Encode(const FString& Profile,const FOpusGameplayState& State)
{
 TSharedPtr<FJsonObject> O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Profile),O)||!O)return TEXT("");auto Data=MakeShared<FJsonObject>();
 if(!FJsonObjectConverter::UStructToJsonObject(FOpusGameplayState::StaticStruct(),&State,Data,0,0))return TEXT("");O->SetObjectField(TEXT("opusGameplay"),Data);FString Out;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Out));return Out;
}
bool UOpusGameplayStore::RollEquipment(UDWRuleWorldSubsystem* R,FName Slot,int32 Tier,int32 Count,int32 Seed,TArray<FString>& Candidates,FString& Error)
{
 if(!R||Count<1||Count>32||Tier<1||Tier>TraitTierCount)return false;TArray<const FDWCollection*> Pool;
 for(const auto& Pair:R->Library().Equipment)if(Pair.Value.Slot==Slot&&Pair.Value.Rarity!=TEXT("legendary"))Pool.Add(&Pair.Value);
 Pool.Sort([](const auto& A,const auto& B){return A.Id.LexicalLess(B.Id);});if(Pool.Num()<Count){Error=TEXT("装备候选种类不足");return false;}
 FDWEquipmentRandom Random{uint32(Seed)};TArray<FString> Next;
 for(int32 I=0;I<Count;++I){const int32 At=FMath::Min(Pool.Num()-1,int32(Random.Next()*Pool.Num()));const auto* D=Pool[At];Pool.RemoveAt(At);FDWEquipmentInstance Item;
  const FName Rarity=FDWEquipmentRules::RandomRarity([&](){return Random.Next();});
  if(!FDWEquipmentRules::Roll(R->Library(),*D,Rarity,[&](){return Random.Next();},TEXT("reward_")+FGuid::NewGuid().ToString(EGuidFormats::Digits),Item,Error,Tier))return false;
  FDWRuleProfile Box;Box.bInitialized=true;Box.Loadout.Instances.Add(MoveTemp(Item));Next.Add(Write(R,Box));}
 Candidates=MoveTemp(Next);return true;
}
FString UOpusGameplayStore::SkillIcon(FName Id){return FPaths::ProjectContentDir()/TEXT("OpusIcons/skill")/(Id.ToString()+TEXT(".png"));}
FOpusEntry UOpusGameplayStore::Entry(UDWRuleWorldSubsystem* R,const FString& Payload)
{
 FOpusEntry E;FDWRuleProfile P;FString Error;if(!Read(R,Payload,P,Error))return E;
 if(P.Loadout.Instances.Num()==1){const auto& I=P.Loadout.Instances[0];E.Id=I.Uid;E.Key=TEXT("equipment:")+I.Uid;E.Kind=TEXT("equipment");E.Name=I.Name;E.Slot=I.Slot.ToString();E.Rarity=I.Rarity.ToString();E.Tier=I.Tier;E.Count=1;E.Identified=I.bIdentified;E.Synthesized=I.bSynthesized;E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/equipment")/(I.BaseId.ToString()+TEXT(".png"));const FString Masked=FPaths::ProjectContentDir()/TEXT("OpusIconsMasked/equipment")/(I.BaseId.ToString()+TEXT(".png"));if(FPaths::FileExists(Masked))E.Icon=Masked;}
 else if(P.Loadout.Owned.Num()==1){for(const auto& Pair:P.Loadout.Owned)if(const auto* D=R->Library().Items.Find(Pair.Key)){E.Id=Pair.Key.ToString();E.Key=TEXT("item:")+E.Id;E.Kind=TEXT("item");E.Name=D->Name;E.Description=D->Description;E.Count=Pair.Value;E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/item")/(E.Id+TEXT(".png"));}}
 else if(P.OwnedEquipment.Num()==1){for(const auto& Pair:P.OwnedEquipment)if(const auto* D=R->Library().Equipment.Find(Pair.Key)){E.Id=Pair.Key.ToString();E.Key=TEXT("equipment:")+E.Id;E.Kind=TEXT("equipment");E.Name=D->Name;E.Count=Pair.Value;E.Identified=false;E.Rarity=D->Rarity.ToString();E.Tier=D->Tier;E.Slot=D->Slot.ToString();E.Icon=FPaths::ProjectContentDir()/TEXT("OpusIcons/equipment")/(E.Id+TEXT(".png"));}}
 return E;
}
TArray<FOpusHoverLine> UOpusGameplayStore::Lines(UDWRuleWorldSubsystem* R,const FString& Payload)
{FDWRuleProfile P;FString E;if(!Read(R,Payload,P,E)||P.Loadout.Instances.Num()!=1)return {};return UOpusData::InstanceLines(P.Loadout.Instances[0],R->Library());}
bool UOpusGameplayStore::Grant(UDWRuleWorldSubsystem* R,const FString& Payload,FString& Error)
{
 FDWRuleProfile Box;if(!Read(R,Payload,Box,Error))return false;auto Next=R->PlayerInventory();
 for(const auto& I:Box.Loadout.Instances){if(Next.Loadout.Instances.ContainsByPredicate([&](const auto& N){return I.Uid==N.Uid;})){Error=TEXT("物品UID重复，操作已取消");return false;}Next.Loadout.Instances.Add(I);}
 for(const auto& I:Box.Loadout.Owned){if(int64(Next.Loadout.Owned.FindRef(I.Key))+I.Value>MAX_int32){Error=TEXT("物品数量达到上限");return false;}Next.Loadout.Owned.FindOrAdd(I.Key)+=I.Value;}
 for(const auto& I:Box.OwnedEquipment){if(int64(Next.OwnedEquipment.FindRef(I.Key))+I.Value>MAX_int32){Error=TEXT("装备数量达到上限");return false;}Next.OwnedEquipment.FindOrAdd(I.Key)+=I.Value;}
 return Commit(R,MoveTemp(Next),Error);
}
bool UOpusGameplayStore::Store(UDWRuleWorldSubsystem* R,const FString& Key,int32 Count,FOpusStoredStack& Stack,FString& Error)
{
 if(!R||Count<1)return false;auto Next=R->PlayerInventory();FDWRuleProfile Box;Box.bInitialized=true;
 if(Key.StartsWith(TEXT("equipment:"))){const FString Id=Key.Mid(10);auto* I=Next.Loadout.Instances.FindByPredicate([&](const auto& N){return N.Uid==Id;});
  if(I){if(Count!=1||R->IsPlayerItemLocked(Id)||Next.Loadout.Equipment.FindRef(I->Slot)==FName(*Id)){Error=TEXT("请先卸下装备并解除祭坛锁定");return false;}Box.Loadout.Instances.Add(*I);Next.Loadout.Instances.RemoveAll([&](const auto& N){return N.Uid==Id;});}
  else {const FName Name(*Id);for(const auto& Pair:Next.Loadout.Equipment)if(Pair.Value==Name){Error=TEXT("请先卸下装备");return false;}if(Next.OwnedEquipment.FindRef(Name)<Count){Error=TEXT("背包装备不足");return false;}Box.OwnedEquipment.Add(Name,Count);Next.OwnedEquipment.FindOrAdd(Name)-=Count;}}
 else if(Key.StartsWith(TEXT("item:"))){const FName Id(*Key.Mid(5));if(!R->Library().Items.Contains(Id)||Next.Loadout.Owned.FindRef(Id)<Count){Error=TEXT("背包物品不足");return false;}Box.Loadout.Owned.Add(Id,Count);Next.Loadout.Owned.FindOrAdd(Id)-=Count;}
 else {Error=TEXT("此物品不可存放");return false;}
 Stack.Ref=Key;Stack.Count=Count;Stack.Payload=Write(R,Box);Stack.Key=TEXT("stored_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 // Remove ownership from its existing cells without relocating the rest of the bag.
 int32 Left=Count;for(auto& S:Next.BagLayout.Slots)if(S.Ref==Key&&Left>0){const int32 N=FMath::Min(S.Count,Left);S.Count-=N;Left-=N;if(S.Count<=0)S=FDWInventoryStackPlacement();}
 return Commit(R,MoveTemp(Next),Error);
}
bool UOpusGameplayStore::Restore(UDWRuleWorldSubsystem* R,const FOpusStoredStack& Stack,FString& Error)
{const auto E=Entry(R,Stack.Payload);if(E.Count!=Stack.Count||E.Key!=Stack.Ref){Error=TEXT("仓库物品内容与记录不符");return false;}return Grant(R,Stack.Payload,Error);}

TArray<FOpusStoredStack> UOpusGameplayStore::InventoryCells(UDWRuleWorldSubsystem* R)
{TArray<FOpusStoredStack> Out;if(R)for(const auto& C:R->PlayerInventory().BagLayout.Slots){FOpusStoredStack S;S.Key=C.Key;S.Ref=C.Ref;S.Count=C.Count;S.Cell=Out.Num();Out.Add(S);}return Out;}
int32 UOpusGameplayStore::InventoryNextKey(UDWRuleWorldSubsystem* R){return R?R->PlayerInventory().BagLayout.NextKey:1;}
bool UOpusGameplayStore::SetInventoryCells(UDWRuleWorldSubsystem* R,const TArray<FOpusStoredStack>& Cells,int32 NextKey,FString& Error)
{
 if(!R||Cells.Num()>100000||NextKey<1){Error=TEXT("无效的背包布局");return false;}
 FDWInventoryLayout Next;Next.NextKey=NextKey;TSet<FString> Keys;
 for(const auto& C:Cells){FDWInventoryStackPlacement S;if(!C.Ref.IsEmpty()){if(C.Key.IsEmpty()||C.Count<1||Keys.Contains(C.Key)){Error=TEXT("无效的背包堆叠");return false;}Keys.Add(C.Key);S.Key=C.Key;S.Ref=C.Ref;S.Count=C.Count;}Next.Slots.Add(S);}
 R->PlayerInventoryLayout()=MoveTemp(Next);return true;
}
