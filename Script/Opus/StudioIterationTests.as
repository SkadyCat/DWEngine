// End-to-end native migration checks against real ownership, WBP and plot surfaces.
class UDWOStudioIterationTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;int Stage=0;double Clock=0,Age=0;bool Done=false;FString Uid,StackKey;int RootId=-1,Hits=0;double SeenAge=0;bool SeenLate=false;
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 bool Check(bool Value,FString Name){UOpusNative::WriteLog((Value?"STUDIO_ITERATION_PASS ":"STUDIO_ITERATION_FAIL ")+Name);if(!Value){Done=true;UOpusNative::Quit(false);}return Value;}
 void Next(int I){Stage=I;Clock=0;}
 void Shot(FString Name){if(UOpusNative::Flag("DWOIterationRender"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/StudioToUE-20261027/"+Name+".png -nosuffix");}
 bool RightPurchase(){auto UI=Tower.Studio.UI;auto N=UI.Panel.FindSourceNode("merchant-shop-stock-0");bool Good=UOpusHoverUI::TestPointerButton(N,EKeys::RightMouseButton,true);UOpusHoverUI::TestPointerButton(N,EKeys::RightMouseButton,false);return Good&&UI.PurchaseMenu!=nullptr;}
 bool Click(UOpusActionNode N){auto B=Tower.Hud.ItemHover.ContextMenu.Input(N);if(B==nullptr)return false;bool Good=UOpusHoverUI::TestPointerButton(B,EKeys::LeftMouseButton,true);UOpusHoverUI::TestPointerButton(B,EKeys::LeftMouseButton,false);return Good;}
 UScrollBox GridScroll(UWidget W){auto S=Cast<UScrollBox>(W);if(S!=nullptr)return S;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=GridScroll(C);if(Found!=nullptr)return Found;}return nullptr;}
 UOpusActionNode WornSlot(UWidget W,FString Slot){auto N=Cast<UOpusActionNode>(W);if(N!=nullptr&&N.IsVisible()&&N.EquipmentSlot==Slot&&DWOInventory::Role(N)=="equipped")return N;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=WornSlot(C,Slot);if(Found!=nullptr)return Found;}return nullptr;}
 UOpusActionNode Cell(UWidget W,int Index,FString Binding)
 {
  auto N=Cast<UOpusActionNode>(W);if(N!=nullptr&&N.BagCell==Index&&N.IsVisible()&&DWOInventory::Role(N)==Binding)return N;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Cell(C,Index,Binding);if(Found!=nullptr)return Found;}return nullptr;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Clock+=Dt;if(Clock>80){Check(false,"Timeout "+Stage);return;}auto H=Tower.Hud;auto R=Tower.Rules;auto UI=Tower.Studio.UI;FString Error;
  if(Stage==0){if(!Tower.Dungeon.Ready||!Tower.Hero.Visual.Ready||!Tower.DropService.Ready||Clock<2)return;
   if(!Check(UOpusNative::ApplyPreset(R,n"whirlwind_swarm",Error),"Fixture "+Error))return;Tower.Hero.AutoCast=false;
   H.Screen.ApplySceneState("character");H.RefreshData();H.InventoryLayout.Reconcile();
   if(!Check(H.InventoryLayout.CharacterGrid("").Num()>=36,"CharacterHas36PhysicalCells"))return;
   for(auto E:H.Bag)if(E.Kind=="equipment"&&!E.Equipped){Uid=E.Id;break;}
   if(Uid.IsEmpty()){for(auto E:H.Bag)if(E.Kind=="equipment"&&DWOInventory::Wearable(E)){Uid=E.Id;R.EquipPlayerItem(FName(E.Slot),NAME_None,Error);break;}H.RefreshData();}
   if(!Check(!Uid.IsEmpty(),"MovableNativeEquipment"))return;
   for(auto C:H.InventoryLayout.Cells)for(auto E:H.Bag)if(C.Ref==E.Key&&E.Id==Uid){StackKey=C.Key;break;}
   if(!Check(H.InventoryLayout.Move(StackKey,20),"MoveToEmptyPhysical20"))return;H.RefreshData();Next(1);return;
  }
  if(Stage==1){if(Clock<.6)return;auto L=H.InventoryLayout;int At=L.Find(StackKey);auto Cell20=Cell(H.Screen.GetRootWidget(),20,"stock");
   if(!Check(At==20&&Cell20!=nullptr&&Cell20.HoverId==Uid,"RealGridUsesSavedPhysicalIndex at="+At+" node="+(Cell20!=nullptr)+" state="+H.Screen.CurrentState+" id="+(Cell20==nullptr?"null":Cell20.HoverId)+" expected="+Uid))return;
   Click(Cell20);if(!Check(H.SelectedId==Uid&&!H.ItemHover.Pinned,"ClickSelectsWithoutPin"))return;
   Click(Cell20);H.RefreshData();if(!Check(H.SelectedId.IsEmpty(),"SecondClickDeselectsWithoutFallback"))return;
   Click(Cell20);H.ItemHover.Enter(Cell20);if(!Check(H.ItemHover.Panel!=nullptr&&!H.ItemHover.Pinned,"HoverIsTransient"))return;H.ItemHover.FilterKey(EKeys::LeftControl);if(!Check(H.ItemHover.Pinned,"OnlyControlPins"))return;
   H.ItemHover.Close();if(!Check(H.SelectedId==Uid,"ClosingDetailPreservesSelection"))return;
   auto Saved=R.ExportPlayerState();if(!Check(R.ImportPlayerState(Saved,Error),"LayoutProfileRoundtrip "+Error))return;H.RefreshData();if(!Check(L.Find(StackKey)==20,"PhysicalHoleSurvivesReload"))return;
   Shot("character-selected");Next(2);return;
  }
  if(Stage==2){if(Clock<.5)return;auto L=H.InventoryLayout;auto Cell20=Cell(H.Screen.GetRootWidget(),20,"stock");int Empty=22;auto EmptyNode=Cell(H.Screen.GetRootWidget(),Empty,"stock");
   if(!Check(EmptyNode!=nullptr&&EmptyNode.HoverId.IsEmpty(),"RealEmptyDropCell"))return;
   H.InventoryDrag.Uid=Uid;H.InventoryDrag.Key=StackKey;H.InventoryDrag.From="stock";H.InventoryDrag.Dragged=true;
   auto B=UOpusHoverUI::AnchorRect(EmptyNode,Gameplay::GetPlayerController(0));if(!Check(H.InventoryDrag.Finish(FVector2D((B.X+B.Z)*.5,(B.Y+B.W)*.5))&&L.Find(StackKey)==22,"PointerDropMovesOnlyToExactEmptyCell"))return;
   if(!Check(H.InventoryDrag.ConsumeClick()&&!H.InventoryDrag.ConsumeClick(),"DragSuppressesTrailingClickOnce"))return;
   auto Before=R.ExportPlayerState();if(!Check(!L.Move(StackKey,-1)&&Before==R.ExportPlayerState(),"InvalidDropCannotMutateOwnership"))return;
   Next(21);return;
  }
  if(Stage==21){if(Clock<.5)return;FOpusEntry E;for(auto Item:H.Bag)if(Item.Id==Uid)E=Item;auto Wear=WornSlot(H.Screen.GetRootWidget(),E.Slot);if(!Check(Wear!=nullptr,"AuthoredWearSlotHasPhysicalTarget"))return;auto Rect=UOpusHoverUI::AnchorRect(Wear,Gameplay::GetPlayerController(0));H.InventoryDrag.Uid=Uid;H.InventoryDrag.Key=StackKey;H.InventoryDrag.From="stock";H.InventoryDrag.Dragged=true;if(!Check(H.InventoryDrag.Finish(FVector2D((Rect.X+Rect.Z)*.5,(Rect.Y+Rect.W)*.5)),"DragBackToMatchingWornSlot"))return;Next(22);return;}
  if(Stage==22){if(Clock<.5)return;auto Empty=Cell(H.Screen.GetRootWidget(),20,"stock");if(!Check(Empty!=nullptr&&Empty.HoverId.IsEmpty(),"UnequipDestinationRemainsPhysicalHole"))return;auto Rect=UOpusHoverUI::AnchorRect(Empty,Gameplay::GetPlayerController(0));H.InventoryDrag.Uid=Uid;H.InventoryDrag.Key="";H.InventoryDrag.From="equipped";H.InventoryDrag.Dragged=true;if(!Check(H.InventoryDrag.Finish(FVector2D((Rect.X+Rect.Z)*.5,(Rect.Y+Rect.W)*.5)),"DragWornItemToExactEmptyCell"))return;Next(23);return;}
  if(Stage==23){if(Clock<.5)return;auto N=Cell(H.Screen.GetRootWidget(),20,"stock");if(!Check(N!=nullptr&&N.HoverId==Uid,"UnequippedItemRendersAtRequestedCell"))return;if(!Check(H.InventoryLayout.Move(StackKey,56),"MoveBeyondDefault36Cells"))return;H.RefreshData();Next(231);return;}
  if(Stage==231){if(Clock<.5)return;auto Host=H.InventoryDrag.Host(H.Screen.GetRootWidget(),"character-grid");auto Scroll=GridScroll(Host);if(!Check(Scroll!=nullptr,"NativeCharacterGridHasRealScroll"))return;Scroll.SetScrollOffset(10000);Next(232);return;}
  if(Stage==232){if(Clock<.5)return;auto Host=H.InventoryDrag.Host(H.Screen.GetRootWidget(),"character-grid");auto Scroll=GridScroll(Host);auto N=Cell(H.Screen.GetRootWidget(),56,"stock");auto R1=UOpusHoverUI::AnchorRect(Host,Gameplay::GetPlayerController(0));auto R2=UOpusHoverUI::AnchorRect(N,Gameplay::GetPlayerController(0));if(!Check(N!=nullptr&&N.HoverId==Uid&&Scroll.GetScrollOffset()>0&&(R2.Y+R2.W)*.5>=R1.Y&&(R2.Y+R2.W)*.5<=R1.W,"NativeScrollReachesNewPhysicalCell"))return;H.InventoryLayout.Move(StackKey,20);H.RefreshData();Scroll.SetScrollOffset(0);H.Screen.ApplySceneState("selected-1");Tower.Recent.State.RewardClaimed=false;Tower.Recent.State.FirstFloorComplete=false;Tower.Recent.State.QuestTrackerCollapsed=false;Tower.Recent.UI.RefreshTracker();Shot("quest-expanded");Next(24);return;}
  if(Stage==24){if(Clock<.5)return;auto Q=Tower.Recent.UI;auto Toggle=Cast<UOpusActionNode>(Q.Tracker.FindSourceNode("studio-quest-"+Q.TrackerMode+"-quest-toggle"));if(!Check(Toggle!=nullptr&&Click(Toggle)&&Tower.Recent.State.QuestTrackerCollapsed&&Q.TrackerMode=="compact-active","QuestTrackerCollapsesThroughSlate"))return;FOpusGameplayState State;if(!Check(UOpusGameplayStore::Decode(UOpusGameplayStore::Encode(R.ExportPlayerState(),Tower.Recent.State),State,Error)&&State.QuestTrackerCollapsed,"QuestCollapsePersistsInProfile"))return;Shot("quest-collapsed");Next(25);return;}
  if(Stage==25){if(Clock<.5)return;auto Q=Tower.Recent.UI;auto Toggle=Cast<UOpusActionNode>(Q.Tracker.FindSourceNode("studio-quest-"+Q.TrackerMode+"-compact-quest-toggle"));if(!Check(Toggle!=nullptr&&Click(Toggle)&&!Tower.Recent.State.QuestTrackerCollapsed&&Q.TrackerMode.StartsWith("expanded"),"QuestTrackerExpandsThroughSlate"))return;Tower.Recent.State.FirstFloorComplete=true;Q.RefreshTracker();if(!Check(Q.TrackerMode=="expanded-reward","QuestTrackerShowsRewardState"))return;Shot("quest-reward-ready");Next(26);return;}
  if(Stage==26){if(Clock<.5)return;auto Q=Tower.Recent.UI;Tower.Recent.State.RewardClaimed=true;Q.RefreshTracker();if(!Check(Q.TrackerMode=="empty","ClaimedQuestUsesCompactEmptyState"))return;Tower.Recent.State.RewardClaimed=false;Tower.Recent.State.FirstFloorComplete=false;Q.RefreshTracker();if(!Check(UI.Open("merchant"),"MerchantOpen"))return;Next(3);return;}
  if(Stage==3){if(Clock<.6)return;int64 Stones=UOpusNative::Stones(R);int Maps=Tower.MapItems.State.Items.Num();
   if(!Check(RightPurchase()&&UI.PurchaseMenu!=nullptr&&Stones==UOpusNative::Stones(R),"RightClickOpensApprovedPurchaseMenu"))return;
   UI.FilterKey(EKeys::Escape);if(!Check(UI.PurchaseMenu==nullptr&&UI.Panel!=nullptr&&UI.Selected==0&&Stones==UOpusNative::Stones(R),"EscapeClosesMenuOnly"))return;
   R.ImportPlayerState("{\"initialized\":true,\"stones\":10000}",Error);if(!Check(RightPurchase(),"PurchaseMenuFunded"))return;Shot("merchant-purchase-menu");Next(4);return;
  }
  if(Stage==4){if(Clock<.5)return;int64 Stones=UOpusNative::Stones(R);if(!Check(UI.ConfirmPurchase()&&UOpusNative::Stones(R)==Stones-20&&!UI.ConfirmPurchase(),"ConfirmationChargesExactlyOnce"))return;
   R.ImportPlayerState("{\"initialized\":true,\"stones\":0}",Error);if(!Check(UI.OpenPurchaseMenu(0)&&!UI.CanPurchase&&!UI.ConfirmPurchase()&&UOpusNative::Stones(R)==0,"InsufficientFundsDisablesMenu"))return;
   UI.Close();auto D=H.MapDevice;D.Open();auto A=D.Atlas;int Current=A.Index(A.Current);FString Other=A.Nodes[Current].Neighbors[0];A.Selected=Other;D.Pan=FVector2D(123,-99);D.FilterKey(EKeys::F);D.DrawAtlas();
   if(!Check((D.Pan+A.Nodes[Current].Position*D.Zoom).Size()<.01&&D.PlayerMarker!=nullptr&&D.PlayerMarkerNode==A.Current&&A.Selected==Other,"FAndPlayerMarkerUseActualLocationNotDestination"))return;Next(5);return;
  }
  if(Stage==5){if(Clock<.7)return;Shot("atlas-player-location");Next(6);return;}
  if(Stage==6){if(Clock<.5)return;H.MapDevice.Close();if(!Check(R.ImportPlayerState("{\"initialized\":true,\"owned\":{\"equipment\":{\"gale_spindle\":1}},\"loadout\":{\"skills\":[\"whirlwind\"],\"equipment\":{\"weapon\":\"gale_spindle\"}}}",Error),"WhirlwindFixture "+Error))return;
   Tower.Hero.AutoCast=false;Tower.Hero.ControlProbe=true;Tower.SetPaused(false);R.SetPlayerCasting(false);R.ClearStaticTargets();FVector Origin=Tower.Hero.CombatLocation();Tower.Hero.Aim=FVector(1,0,0);R.UpdateOwner(Tower.Hero.OwnerSlot,Origin,Tower.Hero.Aim);UOpusNative::TargetPoint(R,Tower.Hero.OwnerSlot,Origin+FVector(1000,0,0));
   R.AddStaticTarget(Origin+FVector(80,0,0),150,1000000000,1);RootId=R.CastBuildPower(UOpusNative::ActivePlan(R),Tower.Hero.OwnerSlot,Origin,Tower.Hero.Aim,30);if(!Check(RootId>=0,"CastNativeWhirlwind"))return;Age=0;Next(7);return;
  }
  if(Stage==7){Age+=Dt;auto D=Tower.Hero.DeliveryPolicy;bool Alive=false;for(int I=0;I<D.ProjectileCount();I++){auto P=D.Projectile(I);if(P.Valid&&P.Kind==n"whirlwind"){Alive=true;SeenAge=Math::Max(SeenAge,P.Age);if(P.Age>=1.5&&!SeenLate){SeenLate=true;Shot("whirlwind-after-contact-1.5s");}}}
   Hits+=R.GetFrameReport().Hits;
   if(Age>2.25){if(!Check(SeenLate&&SeenAge>=1.9&&!Alive&&Hits>0,"WhirlwindLivesTwoSecondsThroughContact age="+SeenAge+" hits="+Hits))return;Check(true,"Complete");Done=true;UOpusNative::Quit(true);}return;
  }
 }
}
