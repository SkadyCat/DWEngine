// Rendered regression: compare the drop's screen position after the row refresh,
// rather than accepting a matching saved BagCell alone. Never uses the live save.
class UDWOInventoryDragTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0;double Clock=0,Total=0;bool Done=false;
 FString Uid="rending_gauntlets",StackKey,Before;FVector2D DropPoint;
 void Initialize(ADWOTowerGameMode T){Tower=T;SetTickGroup(ETickingGroup::TG_PostUpdateWork);Tower.Hero.ControlProbe=true;}
 bool Check(bool Good,FString Why){UOpusNative::WriteLog((Good?"DRAG_PASS ":"DRAG_FAIL ")+Why);if(!Good){Done=true;UOpusNative::Quit(false);}return Good;}
 void Next(int S){Stage=S;Clock=0;}
 bool Visible(UWidget W){auto Current=W;while(Current!=nullptr){if(!Current.IsVisible())return false;Current=Current.GetParent();}return true;}
 UOpusActionNode Cell(UWidget W,int Index,FString Role="stock")
 {
  auto N=Cast<UOpusActionNode>(W);if(N!=nullptr&&Visible(N)&&DWOInventory::Role(N)==Role&&N.BagCell==Index)return N;
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Match=Cell(C,Index,Role);if(Match!=nullptr)return Match;}return nullptr;
 }
 UOpusActionNode Gear(UWidget W,FString Slot)
 {
  auto N=Cast<UOpusActionNode>(W);if(N!=nullptr&&Visible(N)&&DWOInventory::Role(N)=="equipped"&&N.EquipmentSlot==Slot)return N;
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Match=Gear(C,Slot);if(Match!=nullptr)return Match;}return nullptr;
 }
 FVector2D Center(UWidget W){auto R=UOpusHoverUI::AnchorRect(W,Gameplay::GetPlayerController(0));return FVector2D((R.X+R.Z)*.5,(R.Y+R.W)*.5);}
 FString Ref(FString Id){for(auto E:Tower.Hud.Bag)if(E.Id==Id)return E.Key;return "";}
 FString Key(FString Id){FString R=Ref(Id);for(auto C:Tower.Hud.InventoryLayout.Cells)if(C.Ref==R)return C.Key;return "";}
 bool Drop(UOpusActionNode Source,UOpusActionNode Target)
 {
  if(!Check(Source!=nullptr&&Target!=nullptr,"arranged source and target"))return false;
  auto H=Tower.Hud;auto D=H.InventoryDrag;DropPoint=Center(Target);D.Uid=Source.HoverId;D.From=DWOInventory::Role(Source);D.Key=Source.PlacementKey;D.Dragged=true;
  return D.Finish(DropPoint);
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done)return;Clock+=Dt;Total+=Dt;if(Total>75){Check(false,"timeout stage="+Stage);return;}
  auto H=Tower.Hud;auto L=H.InventoryLayout;auto R=Tower.Rules;FString Error;
  if(Stage==0){if(!Tower.Hero.Visual.Ready||!Tower.DropService.Ready)return;
   if(!Check(R.ImportPlayerState("{\"initialized\":true,\"owned\":{\"equipment\":{\"frostthorn_staff\":1,\"rending_gauntlets\":1,\"stormband\":1,\"cinder_guard_boots\":1}},\"loadout\":{\"equipment\":{\"weapon\":\"frostthorn_staff\"}}}",Error),"fixture "+Error))return;
   H.Screen.ApplySceneState("character");H.RefreshData();
   auto Previous=H.InventoryDrag;Previous.Cancel();Previous.SetComponentTickEnabled(false);H.InventoryDrag=nullptr;H.EnsureInventoryInteraction();
   if(!Check(H.InventoryDrag!=nullptr&&H.InventoryDrag.Hud==H,"running HUD lazily restores a missing drag component"))return;
   bool Hidden=false;for(auto C:L.Cells)if(C.Ref==Ref("frostthorn_staff"))Hidden=true;
   if(!Check(!Hidden,"worn equipment reserves no hidden ordinary cell"))return;
   StackKey=Key(Uid);if(!Check(!StackKey.IsEmpty()&&L.Move(StackKey,18),"seed a sparse physical cell"))return;H.RefreshData();Next(1);return;
  }
  if(Clock<.25)return;
  auto Root=H.Screen.GetRootWidget();
  if(Stage==1){if(!Check(Drop(Cell(Root,18),Gear(Root,"gloves")),"equip sparse item"))return;Next(2);return;}
  if(Stage==2){auto Target=Cell(Root,20);if(!Check(Target!=nullptr&&Target.HoverId.IsEmpty(),"exact visible empty target"))return;if(!Check(Drop(Gear(Root,"gloves"),Target),"unequip into pointed empty cell"))return;Next(3);return;}
  if(Stage==3){auto N=H.InventoryDrag.CellAt(Root,DropPoint,false);if(!Check(N!=nullptr&&N.HoverId==Uid,"item remains under drop pointer after unequip refresh"))return;
   StackKey=Key(Uid);Before=R.ExportPlayerState();H.InventoryDrag.Uid=Uid;H.InventoryDrag.Key=StackKey;H.InventoryDrag.From="stock";H.InventoryDrag.Dragged=true;
   bool Rejected=!H.InventoryDrag.Finish(Center(Gear(Root,"boots")));if(!Check(Rejected&&Before==R.ExportPlayerState(),"wrong equipment slot cancels without ownership change"))return;
   auto Filtered=L.CharacterGrid("no such item");bool FalseEmpty=false;for(auto Row:Filtered)for(auto C:L.Cells)if(Row.BagCell==L.Find(C.Key)&&!C.Ref.IsEmpty()&&Row.Value.IsEmpty())FalseEmpty=true;
   if(!Check(!FalseEmpty,"filtered occupied items are never empty drop cells"))return;
   if(!Check(L.Move(StackKey,21),"ordinary move into adjacent empty cell"))return;H.RefreshData();Next(4);return;
  }
  if(Stage==4){auto Source=Cell(Root,21),Target=Cell(Root,22);if(!Check(Drop(Source,Target),"ordinary pointer drop"))return;Next(5);return;}
  if(Stage==5){auto N=H.InventoryDrag.CellAt(Root,DropPoint,false);if(!Check(N!=nullptr&&N.HoverId==Uid,"ordinary item remains under drop pointer"))return;
   auto Saved=R.ExportPlayerState();if(!Check(R.ImportPlayerState(Saved,Error),"placement profile reload "+Error))return;H.RefreshData();Next(6);return;
  }
  if(Stage==6){auto N=H.InventoryDrag.CellAt(Root,DropPoint,false);if(!Check(N!=nullptr&&N.HoverId==Uid,"saved placement preserves the visible drop cell"))return;
   System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/InventoryDrag/character-after.png -nosuffix");Next(7);return;
  }
  if(Stage==7){auto Source=Cell(Root,22),Target=Cell(Root,21);auto B=H.ItemHover.ContextMenu.Input(Source);auto D=H.InventoryDrag;
   if(!Check(B!=nullptr&&UOpusHoverUI::TestPointerButton(B,EKeys::LeftMouseButton,false),"hover the source before pressing"))return;
   H.ItemHover.PointerAnchor=nullptr;
   if(!Check(UOpusHoverUI::TestPointerButton(B,EKeys::LeftMouseButton,true)&&D.Uid==Uid,"Slate pointer-down resolves the physical item without a hover anchor"))return;
   D.UpdatePointer(D.Start+FVector2D(3,0),B.IsPressed());if(!Check(!D.Dragged,"movement below 6px remains a click"))return;
   DropPoint=Center(Target);D.UpdatePointer(DropPoint,B.IsPressed());if(!Check(D.Dragged&&D.Ghost!=nullptr,"crossing threshold creates the drag ghost"))return;
   D.Button=nullptr;D.UpdatePointer(DropPoint,true);if(!Check(D.Dragged&&D.Ghost!=nullptr,"held drag survives a replaced source button"))return;D.Button=B;
   H.ItemHover.Enter(Target);if(!Check(H.ItemHover.Panel==nullptr,"drag does not recreate hover inspectors"))return;
   Next(71);return;
  }
  if(Stage==71){auto D=H.InventoryDrag;
   if(!Check(D.Dragged&&D.Ghost!=nullptr,"held drag survives normal component ticks"))return;
   D.UpdatePointer(DropPoint,D.Button.IsPressed());
   System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/InventoryDrag/drag-held.png -nosuffix");Next(72);return;
  }
  if(Stage==72){auto D=H.InventoryDrag;auto Source=D.Button;auto Target=Cell(Root,21);
   UOpusHoverUI::TestPointerButton(Target,EKeys::LeftMouseButton,false);D.UpdatePointer(DropPoint,Source.IsPressed());Next(8);return;
  }
  if(Stage==8){auto D=H.InventoryDrag;auto N=D.CellAt(Root,DropPoint,false);
   if(!Check(D.Uid.IsEmpty()&&D.Ghost==nullptr&&N!=nullptr&&N.HoverId==Uid,"Slate release commits at pointer and destroys ghost"))return;
   auto B=H.ItemHover.ContextMenu.Input(N);D.ConsumeClick();
   if(!Check(UOpusHoverUI::TestPointerButton(B,EKeys::LeftMouseButton,true)&&D.Uid==Uid,"second native drag begins"))return;
   D.UpdatePointer(Center(Cell(Root,22)),B.IsPressed());Before=R.ExportPlayerState();FString Selected=H.SelectedId;
   if(!Check(H.FilterKey(EKeys::Escape)&&D.Ghost==nullptr&&D.Uid.IsEmpty(),"Escape clears active drag"))return;
   UOpusHoverUI::TestPointerButton(B,EKeys::LeftMouseButton,false);
   if(!Check(Before==R.ExportPlayerState()&&Selected==H.SelectedId,"cancelled drag release neither moves nor clicks"))return;
   if(!Check(R.ImportPlayerState("{\"initialized\":true,\"owned\":{\"equipment\":{\"cinder_guard_boots\":3}},\"loadout\":{\"equipment\":{\"boots\":\"cinder_guard_boots\"}}}",Error),"stacked template fixture"))return;
   H.RefreshData();int Copies=0;for(auto C:L.Cells)if(C.Ref==Ref("cinder_guard_boots"))Copies+=C.Count;
   if(!Check(Copies==2,"one worn copy leaves exactly two ordinary copies"))return;
   int Displayed=0;for(auto Row:L.CharacterGrid(""))Displayed+=Row.Count;
   if(!Check(Displayed==2,"display does not deduct the worn copy twice"))return;
   Check(true,"Complete");Done=true;UOpusNative::Quit(true);
  }
 }
}
