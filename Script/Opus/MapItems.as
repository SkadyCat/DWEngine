// Map-item generation, ownership and entry policy. The native adapter only
// transports data and evaluates the existing map rule sheet.
class UDWOMapItems : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() FOpusMapItemState State;
 bool Ready=false;
 TArray<FOpusMapAffix> Pool;
 TArray<FOpusMapQuality> Qualities;
 bool Initialize(ADWOTowerGameMode T,FString Snapshot,FString& Error)
 {
  Tower=T;Pool=UOpusMapItems::AffixPool();Qualities=UOpusMapItems::Qualities();
  if(Pool.Num()<6){Error="地图专属词缀池不足 6 条";return false;}
  if(!UOpusMapItems::DecodeProfile(Snapshot,State,Error))return false;
  if(State.Version!=1||State.NextId<1){Error="地图物品存档版本或序号无效";return false;}
  TArray<FString> Seen;
  for(auto I:State.Items){if(!Validate(I,Error)||Seen.Contains(I.Uid)){if(Error.IsEmpty())Error="重复的地图物品";return false;}Seen.Add(I.Uid);}
  if(!State.Active.Uid.IsEmpty()&&(!Validate(State.Active,Error)||Seen.Contains(State.Active.Uid))){if(Error.IsEmpty())Error="已使用地图仍存在于背包";return false;}
  while(State.DeviceSlots.Num()<6)State.DeviceSlots.Add("");while(State.DeviceSlots.Num()>6)State.DeviceSlots.RemoveAt(State.DeviceSlots.Num()-1);
  if(!State.DeviceSlots[0].IsEmpty()&&!Owns(State.DeviceSlots[0]))State.DeviceSlots[0]="";
  // Modifier definitions are deliberately deferred; no fabricated effects.
  for(int I=1;I<6;I++)State.DeviceSlots[I]="";
  Ready=true;Publish();return true;
 }
 void Publish(){UOpusMapItems::Publish(Tower.Rules,State);}
 FString Snapshot(){FString Json=UOpusMapItems::EncodeProfile(Tower.Rules.ExportPlayerState(),State);return Tower.Recent.Ready?Tower.Recent.Encode(Json):Json;}
 bool Quality(FString Id,FOpusMapQuality& Out) const
 {for(auto Q:Qualities)if(Q.Id==Id){Out=Q;return true;}return false;}
 bool Definition(FString Id,FOpusEntry& Out) const
 {for(auto D:UOpusMapItems::Catalog(Tower.Rules))if(D.Id==Id){Out=D;return true;}return false;}
 bool Owns(FString Uid) const {for(auto I:State.Items)if(I.Uid==Uid)return true;return false;}
 bool Validate(FOpusMapItem Item,FString& Error) const
 {
  FOpusEntry D;FOpusMapQuality Q;
  if(Item.Uid.IsEmpty()||Item.Tier<1||Item.Tier>16||!Definition(Item.BaseId,D)||!Quality(Item.Rarity,Q)||Item.Tier!=D.Tier||Item.Rarity!=D.Rarity){Error="地图物品定义、层级或品质无效";return false;}
  if(Item.Affixes.Num()<Q.Min||Item.Affixes.Num()>Q.Max){Error="地图词缀数量不符合品质";return false;}
  TArray<FString> Seen;
  for(auto A:Item.Affixes){bool Found=false;for(auto P:Pool)if(P.Id==A.Id)Found=true;
   if(!Found||Seen.Contains(A.Id)||!Math::IsFinite(A.Value)){Error="地图词缀无效或重复";return false;}Seen.Add(A.Id);
  }return true;
 }
 bool Roll(FString BaseId,FOpusMapItem& Out,FString& Error)
 {
  FOpusEntry D;FOpusMapQuality Q;if(!Definition(BaseId,D)||!Quality(D.Rarity,Q)){Error="未知地图物品";return false;}
  if(Q.Min<0||Q.Max>Pool.Num()||Q.Min>Q.Max){Error="地图品质词缀范围无效";return false;}
  FRandomStream Stream;Stream.Initialize(State.Seed);auto Candidates=Pool;FOpusMapItem Item;Item.BaseId=D.Id;Item.Tier=D.Tier;Item.Rarity=D.Rarity;
  Item.Uid="map_item_"+State.NextId;while(Owns(Item.Uid)||Item.Uid==State.Active.Uid){State.NextId++;Item.Uid="map_item_"+State.NextId;}
  int Count=Stream.RandRange(Q.Min,Q.Max);
  for(int I=0;I<Count;I++){
   int At=Stream.RandRange(0,Candidates.Num()-1);auto A=Candidates[At];Candidates.RemoveAt(At);
   if(A.Id.IsEmpty()||A.Min>A.Max||!Math::IsFinite(A.Min)||!Math::IsFinite(A.Max)){Error="地图词缀范围无效";return false;}
   A.Value=Stream.RandRange(int(A.Min),int(A.Max));Item.Affixes.Add(A);
  }
  if(!Validate(Item,Error))return false;
  State.Seed=Stream.GetCurrentSeed();State.NextId++;Out=Item;return true;
 }
 bool Persist(FString& Error)
 {
  if(UOpusNative::Flag("DWOTransient"))return true;
  FString Json=Snapshot();if(Json.IsEmpty()||!UOpusNative::WriteProfile(Json)){Error="地图物品保存失败，操作已取消";return false;}return true;
 }
 bool Buy(FString Key,FString& Error)
 {
  if(!Key.StartsWith("item:")){Error="地图商品无效";return false;}
  auto Before=State;FString Player=Tower.Rules.ExportPlayerState();FOpusMapItem Item;
  if(!Roll(Key.Mid(5),Item,Error)){State=Before;return false;}
  if(!UOpusMapItems::PurchaseTemplate(Tower.Rules,Key,Error)){State=Before;return false;}
  State.Items.Add(Item);Publish();
  if(!Persist(Error)){FString Ignored;Tower.Rules.ImportPlayerState(Player,Ignored);State=Before;Publish();return false;}
  Tower.Hud.RefreshIn=0;Tower.Hud.RefreshPart=0;return true;
 }
 bool Use(FString Uid,FString& Error)
 {
  if(Tower.Traveling||Tower.Preparing||Tower.Run.Phase==EDWOTowerPhase::Waves||Tower.Run.Phase==EDWOTowerPhase::Boss||Tower.Rules.GetPlayerPools().bDead){Error="请在战斗结束后使用地图物品";return false;}
  int At=-1;for(int I=0;I<State.Items.Num();I++)if(State.Items[I].Uid==Uid){At=I;break;}
  if(At<0){Error="地图物品不存在或已使用";return false;}
  auto Item=State.Items[At];if(!Validate(Item,Error))return false;
  FOpusMapData Checked;if(!UOpusMapItems::Evaluate(Tower.Rules,UDWOTowerJourney::Get().MapCore,Item,Checked,Error))return false;
  auto Before=State;State.Items.RemoveAt(At);State.Active=Item;for(int I=0;I<State.DeviceSlots.Num();I++)if(State.DeviceSlots[I]==Uid)State.DeviceSlots[I]="";Publish();
  if(!Persist(Error)){State=Before;Publish();return false;}
  Tower.Run.Floor=Item.Tier;auto Journey=UDWOTowerJourney::Get();Journey.StartOnArrival=false;
  UOpusNative::WriteLog("MapItemUsed uid="+Uid+" tier="+Item.Tier+" affixes="+Item.Affixes.Num());Tower.Travel(false);return true;
 }
}
