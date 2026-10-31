#include "DWRuleFloorRewards.h"
#include "DWRulePawnEquipment.h"

namespace DWRules
{
namespace
{
int32 Choice(int32 Num, FDWEquipmentRandom& Random) { return FMath::Min(Num - 1, int32(FMath::FloorToDouble(Random.Next() * Num))); }
bool ItemCategory(FName Id) { return Id == TEXT("consumable") || Id == TEXT("currency") || Id == TEXT("omen") || Id == TEXT("material"); }
bool HasFixed(const FDWCollection& D) { return D.Legendary.IsSet() || D.Traits.Num() > 0; }
FDWFloorLoot EquipmentLoot(const FDWEquipmentInstance& Item)
{
 FDWFloorLoot L; L.Kind=TEXT("equipment"); L.Id=FName(*Item.Uid); L.Name=Item.Name; L.Rarity=Item.Rarity; L.Tier=Item.Tier; L.Item=Item; return L;
}
FDWFloorLoot ItemLoot(const FDWCollection& D, int32 Count=1)
{
 FDWFloorLoot L; L.Kind=TEXT("item"); L.Id=D.Id; L.Name=D.Name.IsEmpty()?D.Id.ToString():D.Name; L.Rarity=D.Rarity; L.Category=D.Category; L.Description=D.Description; L.Count=Count; return L;
}
// Preserve the web acquisition roll order: chance, selection, rarity (unless forced), tier.
bool RollDrop(const FDWRuleLibrary& Library, FDWEquipmentRandom& Random, double Chance, int32 MaxTier,
 const FString& Uid, FDWEquipmentInstance& Out, FName Only=NAME_None, bool bIdentified=false)
{
 if(Random.Next()>=Chance) return false;
 const double Selection=Random.Next();
 const FName Rarity=Only.IsNone()?FDWEquipmentRules::RandomRarity([&](){return Random.Next();}):FName(TEXT("legendary"));
 TArray<const FDWCollection*> Templates;
 for(const auto& Pair:Library.Equipment) if(!FDWPawnEquipmentRules::IsFixedSlot(Pair.Value.Slot) &&
  (Only.IsNone() || Pair.Key==Only) && (Pair.Value.Rarity!=TEXT("legendary") || Rarity==TEXT("legendary"))) Templates.Add(&Pair.Value);
 if(Templates.IsEmpty()) return false;
 const auto& D=*Templates[FMath::Min(Templates.Num()-1,int32(FMath::FloorToDouble(Selection*Templates.Num())))];
 const int32 Tier=1+Choice(MaxTier,Random);
 if(bIdentified) { FString Error; return FDWEquipmentRules::Roll(Library,D,Rarity,[&](){return Random.Next();},Uid,Out,Error,Tier); }
 Out={}; Out.Uid=Uid; Out.BaseId=D.Id; Out.Name=D.Name.IsEmpty()?D.Id.ToString():D.Name; Out.Slot=D.Slot; Out.Level=D.Level; Out.Rarity=Rarity; Out.Tier=Tier; Out.bIdentified=false;
 return true;
}
}
FDWRuleFloorRewards::FDWRuleFloorRewards(uint32 InSeed,int32 InFloor,int32 Visit,int32 InMaxTier)
 :Floor(InFloor),MaxTier(FMath::Clamp(InMaxTier,1,TraitTierCount)),Seed(InSeed+uint32(Visit)*0x45d9f3bu),Random(Seed^0x4c4f4f54u),MerchantRandom(Seed^0x4d455243u),StonesRandom(InSeed^0x53544f4eu) {}
int64 FDWRuleFloorRewards::KillStones(double MaxHealth,int32 TowerLevel)
{ return TowerLevel==1?5+Choice(6,StonesRandom):FMath::Max<int64>(1,int64(FMath::FloorToDouble(2+MaxHealth/40+.5))); }
FString FDWRuleFloorRewards::Uid(const TCHAR* Prefix) { return FString::Printf(TEXT("%s_%u_%d_%d"),Prefix,Seed,Floor,NextId++); }
int32 FDWRuleFloorRewards::Remaining() const { int32 N=0; for(const auto& L:Ground) if(!L.bClaimed) ++N; return N; }
void FDWRuleFloorRewards::OnKill(const FDWRuleLibrary& Library,FVector2d PositionCm,bool bBoss)
{
 TArray<FDWFloorLoot> Out; FDWEquipmentInstance Item;
 const FString DropId=Uid(TEXT("drop"));
 if(RollDrop(Library,Random,bBoss?1:EquipmentChance,MaxTier,DropId,Item)) Out.Add(EquipmentLoot(Item));
 TArray<const FDWCollection*> Items; for(const auto& P:Library.Items) if(ItemCategory(P.Value.Category)) Items.Add(&P.Value);
 if(Items.Num() && Random.Next()<(bBoss?1:ItemChance))
 { const auto& D=*Items[Choice(Items.Num(),Random)]; Out.Add(ItemLoot(D,(D.Category==TEXT("currency")||D.Category==TEXT("material"))?1+Choice(2,Random):1)); }
 for(int32 I=0;I<Out.Num();++I) { auto L=MoveTemp(Out[I]); L.Key=Uid(TEXT("ground")); const double Angle=NextId*2.3999632297, Distance=65+I*22;
  L.SourcePosition=PositionCm; L.Position=PositionCm+FVector2d(FMath::Cos(Angle),FMath::Sin(Angle))*Distance; Ground.Add(MoveTemp(L)); }
}
bool FDWRuleFloorRewards::Grant(FDWRuleProfile& Profile,FDWFloorLoot& Loot,FString& Error)
{
 if(Loot.bClaimed) { Error=TEXT("已领取"); return false; }
 if(Loot.Kind==TEXT("equipment")) {
  if(Loot.Item.Uid.IsEmpty()&&Loot.Origin==TEXT("inventory")) {
   const int64 Next=int64(Profile.OwnedEquipment.FindRef(Loot.Id))+Loot.Count;
   if(Loot.Id.IsNone()||Loot.Count<1||Next>9999){Error=TEXT("装备数量无效或背包已达上限");return false;}
   Profile.OwnedEquipment.Add(Loot.Id,int32(Next));
  } else {
   if(Loot.Item.Uid.IsEmpty() || Profile.Loadout.Instances.ContainsByPredicate([&](const auto& I){return I.Uid==Loot.Item.Uid;})) { Error=TEXT("装备实例无效或已领取"); return false; }
   Profile.Loadout.Instances.Add(Loot.Item); if(!Loot.bMerchant && Loot.Origin!=TEXT("inventory") && Profile.bProgression) ++Profile.Drops;
  }
 }
 else if(Loot.Kind==TEXT("item")) { const int64 Next=int64(Profile.Loadout.Owned.FindRef(Loot.Id))+Loot.Count;
  if(Loot.Count<1 || Next>9999) { Error=TEXT("物品数量无效或背包已达上限"); return false; } Profile.Loadout.Owned.Add(Loot.Id,int32(Next)); }
 else { Error=TEXT("未知掉落类型"); return false; }
 Loot.bClaimed=true; return true;
}
bool FDWRuleFloorRewards::Pickup(const FString& Key,FDWRuleProfile& Profile,FVector2d HeroCm,bool bDead,FString& Error)
{
 auto* Loot=Ground.FindByPredicate([&](const auto& L){return L.Key==Key;});
 if(!Loot || Loot->bClaimed) { Error=TEXT("掉落已拾取或不存在"); return false; }
 if(bDead) { Error=TEXT("倒下时无法拾取"); return false; }
 if(FVector2d::Distance(HeroCm,Loot->Position)>PickupRadiusCm) { Error=TEXT("请先靠近掉落"); return false; }
 return Grant(Profile,*Loot,Error);
}
bool FDWRuleFloorRewards::DropInventory(const FDWRuleLibrary& Library,FDWRuleProfile& Profile,FName Kind,const FString& Id,int32 Count,FVector2d HeroCm,bool bDead,FString& Key,FString& Error)
{
 Key.Reset();Error.Reset();
 if(bDead){Error=TEXT("倒下时无法丢出物品");return false;}
 if(!FMath::IsFinite(HeroCm.X)||!FMath::IsFinite(HeroCm.Y)||Count<1){Error=TEXT("物品数量或地面位置无效");return false;}
 FDWFloorLoot Loot;Loot.Kind=Kind;Loot.Id=FName(*Id);Loot.Count=Count;Loot.Origin=TEXT("inventory");Loot.SourcePosition=HeroCm;
 const auto* Instance=Profile.Loadout.Instances.FindByPredicate([&](const auto& I){return I.Uid==Id;});
 if(Kind==TEXT("equipment"))
 {
  const auto* Definition=Library.Equipment.Find(Instance?Instance->BaseId:FName(*Id));
  const FName Slot=Instance?Instance->Slot:Definition?Definition->Slot:NAME_None;
  if(!Definition||FDWPawnEquipmentRules::IsFixedSlot(Slot)||!FDWPawnEquipmentRules::IsSlot(Slot,EPawnKind::Player)){Error=TEXT("固定装备不能丢到地面");return false;}
  if(Instance){
   if(Count!=1){Error=TEXT("装备实例数量无效");return false;}
   Loot=EquipmentLoot(*Instance);Loot.Origin=TEXT("inventory");Loot.SourcePosition=HeroCm;Profile.Discard(Id);
  }else{
   const int32 Owned=Profile.OwnedEquipment.FindRef(FName(*Id));if(Owned<Count){Error=TEXT("背包中的装备数量不足");return false;}
   Loot.Name=Definition->Name;Loot.Rarity=Definition->Rarity;Loot.Tier=Definition->Tier;
   if(Owned>Count)Profile.OwnedEquipment.Add(FName(*Id),Owned-Count);
   else{Profile.OwnedEquipment.Remove(FName(*Id));for(auto It=Profile.Loadout.Equipment.CreateIterator();It;++It)if(It.Value()==FName(*Id))It.RemoveCurrent();}
  }
 }else if(Kind==TEXT("item")){
  const auto* Definition=Library.Items.Find(FName(*Id));const int32 Owned=Profile.Loadout.Owned.FindRef(FName(*Id));
  if(!Definition||Owned<Count){Error=TEXT("背包中的物品数量不足");return false;}
  Loot=ItemLoot(*Definition,Count);Loot.Origin=TEXT("inventory");Loot.SourcePosition=HeroCm;
  if(Owned>Count)Profile.Loadout.Owned.Add(FName(*Id),Owned-Count);
  else{Profile.Loadout.Owned.Remove(FName(*Id));for(FName& Existing:Profile.Belt)if(Existing==FName(*Id))Existing=NAME_None;}
 }else{Error=TEXT("此类型不能丢到地面");return false;}
 Profile.Loadout.Validate(Library);Loot.Key=Uid(TEXT("ground"));Key=Loot.Key;
 const double Angle=NextId*2.3999632297;Loot.Position=HeroCm+FVector2d(FMath::Cos(Angle),FMath::Sin(Angle))*80;
 Ground.Add(MoveTemp(Loot));return true;
}
void FDWRuleFloorRewards::Complete(const FDWRuleLibrary& Library)
{
 if(bCompleted) return; bCompleted=true;
 auto& R=MerchantRandom; if(R.Next()>=MerchantProbability) return;
 const auto All=FDWShopRules::Listings(Library); TArray<FDWShopListing> Equipment,Items;
 for(const auto& L:All) { if(L.Kind==TEXT("equipment") && HasFixed(Library.Equipment[L.Id])) Equipment.Add(L);
  else if(L.Kind==TEXT("item") && ItemCategory(Library.Items[L.Id].Category)) Items.Add(L); }
 auto Available=Equipment;
 for(int32 I=0;I<2 && Equipment.Num();++I) { if(Available.IsEmpty()) Available=Equipment; const int32 Index=Choice(Available.Num(),R); const auto Listing=Available[Index]; Available.RemoveAt(Index);
  FDWEquipmentInstance Item; const FString Id=Uid(TEXT("merchant")); if(!RollDrop(Library,R,1,MaxTier,Id,Item,Listing.Id,true)) continue;
  auto L=EquipmentLoot(Item); L.Key=Uid(TEXT("offer")); L.bMerchant=true;
  const auto Doc=FDWEquipmentRules::Document(Item,&Library);
  L.Price=Item.Tier==1?20+Choice(31,R):(Listing.bCustom?Listing.Price:FDWShopRules::DefaultPrice(TEXT("equipment"),Doc.Rarity,Doc.Category,Doc.Level,Doc.Traits.Num())); Stock.Add(MoveTemp(L)); }
 for(int32 I=0;I<4 && Items.Num();++I) { const int32 Index=Choice(Items.Num(),R); const auto Listing=Items[Index]; Items.RemoveAt(Index);
  auto L=ItemLoot(Library.Items[Listing.Id]); L.Key=Uid(TEXT("offer")); L.bMerchant=true; L.Price=Listing.Price; Stock.Add(MoveTemp(L)); }
 bHasMerchant=Stock.Num()>0;
}
bool FDWRuleFloorRewards::Buy(const FString& Key,FDWRuleProfile& Profile,FString& Error)
{
 if(!bCompleted || !bHasMerchant) { Error=TEXT("当前没有商人"); return false; }
 auto* L=Stock.FindByPredicate([&](const auto& O){return O.Key==Key;}); if(!L || L->bClaimed) { Error=TEXT("货品已售出或不存在"); return false; }
 if(L->Price<0) { Error=TEXT("价格无效"); return false; } if(Profile.Stones<L->Price) { Error=FString::Printf(TEXT("灵石不足（还差 %lld）"),L->Price-Profile.Stones); return false; }
 if(!Grant(Profile,*L,Error)) return false; Profile.Stones-=L->Price; Profile.Spent+=L->Price; return true;
}
}
