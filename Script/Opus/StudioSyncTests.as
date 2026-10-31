// Real transient native inventory + saved UICreator interaction, never a formal profile.
class UDWOStudioSyncTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0;double Clock=0;bool Done=false;
 FString ItemId,DropKey,StockKey;int64 Period=0;
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 bool Check(bool Good,FString What){if(!Good){Done=true;UOpusNative::WriteLog("STUDIO_SYNC_FAIL "+What);UOpusNative::Quit(false);}else UOpusNative::WriteLog("STUDIO_SYNC_PASS "+What);return Good;}
 void Shot(FString Name){if(UOpusNative::Flag("DWOStudioRender"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/StudioSync/"+Name+".png -nosuffix");}
 void Next(int Value){Stage=Value;Clock=0;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Clock+=Dt;if(Clock>100){Check(false,"Timeout stage "+Stage);return;}auto G=Tower.Studio;auto UI=G.UI;auto R=Tower.Rules;FString Error;
  if(Stage==0)
  {
   if(!Tower.Dungeon.Ready||!Tower.Hero.Visual.Ready||!Tower.DropService.Ready||Clock<2)return;
   if(UOpusNative::Flag("DWOStudioRender")&&!UOpusWorldUI::VisualReady(UI.MerchantTag))return;
   if(!Check(UI!=nullptr&&UI.Rail!=nullptr&&G.Map.Mini!=nullptr&&G.MerchantReady,"SavedUICreatorAndOriginalMerchant"))return;
   FVector BossPoint;int Reserved=Tower.Dungeon.Reserved.Num();bool BossPlaced=Tower.Dungeon.SpawnPoint(0,true,BossPoint);
   if(!Check(BossPlaced&&(BossPoint-Tower.Dungeon.PlayerSpawnLocation).Size2D()>2600&&UOpusDungeon::Path(Tower,Tower.Dungeon.PlayerSpawnLocation,BossPoint).Num()>1,"BossUsesReachableDistantRoom"))return;
   while(Tower.Dungeon.Reserved.Num()>Reserved)Tower.Dungeon.Reserved.RemoveAt(Tower.Dungeon.Reserved.Num()-1);
   if(!Check(R.ImportPlayerState("{\"initialized\":true,\"stones\":100000}",Error),"PurchaseFixture "+Error))return;
   auto Before=R.ExportPlayerState();if(!Check(G.EnsureStock()&&Before==R.ExportPlayerState()&&Tower.Recent.State.MerchantStock.Num()==6&&G.MapOffers().Num()==3,"PrivateStockNoCombatMutation"))return;
   Period=Tower.Recent.State.MerchantPeriod;StockKey=Tower.Recent.State.MerchantStock[0].Key;
   if(UOpusNative::Flag("DWOStudioRender"))
   {
    auto Tag=UI.MerchantTag.FindSourceNode("merchant-launcher-merchant-open");
    auto Input=DWOHoverTests::Input(Tag);
    if(!Check(Input!=nullptr&&UOpusHoverUI::TestPointerButton(Input,EKeys::LeftMouseButton,true),"MerchantBubblePointerDown"))return;
    UOpusHoverUI::TestPointerButton(Input,EKeys::LeftMouseButton,false);
    if(!Check(UI.Panel!=nullptr&&UI.View=="merchant","WholeMerchantBubbleOpensThroughSlate"))return;
   }
   else if(!Check(UI.Open("merchant"),"OpenMerchant"))return;Next(1);return;
  }
  if(Stage==1)
  {
   if(Clock<1)return;auto Node=UI.Panel.FindSourceNode("merchant-shop-stock-3");int64 Stones=UOpusNative::Stones(R);
   if(!Check(Node!=nullptr,"SavedOfferAction"))return;
   if(UOpusNative::Flag("DWOStudioRender")){auto Input=DWOHoverTests::Input(Node);UOpusHoverUI::TestPointerButton(Input,EKeys::LeftMouseButton,true);UOpusHoverUI::TestPointerButton(Input,EKeys::LeftMouseButton,false);}else UI.Panel.OnSourceAction.Broadcast(Node);
   if(!Check(UI.Selected==3&&UOpusNative::Stones(R)==Stones,"LeftClickSelectsWithoutSpending"))return;
   if(UOpusNative::Flag("DWOStudioRender"))
   {
    if(!Check(UI.Hover!=nullptr&&UI.FilterKey(EKeys::LeftControl)&&UI.HoverPinned,"MerchantHoverPinsWithControl"))return;
    UI.HandleKeys(Gameplay::GetPlayerController(0));int64 BeforePinned=UOpusNative::Stones(R);auto Pinned=UI.Hover.GetRootWidget();
    bool Blocked=UOpusHoverUI::TestPointerButton(Pinned,EKeys::RightMouseButton,true);UOpusHoverUI::TestPointerButton(Pinned,EKeys::RightMouseButton,false);
    if(!Check(Blocked&&UI.Suppress&&UOpusNative::Stones(R)==BeforePinned,"PinnedTooltipBlocksPurchaseThroughIt"))return;
    UI.LeaveHover(Node);if(!Check(UI.Hover!=nullptr&&UI.FilterKey(EKeys::Escape)&&UI.Hover==nullptr&&UI.Panel!=nullptr,"PinnedHoverLeavesAndEscapePreservesShop"))return;
   }
   Shot("merchant-selected");Next(2);return;
  }
  if(Stage==2)
  {
   if(Clock<.5)return;int Count=Tower.MapItems.State.Items.Num();int64 Stones=UOpusNative::Stones(R);
   bool Menu=false;if(UOpusNative::Flag("DWOStudioRender")){auto Buy=UI.Panel.FindSourceNode("merchant-shop-stock-0");Menu=UOpusHoverUI::TestPointerButton(Buy,EKeys::RightMouseButton,true);UOpusHoverUI::TestPointerButton(Buy,EKeys::RightMouseButton,false);}else Menu=UI.OpenPurchaseMenu(0);
   if(!Check(Menu&&UI.PurchaseMenu!=nullptr&&Tower.MapItems.State.Items.Num()==Count&&UOpusNative::Stones(R)==Stones,"RightClickMenuDoesNotSpend"))return;
   bool Bought=UI.ConfirmPurchase();
   if(!Check(Bought&&Tower.MapItems.State.Items.Num()==Count+1&&UOpusNative::Stones(R)==Stones-20,"UnlimitedT1MapCosts20"))return;
   if(!Check(UI.Purchase(1)&&UI.Purchase(2)&&UOpusNative::Stones(R)==Stones-120,"T2T3MapsCost40And60"))return;
   auto Offer=Tower.Recent.State.MerchantStock[0];auto E=UOpusGameplayStore::Entry(R,Offer.Payload);Stones=UOpusNative::Stones(R);
   if(!Check(UI.Purchase(3)&&Tower.Recent.State.MerchantStock[0].Claimed&&UOpusNative::Stones(R)==Stones-Offer.Price,"SpecialPurchaseChargesOnce"))return;
   bool Found=false;for(auto I:UOpusData::Inventory(R))if(I.Id==E.Id)Found=true;
   if(!Check(Found&&!UI.Purchase(3)&&UOpusNative::Stones(R)==Stones-Offer.Price,"FiniteStockPreservesUidAndCannotRepurchase"))return;
   FOpusGameplayState Decoded;if(!Check(UOpusGameplayStore::Decode(Tower.MapItems.Snapshot(),Decoded,Error)&&Decoded.MerchantStock[0].Claimed,"StockSnapshotRoundtrip"))return;
   Tower.Recent.State=Decoded;if(!Check(G.EnsureStock(Period*1800-1800)&&Tower.Recent.State.MerchantStock[0].Claimed&&Tower.Recent.State.MerchantPeriod==Period,"ReloadAndClockRollbackDoNotReplenish"))return;
   if(!Check(G.EnsureStock((Period+1)*1800)&&Tower.Recent.State.MerchantStock.Num()==6&&!Tower.Recent.State.MerchantStock[0].Claimed&&Tower.Recent.State.MerchantStock[0].Key!=StockKey&&!G.Buy(3,StockKey),"RefreshChangesStockAndRejectsStaleOffer"))return;
   UI.Close();if(!Check(!Tower.Paused,"MerchantRestoresPause"))return;
   bool Dropped=G.DropTrait("frostburst_staff",DropKey);if(!Check(Dropped,"CatalogueCreatesRealGroundEquipment "+Tower.LastError))return;
   bool Ground=false;for(auto L:UOpusData::Ground(R))if(L.Key==DropKey&&L.Name=="霜爆法杖"&&L.Rarity=="magic"&&L.Tier==1){Ground=true;ItemId=L.Id;}
   if(!Check(Ground&&ItemId.StartsWith("trait_"),"GroundHasNamedUniqueT1MagicStaff"))return;
   if(!Check(UOpusNative::Pickup(R,DropKey,Tower.Hero.ActorLocation,Error),"NativePickup "+Error))return;
   bool Picked=false;for(auto I:UOpusData::Inventory(R))if(I.Id==ItemId)Picked=true;
   auto Lines=UOpusData::HoverLines(R,ItemId);bool Burst=false,CountLine=false;for(auto L:Lines){if(L.Text.Contains("冰缓"))Burst=true;if(L.Text.Contains("条随机词条"))CountLine=true;}
   if(!Check(Picked&&Burst&&!CountLine,"PickupPreservesTraitAndCleanTooltip"))return;
   if(!Check(UI.Open("trait"),"TraitCatalogueUsesSavedPanel"))return;Next(3);return;
  }
  if(Stage==3)
  {
   if(Clock<.7)return;Shot("trait-equipment");Next(31);return;
  }
  if(Stage==31)
  {
   if(Clock<.3)return;UI.Close();if(!Check(G.Map.Open()&&G.Map.Cells.Num()>20&&G.Map.Rooms.Num()>0,"MapReadsActualNativeDungeon"))return;
   G.Map.Scale(false,2);auto Bounds=G.Map.ClampCenter(FVector2D(1e8,-1e8));
   if(!Check(Bounds.X<1e6&&Bounds.Y>-1e6&&G.Map.Zoom>G.Map.FitZoom,"MapZoomAndPanBounds"))return;
   G.Map.Locate();Next(4);return;
  }
  if(Stage==4)
  {
   if(Clock<.7)return;Shot("dungeon-map");Next(41);return;
  }
  if(Stage==41)
  {
   if(Clock<.3)return;G.Map.Close();if(!Check(!Tower.Paused,"MapRestoresPause"))return;
   auto Maps=Tower.MapItems.State;auto State=Tower.Recent.State;
   if(!Check(R.ImportPlayerState("{\"initialized\":true,\"stones\":0}",Error)&&!G.Buy(0)&&Tower.MapItems.State.NextId==Maps.NextId&&Tower.MapItems.State.Seed==Maps.Seed&&Tower.MapItems.State.Items.Num()==Maps.Items.Num(),"InsufficientFundsRestoresMapRngAndOwnership"))return;
   auto Atlas=Tower.Hud.MapDevice.Atlas;if(!Check(Atlas.Active.IsEmpty(),"InitialRegionHasNoPaidAdmission"))return;
   Atlas.Selected="0:0";if(!Check(Atlas.Enter(1),"FreeInitialAdmission"))return;Atlas.Complete();
   if(!Check(Atlas.Enter(1)&&Atlas.Current=="0:0","FreeInitialRevisit"))return;Atlas.Complete();
   int At=Atlas.Index("0:0");FString NextId=Atlas.Nodes[At].Neighbors[0];Atlas.Select(NextId);auto D=Tower.Hud.MapDevice;Tower.MapItems.State.DeviceSlots[0]="";auto Before=Tower.MapItems.Snapshot();D.Open();D.EnterRegion();
   if(!Check(!Tower.Traveling&&Before==Tower.MapItems.Snapshot()&&Atlas.Active.IsEmpty(),"ConnectedRegionRequiresActualMapTicket"))return;D.Close();
   FVector Destination=Tower.Dungeon.PlayerSpawnLocation;G.BossDefeated(Destination);
   if(!Check(G.PortalReady&&G.Portal!=nullptr&&G.UI.PortalTag!=nullptr,"OriginalBossExitModelAndUICreatorAction"))return;
   Tower.Hero.SetActorLocation(Destination);if(!Check(G.ActivatePortal()&&D.Visible&&D.Atlas.Selected==D.Atlas.Current,"PortalOpensAtlasAtCurrentRegion"))return;D.Close();
   bool Far=false;for(auto P:Tower.Dungeon.Points)if((P-Destination).Size2D()>=1000&&(P-Destination).Size2D()<1600){Tower.Hero.SetActorLocation(P+FVector(0,0,41));Far=true;break;}
   if(!Check(Far&&G.ActivatePortal()&&G.Approaching&&G.Route.Num()>0,"RemotePortalStartsNativeRoute"))return;
   Next(5);return;
  }
  if(Stage==5)
  {
   if(Clock<.7)return;Shot("native-world");Next(6);return;
  }
  if(Stage==6)
  {
   if(Clock<.3||G.Approaching)return;
   auto D=Tower.Hud.MapDevice;if(!Check(D.Visible&&D.Atlas.Selected==D.Atlas.Current,"RemotePortalApproachesAndOpensAtlas"))return;D.Close();Next(7);return;
  }
  if(Stage==7)
  {
   if(Clock<.3)return;Done=true;UOpusNative::WriteLog("STUDIO_SYNC_PASS Complete");UOpusNative::Quit(true);
  }
 }
}
