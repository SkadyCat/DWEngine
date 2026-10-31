// Acquisition and navigation policy ported from Studio. The rule engine is unchanged.
class UDWOStudioGameplay : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UDWOStudioUI UI;
 UPROPERTY() UDWODungeonMap Map;
 UPROPERTY() ADWOOutpostMerchant Merchant;
 UPROPERTY() ADWWebVFXActor Portal;
 UPROPERTY() UDWWebVFXPrefab MerchantAsset;
 UPROPERTY() UDWWebVFXPrefab PortalAsset;
 FVector MerchantPosition,PortalPosition;
 bool MerchantReady=false,PortalReady=false,Approaching=false,Ready=false;
 TArray<FVector> Route;int Waypoint=0;double RefreshIn=0;
 void Initialize(ADWOTowerGameMode T)
 {
  Tower=T;UI=UDWOStudioUI::Create(T);UI.Initialize(this);Map=UDWODungeonMap::Create(T);Map.Initialize(T);Ready=true;
  MerchantAsset=Cast<UDWWebVFXPrefab>(LoadObject(nullptr,"/Game/WebVFX/Variants/DA_outpost_merchant.DA_outpost_merchant"));
  PortalAsset=Cast<UDWWebVFXPrefab>(LoadObject(nullptr,"/Game/WebVFX/Variants/DA_dungeon_portal.DA_dungeon_portal"));
 }
 void Started()
 {
  auto A=Tower.Hud.MapDevice.Atlas;if(A.Active.IsEmpty()&&Tower.MapItems.State.Active.Uid.IsEmpty()&&Tower.Run.Floor==1){A.Selected="0:0";if(A.Enter(1))Tower.Hud.MapDevice.Save();}
 }
 bool EnsureStock(int64 Seconds=-1)
 {
  int64 Now=Seconds<0?UOpusStudioBridge::UnixSeconds():Seconds;int64 Period=Now/1800;auto Before=Tower.Recent.State;
  int World=UDWOTowerJourney::Get().Seed;
  if(Before.MerchantWorld==World&&Before.MerchantPeriod>=0)Period=Math::Max(Period,Before.MerchantPeriod);
  if(Before.MerchantWorld==World&&Before.MerchantPeriod==Period)return true;
  auto Pool=UOpusStudioBridge::FixedTemplates(Tower.Rules);FRandomStream Random;Random.Initialize(World^int(Period*0x45d9f3b)^0x4d455243);
  TArray<FOpusVendorOffer> Stock;FString Error;
  for(int I=0;I<6&&Pool.Num()>0;I++)
  {
   int At=Random.RandRange(0,Pool.Num()-1),Tier=Random.RandRange(1,3);auto Base=Pool[At];Pool.RemoveAt(At);
   FOpusVendorOffer Offer;Offer.Key="special:"+World+":"+Period+":"+I;
   if(!UOpusStudioBridge::Equipment(Tower.Rules,FName(Base.Id),n"legendary",Tier,NAME_None,Random.RandRange(1,2147483646),"outpost_"+World+"_"+Period+"_"+I,"",Offer.Payload,Error)){Tower.LastError=Error;return false;}
   Offer.Price=UOpusStudioBridge::Price(Tower.Rules,FName(Base.Id),Offer.Payload);if(Offer.Price<0){Tower.LastError="特殊装备价格无效";return false;}Stock.Add(Offer);
  }
  Tower.Recent.State.MerchantWorld=World;Tower.Recent.State.MerchantPeriod=Period;Tower.Recent.State.MerchantStock=Stock;
  if(!Tower.Recent.Persist(Error)){Tower.Recent.State=Before;Tower.LastError=Error;return false;}return true;
 }
 TArray<FOpusEntry> MapOffers() const
 {
  TArray<FOpusEntry> Out;for(int Tier=1;Tier<=3;Tier++)for(auto E:UOpusMapItems::Catalog(Tower.Rules))if(E.Tier==Tier&&E.Rarity=="common"){Out.Add(E);break;}return Out;
 }
 bool Buy(int Index,FString ExpectedKey="")
 {
  if(!EnsureStock())return false;auto Maps=MapOffers();if(Index<0||Index>=Maps.Num()+Tower.Recent.State.MerchantStock.Num())return false;
  auto OldState=Tower.Recent.State;auto OldMaps=Tower.MapItems.State;auto Pools=Tower.Rules.GetPlayerPools();FString Before=Tower.Rules.ExportPlayerState(),Error,Payload;int64 Cost=0;
  FOpusMapItem MapItem;
  if(Index<Maps.Num())
  {Cost=Maps[Index].Tier*20;if(!Tower.MapItems.Roll(Maps[Index].Id,MapItem,Error)){Tower.LastError=Error;return false;}}
  else {auto Offer=Tower.Recent.State.MerchantStock[Index-Maps.Num()];if((!ExpectedKey.IsEmpty()&&ExpectedKey!=Offer.Key)||Offer.Claimed){Tower.LastError="商品已售出或已刷新";return false;}Payload=Offer.Payload;Cost=Offer.Price;}
  if(!UOpusStudioBridge::Purchase(Tower.Rules,Payload,Cost,Error)){Tower.MapItems.State=OldMaps;Tower.LastError=Error;return false;}
  if(Index<Maps.Num())Tower.MapItems.State.Items.Add(MapItem);else Tower.Recent.State.MerchantStock[Index-Maps.Num()].Claimed=true;
  Tower.MapItems.Publish();
  if(!Tower.Recent.Persist(Error)){FString Ignored;Tower.Rules.ImportPlayerState(Before,Ignored);UOpusNative::CarryPools(Tower.Rules,Pools);Tower.MapItems.State=OldMaps;Tower.Recent.State=OldState;Tower.MapItems.Publish();Tower.LastError=Error;return false;}
  Tower.Hud.RefreshIn=0;Tower.LastError="";return true;
 }
 bool DropTrait(FString Id,FString& Key)
 {
  if(Id!="frostburst_staff"||Tower.Rules.GetPlayerPools().bDead||Tower.Traveling||Tower.Preparing){Tower.LastError="当前无法生成特质装备";return false;}
  FString Payload,Error,Before=Tower.Rules.ExportPlayerState();auto Pools=Tower.Rules.GetPlayerPools();
  FString Uid="trait_"+UOpusStudioBridge::UniqueId();
  if(!UOpusStudioBridge::Equipment(Tower.Rules,n"frostthorn_staff",n"magic",1,n"chill_consume_burst",71,Uid,"霜爆法杖",Payload,Error)||!UOpusGameplayStore::Grant(Tower.Rules,Payload,Error)){Tower.LastError=Error;return false;}
  if(!Tower.Rules.DropPlayerInventory(n"equipment",Uid,1,Tower.Hero.CombatLocation()+FVector(100,0,0),Key,Error))
  {FString Ignored;Tower.Rules.ImportPlayerState(Before,Ignored);UOpusNative::CarryPools(Tower.Rules,Pools);Tower.LastError=Error;return false;}
  Tower.Hud.ShowLoot=true;Tower.Hud.RefreshIn=0;Tower.SaveProfile();Tower.LastError="";return true;
 }
 bool InitialRegion() const {return Tower.Hud.MapDevice.Atlas.Current=="0:0";}
 void SpawnMerchant()
 {
  if(MerchantReady||!Tower.Dungeon.Ready||UDWOTowerJourney::Get().Hideout||!InitialRegion())return;
  double Best=1e20;FVector Goal=Tower.Dungeon.PlayerSpawnLocation+FVector(350,100,0);
  for(auto P:Tower.Dungeon.Points)if((P-Tower.Dungeon.PlayerSpawnLocation).Size2D()>=300){double D=(P-Goal).Size2D();if(D<Best){Best=D;MerchantPosition=P;}}
  if(Best==1e20)return;
  auto Asset=MerchantAsset;
  if(Asset==nullptr){Tower.LastError="缺少拓荒商人原始模型";return;}
  Merchant=Cast<ADWOOutpostMerchant>(SpawnActor(ADWOOutpostMerchant,MerchantPosition-FVector(0,0,60)));Merchant.Prefab=Asset;Merchant.PlayRate=0;Merchant.bLoop=false;MerchantReady=true;UI.ShowMerchant();
 }
 void BossDefeated(FVector Position)
 {
  if(PortalReady)return;PortalPosition=Position;PortalReady=true;
  auto Asset=PortalAsset;
  if(Asset!=nullptr){Portal=Cast<ADWWebVFXActor>(SpawnActor(ADWWebVFXActor,Position-FVector(0,0,60)));Portal.Prefab=Asset;Portal.bLoop=true;}
  UI.ShowPortal();
 }
 bool ActivatePortal()
 {
  if(!PortalReady||Tower.Rules.GetPlayerPools().bDead)return false;
  if((Tower.Hero.ActorLocation-PortalPosition).Size2D()<=220){Approaching=false;auto D=Tower.Hud.MapDevice;D.Atlas.Select(D.Atlas.Current);D.Open();return true;}
  Route=UOpusDungeon::Path(Tower,Tower.Hero.ActorLocation,PortalPosition);Waypoint=0;
  if(Route.Num()==0){Tower.LastError="无法抵达传送阵";return false;}Approaching=true;Tower.Hero.Visual.MovementPressed();Tower.Hero.PickupTarget="";return true;
 }
 FVector Approach(FVector Input)
 {
  if(!Approaching)return Input;if(!Input.IsNearlyZero()||Tower.Rules.GetPlayerPools().bDead){Approaching=false;return Input;}
  if((Tower.Hero.ActorLocation-PortalPosition).Size2D()<=220){ActivatePortal();return FVector::ZeroVector;}
  while(Waypoint<Route.Num()&&(Route[Waypoint]-Tower.Hero.ActorLocation).Size2D()<45)Waypoint++;
  return ((Waypoint<Route.Num()?Route[Waypoint]:PortalPosition)-Tower.Hero.ActorLocation).GetSafeNormal2D();
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(!Ready||Tower.Traveling)return;RefreshIn-=Dt;if(RefreshIn<=0){RefreshIn=1;SpawnMerchant();if(UI.View=="merchant")if(EnsureStock())UI.Refresh();}
 }
}
