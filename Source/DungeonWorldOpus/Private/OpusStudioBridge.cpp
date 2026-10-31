#include "OpusStudioBridge.h"
#include "OpusGameplayState.h"
#include "DWRuleStateReader.h"
#include "DWRuleShop.h"
#include "DWRulePawnEquipment.h"
using namespace DWRules;
int64 UOpusStudioBridge::UnixSeconds(){return FDateTime::UtcNow().ToUnixTimestamp();}
FString UOpusStudioBridge::UniqueId(){return FGuid::NewGuid().ToString(EGuidFormats::Digits);}
TArray<FOpusEntry> UOpusStudioBridge::FixedTemplates(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusEntry> Out;if(!R)return Out;
 for(const auto& L:FDWShopRules::Listings(R->Library()))if(L.Kind==TEXT("equipment"))
  if(const auto* D=R->Library().Equipment.Find(L.Id);D&&D->Legendary.IsSet())
  {FOpusEntry E;E.Id=L.Id.ToString();E.Name=L.Name;E.Price=L.Price;Out.Add(E);}
 Out.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});return Out;
}
bool UOpusStudioBridge::Equipment(UDWRuleWorldSubsystem* R,FName Base,FName Rarity,int32 Tier,FName Affix,int32 Seed,const FString& Uid,const FString& Name,FString& Payload,FString& Error)
{
 if(!R||Uid.IsEmpty()||Tier<1||Tier>TraitTierCount)return false;
 const auto* D=R->Library().Equipment.Find(Base);if(!D){Error=TEXT("装备模板不存在");return false;}
 FDWEquipmentRandom Random{uint32(Seed)};auto Roll=[&](){return Random.Next();};FDWEquipmentInstance Item;
 if(!FDWEquipmentRules::Roll(R->Library(),*D,Rarity,Roll,Uid,Item,Error,Tier))return false;
 if(!Affix.IsNone())
 {
  const auto Pool=FDWEquipmentRules::Pool(R->Library());const auto* A=Pool.FindByPredicate([&](const auto& V){return V.Id==Affix;});FDWEquipmentAffix Fixed;
  if(!A||!FDWEquipmentRules::RollAffix(R->Library(),*A,Roll,Fixed,Error,Tier)){if(Error.IsEmpty())Error=TEXT("特质词缀不存在");return false;}
  Item.Affixes={Fixed};
 }
 if(!Name.IsEmpty())Item.Name=Name;
 FDWRuleProfile Box;Box.bInitialized=true;Box.Loadout.Instances.Add(Item);Payload=FDWRuleStateReader::Write(Box,R->Library());return true;
}
int64 UOpusStudioBridge::Price(UDWRuleWorldSubsystem* R,FName Base,const FString& Payload)
{
 if(!R)return -1;const auto* Custom=R->Library().Shop.Prices.Find(TEXT("equipment:")+Base.ToString());if(Custom)return *Custom;
 FDWRuleProfile Box;FString Error;if(!FDWRuleStateReader::Read(Payload,R->Library(),Box,Error)||Box.Loadout.Instances.Num()!=1)return -1;
 const auto D=FDWEquipmentRules::Document(Box.Loadout.Instances[0],&R->Library());
 return FDWShopRules::DefaultPrice(TEXT("equipment"),D.Rarity,D.Category,D.Level,D.Traits.Num());
}
bool UOpusStudioBridge::Purchase(UDWRuleWorldSubsystem* R,const FString& Payload,int64 Cost,FString& Error)
{
 if(!R||Cost<0){Error=TEXT("商品价格无效");return false;}
 const auto Pools=R->GetPlayerPools();auto Before=R->PlayerInventory();
 if(Before.Stones<Cost||Before.Spent>MAX_int64-Cost){Error=TEXT("灵石不足或消费记录无效");return false;}
 if(!Payload.IsEmpty()&&!UOpusGameplayStore::Grant(R,Payload,Error))return false;
 auto Next=R->PlayerInventory();Next.Stones-=Cost;Next.Spent+=Cost;
 if(!R->ImportPlayerState(FDWRuleStateReader::Write(Next,R->Library()),Error))
 {FString Ignored;R->ImportPlayerState(FDWRuleStateReader::Write(Before,R->Library()),Ignored);return false;}
 auto* S=R->PlayerSession();S->Character.Life=FMath::Min(Pools.Life,S->Character.MaxLife);S->Character.Shield=FMath::Min(Pools.Shield,S->Character.ShieldCap);S->Character.Mana=FMath::Min(Pools.Mana,S->Character.ManaCapacity());return true;
}
