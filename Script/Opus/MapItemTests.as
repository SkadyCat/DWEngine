// Transient integration checks use real catalogue, shop, save codec and UICreator controls.
class UDWOMapItemTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 double Clock=0;bool Done=false;
 bool Check(bool Good,FString What){if(!Good){UOpusNative::WriteLog("MAP_ITEMS_FAIL "+What);UOpusNative::Quit(false);}return Good;}
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;if(Done||Clock<1||Tower==nullptr||Tower.Hud.Screen==nullptr)return;
  if(UOpusNative::Flag("DWODropsOnline")&&!Tower.DropService.Ready){if(Tower.DropService.Failed||Clock>10){Check(false,"online map pool rejected: "+Tower.DropService.Error);Done=true;}return;}Done=true;
  auto M=Tower.MapItems;auto Journey=UDWOTowerJourney::Get();FString Error;
  if(Journey.MapItemTestStage==1||UOpusNative::Flag("DWOMapItemReloadTest"))
  {
   auto Active=M.State.Active;
   if(UOpusNative::Flag("DWODropsOnline")&&!Check(Tower.DropService.Ready&&Tower.DropService.Requests==1,"T16 pool accepted from the real backend after travel"))return;
   if(!Check(Tower.Run.Floor==16&&Tower.Map.Tier==16&&Tower.Run.Phase==EDWOTowerPhase::Ready,"T16 arrival remains ready"))return;
   if(!Check(!Active.Uid.IsEmpty()&&!M.Owns(Active.Uid)&&M.State.Items.Num()==0,"travel consumes exactly one owned map"))return;
   if(!Check(!M.Use(Active.Uid,Error)&&M.State.Items.Num()==0,"used UID cannot be consumed twice"))return;
   FOpusMapData Expected;Error="";if(!Check(UOpusNative::MapCoreData(Tower.Rules,Journey.MapCore,Expected,Error),"base map sheet"))return;
   double Packs=Expected.Packs,Life=Expected.LifeIncreased,Damage=Expected.DamageIncreased,Base=Expected.LifeBase,MoreLife=Expected.LifeMore,MoreDamage=Expected.DamageMore;
   for(auto A:Active.Affixes){if(A.Id=="map_monster_packs")Packs+=A.Value;else if(A.Id=="map_monster_life")Life+=A.Value;else if(A.Id=="map_monster_damage")Damage+=A.Value;else if(A.Id=="map_monster_base_life")Base+=A.Value;else if(A.Id=="map_monster_more_life")MoreLife*=1+A.Value/100;else if(A.Id=="map_monster_more_damage")MoreDamage*=1+A.Value/100;}
   if(!Check(Math::Abs(MoreLife-Tower.Map.LifeMore)<0.00001&&Math::Abs(MoreDamage-Tower.Map.DamageMore)<0.00001&&Packs==Tower.Map.Packs&&Base==Tower.Map.LifeBase&&Life==Tower.Map.LifeIncreased&&Damage==Tower.Map.DamageIncreased,"all six rolled effects reach the existing rule sheet"))return;
   Tower.ChallengeFloor(17);if(!Check(!Tower.Traveling&&Tower.Run.Floor==16,"active map cannot bypass another map consumption"))return;
   if(!Check(UOpusNative::Flag("DWOMapItemProfileTest")&&UOpusNative::WriteProfile(M.Snapshot())&&UOpusNative::ReadProfile()==M.Snapshot(),"save-file replacement succeeds and reads back unchanged"))return;
   UOpusNative::WriteLog(UOpusNative::Flag("DWOMapItemReloadTest")?"MAP_ITEMS_PASS SeparateProcessReload":"MAP_ITEMS_PASS PurchasePersistenceUseAndTravel");UOpusNative::Quit(true);return;
  }
  auto Original=M.State;
  auto Catalog=UOpusMapItems::Catalog(Tower.Rules);if(!Check(Catalog.Num()==48,"16 tiers by 3 qualities are loaded natively"))return;
  TArray<int> Counts;FOpusMapItem Rolled;int RareSixSeed=0;
  for(auto D:Catalog)for(int Seed=1;Seed<=32;Seed++)
  {
   M.State.Seed=Seed;Error="";if(!Check(M.Roll(D.Id,Rolled,Error),"roll "+D.Id+" "+Error))return;
   int Count=Rolled.Affixes.Num();Counts.AddUnique(Count);
   bool Range=D.Rarity=="common"?Count==0:D.Rarity=="magic"?Count>=1&&Count<=2:Count>=3&&Count<=6;
   if(!Check(Range&&Rolled.Tier==D.Tier&&M.Validate(Rolled,Error),"quality bounds and unique map-only affixes"))return;
   if(D.Tier==16&&Count==6)RareSixSeed=Seed;
  }
  for(int Count=0;Count<=6;Count++)if(!Check(Counts.Contains(Count),"all supported affix counts observed "+Count))return;
  M.State=Original;M.Publish();
  FString Player=Tower.Rules.ExportPlayerState();
  if(!Check(Tower.Rules.ImportPlayerState("{\"initialized\":true,\"stones\":0,\"owned\":{\"item\":{\"map_t16_rare\":1}}}",Error),"full template-stack fixture"))return;
  FString Before=M.Snapshot();Error="";
  if(!Check(!M.Buy("item:map_t16_rare",Error)&&M.Snapshot()==Before,"rejected native acquisition preserves ownership and map RNG"))return;
  Error="";if(!Check(Tower.Rules.ImportPlayerState(Player,Error),"restore ordinary player fixture"))return;
  auto Stones=UOpusNative::Stones(Tower.Rules);
  M.State.Seed=RareSixSeed;
  if(!Check(M.Buy("item:map_t16_rare",Error)&&UOpusNative::Stones(Tower.Rules)==Stones&&M.State.Items.Num()==1,"existing GM shop preserves currency and creates one map instance"))return;
  auto Item=M.State.Items[0];if(!Check(Item.Affixes.Num()==6,"rare map can contain all six dedicated affixes"))return;
  int Maps=0;for(auto E:UOpusData::Inventory(Tower.Rules))if(E.Category=="map"){Maps++;if(!Check(E.Id==Item.Uid&&E.Tier==16,"inventory exposes actual UID and tier"))return;}
  if(!Check(Maps==1,"catalogue template copy is converted without duplication"))return;
  FString Saved=M.Snapshot();FOpusMapItemState Restored;
  if(!Check(UOpusMapItems::DecodeProfile(Saved,Restored,Error)&&UOpusMapItems::EncodeProfile(Tower.Rules.ExportPlayerState(),Restored)==Saved,"save roundtrip preserves UID, rolls and RNG"))return;
  FString Core=Tower.Rules.ExportPlayerState();if(!Check(Tower.Rules.ImportPlayerState(Saved,Error)&&Tower.Rules.ExportPlayerState()==Core,"native profile reader accepts addon without changing core state"))return;
  auto Bad=Item;Bad.Tier=15;Error="";if(!Check(!M.Validate(Bad,Error),"mismatched map template tier rejected"))return;
  Before=M.Snapshot();Tower.Run.Phase=EDWOTowerPhase::Waves;Error="";
  if(!Check(!M.Use(Item.Uid,Error)&&Before==M.Snapshot(),"using a map in combat cannot consume it"))return;
  Tower.Run.Phase=EDWOTowerPhase::Ready;Error="";Tower.Rules.ImportPlayerState(Player,Error);
  auto H=Tower.Hud;H.Screen.ApplySceneState("inventory");H.RefreshData();
  auto Anchor=DWOHoverTests::Find(H.Screen.GetRootWidget(),Item.Uid);
  if(!Check(Anchor!=nullptr,"UICreator bag contains the generated map"))return;
  if(!Check(DWOHoverTests::Click(Anchor)&&H.ItemHover.Pinned&&H.ItemHover.Panel!=nullptr,"clicking the UICreator map cell pins its inspector"))return;auto Panel=H.ItemHover.Panel;
  auto Use=DWOHoverTests::Action(Panel.GetRootWidget(),"hover-use-map");
  if(!Check(Use!=nullptr&&DWOHoverTests::VisibleText(Panel.GetRootWidget()).Contains("使用地图"),"existing UICreator inspector exposes map use"))return;
  if(UOpusNative::Flag("DWOMapItemPreview")){H.SetComponentTickEnabled(false);UOpusNative::WriteLog("MAP_ITEMS_PASS InspectorPreview");return;}
  Journey.MapItemTestStage=1;
  if(!Check(DWOHoverTests::Click(Use),"authored use button invokes AS map use"))return;
 }
}
