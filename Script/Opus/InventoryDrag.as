// Pointer threshold, physical cells and icon-only ghost mirror Studio.
class UDWOInventoryDrag : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;UPROPERTY() UButton Button;UPROPERTY() UOpusWorldScreen Ghost;
 FString Uid,From,Key,State;FVector2D Start;bool Held=false,Dragged=false,SkipClick=false;double SkipUntil=0,Clock=0;
 void Initialize(UDWOSvgHUD H){Hud=H;}
 USvgCanvasNode Host(UWidget W,FString Binding){if(W==nullptr||!W.IsVisible())return nullptr;auto N=Cast<USvgCanvasNode>(W);if(N!=nullptr&&N.Binding==Binding)return N;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Match=Host(C,Binding);if(Match!=nullptr)return Match;}return nullptr;}
 FVector2D Pointer(){return Hud.ItemHover.ContextMenu.Pointer();}
 bool Allowed(){return Hud.Screen.CurrentState=="inventory"||Hud.Screen.CurrentState=="equipment"||Hud.Screen.CurrentState=="character";}
 bool Contains(UWidget W,FVector2D P){auto R=UOpusHoverUI::AnchorRect(W,Gameplay::GetPlayerController(0));return R.Z>R.X&&P.X>=R.X&&P.X<=R.Z&&P.Y>=R.Y&&P.Y<=R.W;}
 UOpusActionNode CellAt(UWidget W,FVector2D P,bool Worn)
 {
  if(W==nullptr||!W.IsVisible()||!Contains(W,P))return nullptr;auto N=Cast<UOpusActionNode>(W);
  if(N!=nullptr&&((!Worn&&N.BagCell>=0)||(Worn&&!N.EquipmentSlot.IsEmpty()))&&DWOInventory::Role(N)==(Worn?"equipped":"stock"))return N;
  auto Panel=Cast<UPanelWidget>(W);if(Panel!=nullptr)for(auto C:Panel.GetAllChildren()){auto Match=CellAt(C,P,Worn);if(Match!=nullptr)return Match;}return nullptr;
 }
 void Press()
 {
  Cancel();if(!Allowed()||!Hud.Screen.DispatchingPointer||Hud.ItemHover.ContextMenu.IsOpen())return;
  // Hover can still refer to a row replaced by the inventory refresh. Resolve
  // the physical cell under this press instead of depending on hover delivery.
  auto Position=Pointer();auto N=CellAt(Hud.Screen.GetRootWidget(),Position,false);if(N==nullptr)N=CellAt(Hud.Screen.GetRootWidget(),Position,true);
  if(N==nullptr)return;FString Role=DWOInventory::Role(N);if(Role.IsEmpty())return;
  for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==N.HoverId&&!E.Locked&&DWOInventory::Count(E,Role)>0){Uid=E.Id;From=Role;Key=N.PlacementKey;State=Hud.Screen.CurrentState;Button=Hud.ItemHover.ContextMenu.Input(N);Start=Pointer();return;}
 }
 bool ConsumeClick(){bool Out=SkipClick&&Clock<SkipUntil;SkipClick=false;return Out;}
 void Cancel(){if(Dragged){SkipClick=true;SkipUntil=Clock+.3;}Button=nullptr;Uid="";From="";Key="";Held=false;Dragged=false;if(Ghost!=nullptr)Ghost.RemoveFromParent();Ghost=nullptr;}
 bool Finish(FVector2D P)
 {
  bool Good=false;if(!Uid.IsEmpty()&&Dragged&&Allowed())for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==Uid&&!E.Locked&&DWOInventory::Count(E,From)>0){
   FString Error;auto Wear=CellAt(Hud.Screen.GetRootWidget(),P,true);auto Bag=CellAt(Hud.Screen.GetRootWidget(),P,false);
   if(Wear!=nullptr&&From=="stock"&&DWOInventory::Wearable(E)&&E.Identified&&Wear.EquipmentSlot==E.Slot){Good=Hud.Tower.Rules.EquipPlayerItem(FName(E.Slot),FName(Uid),Error);if(Good)Hud.SelectedCompartment="equipped";}
   else if(Bag!=nullptr&&Bag.HoverId.IsEmpty()){
    if(From=="equipped"){Good=Hud.Tower.Rules.EquipPlayerItem(FName(E.Slot),NAME_None,Error);if(Good){Hud.Bag=UOpusData::Inventory(Hud.Tower.Rules);Hud.InventoryLayout.Reconcile();if(Key.IsEmpty())for(auto C:Hud.InventoryLayout.Cells)if(C.Ref==E.Key){Key=C.Key;break;}}}else Good=true;
    if(Good)Good=Hud.InventoryLayout.Move(Key,Bag.BagCell);if(Good)Hud.SelectedCompartment="stock";
   }
   if(Good){Hud.SelectedId=Uid;Hud.SelectedKey=Key;Hud.Tower.SaveProfile();Hud.RefreshDataPart(0);Hud.RefreshDataPart(1);}Hud.Tower.LastError=Error;break;
  }
  if(Dragged){SkipClick=true;SkipUntil=Clock+.3;}Cancel();return Good;
 }
 void UpdatePointer(FVector2D P,bool Pressed)
 {
  if(Uid.IsEmpty())return;if(!Allowed()||State!=Hud.Screen.CurrentState||(!Held&&!Pressed)){Cancel();return;}if(Pressed)Held=true;
  if(Held&&(P-Start).Size()>6)Dragged=true;
  if(Dragged){SkipClick=true;SkipUntil=Clock+.3;Hud.ItemHover.Close();if(Ghost==nullptr){Ghost=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"inventory-drag-icon","UICreatorStudio",300);if(Ghost!=nullptr){Ghost.SetVisibility(ESlateVisibility::HitTestInvisible);for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==Uid)UOpusUI::SetImage(Ghost,"inventory-drag-picture",E.Icon);}}if(Ghost!=nullptr)UOpusHoverUI::Place(Ghost,P+FVector2D(12,12),1);}
  if(Held&&!Pressed)Finish(P);
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt){Clock+=Dt;if(!Uid.IsEmpty())UpdatePointer(Pointer(),UOpusMapUI::MouseDown()||(Button!=nullptr&&Button.IsPressed()));}
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){Cancel();}
}
