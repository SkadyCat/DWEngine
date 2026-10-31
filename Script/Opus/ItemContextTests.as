// Real Slate right/left button routes in a transient, rendered game viewport.
class UDWOItemContextTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UOpusActionNode Source;
 UPROPERTY() UButton Pressed;
 TArray<FString> Items;
 int Stage=0;
 double Clock=0,Total=0;
 int64 BeforeMoney=0,Quote=0;
 FString BeforeRules,BeforeLines,DropKey;
 bool Done=false;
 bool Check(bool Good,FString Why){if(Good)return true;Done=true;UOpusNative::WriteLog("CONTEXT_FAIL "+Why);UOpusNative::Quit(false);return false;}
 void Pass(FString What){UOpusNative::WriteLog("CONTEXT_PASS "+What);}
 void Next(int Value){Stage=Value;Clock=0;}
 void Initialize(ADWOTowerGameMode Game){Tower=Game;SetTickGroup(ETickingGroup::TG_PostUpdateWork);Tower.Hero.ControlProbe=true;Tower.Rules.SetPlayerGod(true);}
 bool Visible(UWidget W) const {UWidget Current=W;while(Current!=nullptr){if(Current.GetVisibility()==ESlateVisibility::Collapsed||Current.GetVisibility()==ESlateVisibility::Hidden)return false;Current=Current.GetParent();}return true;}
 UOpusActionNode Find(UWidget W,FString Id)
 {
  auto N=Cast<UOpusActionNode>(W);if(N!=nullptr&&N.HoverId==Id&&Visible(N)){auto R=UOpusHoverUI::AnchorRect(N,Gameplay::GetPlayerController(0));if(R.Z>R.X&&R.W>R.Y)return N;}
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Match=Find(C,Id);if(Match!=nullptr)return Match;}return nullptr;
 }
 FOpusEntry Entry(FString Id){for(auto E:UOpusData::Inventory(Tower.Rules))if(E.Id==Id)return E;return FOpusEntry();}
 bool InBag(UWidget W,FString Id,FString Binding)
 {
  auto Node=Cast<USvgCanvasNode>(W);if(Node!=nullptr&&Node.Binding==Binding)return Find(W,Id)!=nullptr;
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto Child:P.GetAllChildren())if(InBag(Child,Id,Binding))return true;return false;
 }
 bool Placement(FString Id,bool Equipped)
 {
  if(!UOpusNative::Flag("DWOInventorySplitTest"))return true;
  auto Root=Tower.Hud.Screen.GetRootWidget();return Check(InBag(Root,Id,"bag-grid")==!Equipped&&InBag(Root,Id,"equipped-bag-grid")==Equipped,"item occurs in exactly its correct backpack: "+Id);
 }
 UOpusActionNode DropTarget(UWidget W,bool Wear,FString Slot)
 {
  auto N=Cast<UOpusActionNode>(W);
  if(N!=nullptr&&Visible(N)&&(Wear?N.EquipmentSlot==Slot&&DWOInventory::Role(N)=="equipped":N.BagCell>=0&&N.HoverId.IsEmpty()&&DWOInventory::Role(N)=="stock"))return N;
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Match=DropTarget(C,Wear,Slot);if(Match!=nullptr)return Match;}return nullptr;
 }
 bool DragTo(FString Id,bool Wear)
 {
  auto H=Tower.Hud;auto D=H.InventoryDrag;
  // The full HUD retains other pages' row instances. GeneratePathToWidgetUnchecked
  // can route synthetic input to those stale copies, so select the active bag.
  auto SourceHost=D.Host(H.Screen.GetRootWidget(),Wear?"bag-grid":"equipped-bag-grid");Source=Find(SourceHost,Id);auto B=H.ItemHover.ContextMenu.Input(Source);
  if(!Check(Source!=nullptr&&DWOInventory::Role(Source)==(Wear?"stock":"equipped"),"source belongs to the currently visible backpack"))return false;
  if(!Check(B!=nullptr,"drag source button"))return false;B.OnHovered.Broadcast();if(!Check(UOpusHoverUI::TestPointerButton(B,EKeys::LeftMouseButton,true)&&D.Uid==Id,"native pointer starts backpack drag"))return false;
  auto TargetHost=D.Host(H.Screen.GetRootWidget(),Wear?"equipped-bag-grid":"bag-grid");auto Target=DropTarget(TargetHost,Wear,Entry(Id).Slot);
  if(!Check(Target!=nullptr,"matching worn slot or genuinely empty ordinary cell"))return false;
  // Returning a replaced ring puts it in cell 0. That occupied cell is not a
  // valid unequip destination; use a real empty cell, not fixed pixel (20,20).
  auto R=UOpusHoverUI::AnchorRect(Target,Gameplay::GetPlayerController(0));FVector2D Point=FVector2D((R.X+R.Z)*.5,(R.Y+R.W)*.5);
  UOpusHoverUI::TestPointerButton(B,EKeys::LeftMouseButton,false);D.Dragged=true;
  FString DragId=D.Uid,Role=D.From;auto Hit=D.CellAt(H.Screen.GetRootWidget(),Point,Wear);bool Good=D.Finish(Point);
  return Check(Good,"drag policy commits cross-backpack transfer id="+DragId+" role="+Role+" hit="+(Hit==nullptr?"null":Hit.EquipmentSlot+"/"+Hit.BagCell+"/"+Hit.HoverId)+" error="+Tower.LastError);
 }
 FString Lines(FString Id){FString Out;for(auto L:UOpusData::HoverLines(Tower.Rules,Id))Out+=L.Text+L.Tier+L.Grade;return Out;}
 bool Right(FString Id)
 {
  Source=Find(Tower.Hud.Screen.GetRootWidget(),Id);if(!Check(Source!=nullptr,"arranged inventory cell "+Id))return false;
  auto B=Tower.Hud.ItemHover.ContextMenu.Input(Source);if(!Check(B!=nullptr,"native cell button"))return false;
  B.OnHovered.Broadcast();BeforeRules=Tower.Rules.ExportPlayerState();
  if(!Check(UOpusHoverUI::TestPointerButton(B,EKeys::RightMouseButton,true),"Slate right-down is handled by the screen"))return false;
  UOpusHoverUI::TestPointerButton(B,EKeys::RightMouseButton,false);return true;
 }
 bool Down(FString Op)
 {
  auto C=Tower.Hud.ItemHover.ContextMenu;Pressed=C.Input(C.Button(Op));
  bool Handled=Pressed!=nullptr&&Pressed.IsEnabled&&UOpusHoverUI::TestPointerButton(Pressed,EKeys::LeftMouseButton,true);
  if(Pressed!=nullptr)UOpusNative::WriteLog("CONTEXT_POINTER down "+Op+" hover="+Pressed.IsHovered()+" pressed="+Pressed.IsPressed());
  return Check(Handled,"native menu button down "+Op);
 }
 void Up(){if(Pressed!=nullptr){UOpusHoverUI::TestPointerButton(Pressed,EKeys::LeftMouseButton,false);UOpusNative::WriteLog("CONTEXT_POINTER up hover="+Pressed.IsHovered()+" pressed="+Pressed.IsPressed());}Pressed=nullptr;}
 void RefreshBag(){Tower.Hud.RefreshDataPart(0);Tower.Hud.RefreshDataPart(1);}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Clock+=Dt;Total+=Dt;if(Total>100){Check(false,"timeout stage="+Stage);return;}
  auto H=Tower.Hud;auto C=H.ItemHover.ContextMenu;
  if(Stage==0)
  {
   if(!Tower.Hero.Visual.Ready||!Tower.DropService.Ready)return;
   for(int I=0;I<3;I++)
   {
    auto Offer=Cast<UOpusEventOffer>(NewObject(this,UOpusEventOffer));TArray<FName> Allowed;Allowed.Add(n"stormband");TArray<int> Pick;Pick.Add(0);FString Error;
    if(!Check(Offer.Generate(Tower.Rules,Allowed,610+I,1,n"rare",1,1,Error)&&Offer.Claim(Pick,Error),"real random rare ring "+Error))return;Items.Add(Offer.Entries()[0].Id);
   }
   if(UOpusNative::Flag("DWOInventorySplitTest")){if(!Check(DWOInventory::StockCount(3,true)==2&&DWOInventory::StockCount(1,true)==0&&DWOInventory::StockCount(3,false)==3,"template copies conserve quantity"))return;Pass("CompartmentCounts");}
   H.Screen.ApplySceneState("inventory");RefreshBag();Next(1);return;
  }
  if(Clock<.12)return;
  if(Stage==1){if(!Placement(Items[0],false)||!Right(Items[0]))return;Next(2);return;}
  if(Stage==2)
  {
   if(!Check(C.IsOpen()&&C.ItemId==Items[0]&&H.Screen.CurrentState=="inventory"&&!Entry(Items[0]).Equipped&&BeforeRules==Tower.Rules.ExportPlayerState(),"right press opens menu without left action or inventory mutation"))return;
   if(!Check(UOpusUI::SourceHash(C.Panel)=="95674cf399e4aae59c06e5ae39255e955bceae085f5cfa4e34b3ed00ad3c4649","frozen UICreator context source"))return;
   Pass("RightPointerAndUICreator");System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/"+(UOpusNative::Flag("DWOInventorySplitTest")?"InventoryCompartments/ue-inventory.png":"ItemContext/ue-menu.png")+" -nosuffix");Next(3);return;
  }
  if(Stage==3){if(!Down("primary"))return;Next(4);return;}
  if(Stage==4){Up();Next(5);return;}
  if(Stage==5){if(!Check(!C.IsOpen()&&Entry(Items[0]).Equipped,"native menu equips selected ring; open="+C.IsOpen()+" error="+Tower.LastError))return;RefreshBag();Pass("Equip");Next(6);return;}
  if(Stage==6){if(!Placement(Items[0],true)||!Right(Items[0]))return;Next(7);return;}
  if(Stage==7){if(!Check(DWOHoverTests::VisibleText(C.Panel.GetRootWidget()).Contains("卸下"),"equipped menu changes action label"))return;if(!Down("primary"))return;Next(8);return;}
  if(Stage==8){Up();Next(9);return;}
  if(Stage==9){if(!Check(!Entry(Items[0]).Equipped,"native menu unequips selected ring"))return;Pass("Unequip");RefreshBag();Next(10);return;}
  if(Stage==10){if(!Placement(Items[0],false)||!Right(Items[0]))return;FString Error;BeforeMoney=UOpusNative::Stones(Tower.Rules);if(!Check(Tower.Rules.QuotePlayerEquipmentSale(Items[0],Quote,Error),Error))return;Next(11);return;}
  if(Stage==11){if(!Down("sell"))return;Next(12);return;}
  if(Stage==12){Up();Next(13);return;}
  if(Stage==13){if(!Check(Entry(Items[0]).Id.IsEmpty()&&UOpusNative::Stones(Tower.Rules)==BeforeMoney+Quote,"sale removes one item and pays exact native quote once"))return;Pass("SaleOnce");RefreshBag();Next(14);return;}
  if(Stage==14){BeforeLines=Lines(Items[1]);if(!Right(Items[1]))return;Next(15);return;}
  if(Stage==15){if(!Down("drop"))return;Next(16);return;}
  if(Stage==16){Up();DropKey=C.LastDropKey;Next(17);return;}
  if(Stage==17)
  {
   bool Ground=false;for(auto L:UOpusNative::Loot(Tower.Rules))if(L.Key==DropKey&&!L.Claimed)Ground=true;
   if(!Check(Entry(Items[1]).Id.IsEmpty()&&!DropKey.IsEmpty()&&Ground,"drop removes bag item and creates real ground loot"))return;
   FString Error;if(!Check(UOpusNative::Pickup(Tower.Rules,DropKey,Tower.Hero.ActorLocation,Error),"ground item can be picked back up: "+Error))return;
   if(!Check(Entry(Items[1]).Id==Items[1]&&Lines(Items[1])==BeforeLines&&!UOpusNative::Pickup(Tower.Rules,DropKey,Tower.Hero.ActorLocation,Error),"same UID and affixes return once"))return;
   Pass("DropAndPickupIdentity");RefreshBag();FString LockError;if(!Check(Tower.Rules.LockPlayerEventItem(Items[2],"context-test",LockError),LockError))return;Next(18);return;
  }
  if(Stage==18){if(!Right(Items[2]))return;Next(19);return;}
  if(Stage==19)
  {
   FString Error,Key;if(!Check(!C.CanPrimary&&!C.CanSell&&!C.CanDrop&&!Tower.Rules.DropPlayerInventory(n"equipment",Items[2],1,Tower.Hero.ActorLocation,Key,Error)&&!Entry(Items[2]).Id.IsEmpty(),"altar lock blocks both menu and direct drop transaction"))return;
   if(!Check(C.FilterKey(EKeys::Escape)&&!C.IsOpen()&&H.Screen.CurrentState=="inventory","Escape closes only context menu"))return;
   Tower.Rules.UnlockPlayerEventItem(Items[2],"context-test",Error);Pass("LockAndEscape");
   if(UOpusNative::Flag("DWOInventorySplitTest")){if(!Check(Tower.Rules.EquipPlayerItem(n"ring",FName(Items[2]),Error),Error))return;RefreshBag();Next(20);return;}
   Pass("Complete");Done=true;UOpusNative::Quit(true);
  }
  if(Stage==20){if(!Placement(Items[2],true)||!Placement(Items[1],false))return;FString Error;if(!Check(Tower.Rules.EquipPlayerItem(n"ring",FName(Items[1]),Error),Error))return;RefreshBag();Next(21);return;}
  if(Stage==21){if(!Placement(Items[1],true)||!Placement(Items[2],false))return;Pass("ReplacementReturnsOldEquipment");System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/InventoryCompartments/ue-inventory.png -nosuffix");Next(22);return;}
  if(Stage==22){if(!Right(Items[1]))return;Next(23);return;}
  if(Stage==23){if(!Down("drop"))return;Next(24);return;}
  if(Stage==24){Up();DropKey=C.LastDropKey;Next(25);return;}
  if(Stage==25){if(!Check(Entry(Items[1]).Id.IsEmpty(),"dropping worn gear removes equipped ownership"))return;FString Error;if(!Check(UOpusNative::Pickup(Tower.Rules,DropKey,Tower.Hero.ActorLocation,Error),Error))return;RefreshBag();Next(26);return;}
  if(Stage==26){if(!Placement(Items[1],false))return;Pass("EquippedDropReturnsToOrdinary");if(!DragTo(Items[1],true))return;Next(27);return;}
  if(Stage==27){if(!Placement(Items[1],true)||!DragTo(Items[1],false))return;Next(28);return;}
  if(Stage==28){if(!Placement(Items[1],false))return;Pass("CrossBackpackDrag");Pass("Complete");Done=true;UOpusNative::Quit(true);}
 }
}
