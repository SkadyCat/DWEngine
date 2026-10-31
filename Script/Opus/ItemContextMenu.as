// UICreator presentation; item actions reuse native inventory transactions.
class UDWOItemContextMenu : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() UOpusWorldScreen Panel;
 UPROPERTY() UOpusActionNode Anchor;
 FString ItemId,Context,Template,Signature,Compartment;
 FString LastDropKey;
 FOpusEntry Item;
 FVector2D Position;
 int FocusIndex=0;
 int VisibleCount=0;
 bool Worn=false;
 bool CanPrimary=false,CanSell=false,CanDrop=false;
 void Initialize(UDWOSvgHUD Owner){Hud=Owner;}
 bool IsOpen() const {return Panel!=nullptr;}
 FVector2D Pointer() const
 {
  if(Panel!=nullptr&&Panel.DispatchingPointer)return Panel.PointerPosition;
  if(Hud.Screen.DispatchingPointer)return Hud.Screen.PointerPosition;
  auto PC=Gameplay::GetPlayerController(0);float32 X=0,Y=0;PC.GetMousePosition(X,Y);int W=0,H=0;PC.GetViewportSize(W,H);auto View=UOpusHoverUI::ViewSize(PC);
  return FVector2D(W>0?X*View.X/W:X,H>0?Y*View.Y/H:Y);
 }
 bool ContainsPointer() const
 {
  if(Panel==nullptr)return false;auto Point=Pointer();auto Rect=UOpusHoverUI::AnchorRect(Panel.GetRootWidget(),Gameplay::GetPlayerController(0));return Point.X>=Rect.X&&Point.X<=Rect.Z&&Point.Y>=Rect.Y&&Point.Y<=Rect.W;
 }
 void Close()
 {
  if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();}Panel=nullptr;Anchor=nullptr;ItemId="";Signature="";
 }
 bool Open(UOpusActionNode Node)
 {
  if(Node==nullptr||Node.Screen!=Hud.Screen||Node.HoverId.IsEmpty())return false;
  FOpusEntry Found;bool Has=false;for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==Node.HoverId){Found=E;Has=true;break;}
  if(!Has||(Found.Kind!="equipment"&&Found.Kind!="item")||Found.Category=="map")return false;
  FString Role=DWOInventory::Role(Node);
  if(DWOInventory::Count(Found,Role)<1)return false;
  Close();Compartment=Role;Anchor=Node;ItemId=Found.Id;Item=Found;Context=Hud.Screen.CurrentState;Hud.SelectedId=ItemId;Hud.SelectedCompartment=Role;Hud.SelectedKey=Node.PlacementKey;Hud.ItemHover.Close();
  Template=Found.Kind=="item"?"context-item":"context-"+(Found.Rarity=="normal"?"common":Found.Rarity);
  if(Template!="context-item"&&Template!="context-common"&&Template!="context-magic"&&Template!="context-rare"&&Template!="context-legendary"&&Template!="context-unique")Template="context-common";
  Panel=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Template,"UICreatorContext",350);
  if(Panel==nullptr){Hud.Tower.LastError="缺少 UICreator 右键菜单模板";Close();return false;}
  Panel.OnSourceAction.AddUFunction(this,n"OnAction");Panel.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");Position=Pointer();
  auto Rect=UOpusHoverUI::AnchorRect(Node,Gameplay::GetPlayerController(0));if(Position.X<Rect.X||Position.X>Rect.Z||Position.Y<Rect.Y||Position.Y>Rect.W)Position=FVector2D(Rect.X,Rect.W);
  Refresh();Place();FocusIndex=0;Focus(0);return true;
 }
 UOpusActionNode Button(FString Op) const {return Panel==nullptr?nullptr:Cast<UOpusActionNode>(Panel.FindSourceNode(Template+"-"+Op));}
 UButton Input(UWidget Widget) const
 {
  auto B=Cast<UButton>(Widget);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(Widget);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Input(C);if(Found!=nullptr)return Found;}return nullptr;
 }
 void Enable(FString Op,bool Value,FString Why=""){auto B=Input(Button(Op));if(B!=nullptr){B.SetIsEnabled(Value);B.SetToolTipText(FText::FromString(Why));}}
 void Refresh()
 {
  bool Found=false;for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==ItemId){Item=E;VisibleCount=DWOInventory::Count(E,Compartment);Worn=DWOInventory::Worn(E,Compartment);Found=VisibleCount>0;break;}
  if(!Found||Context!=Hud.Screen.CurrentState){Close();return;}
  FString Error;int64 Price=0;bool Equipment=Item.Kind=="equipment",Locked=Hud.Tower.Rules.IsPlayerItemLocked(ItemId);
  CanSell=Equipment&&!Locked&&Hud.Tower.Rules.QuotePlayerEquipmentSale(ItemId,Price,Error);
  FString Blocked=Locked?"该装备已被祭坛锁定":"";
  CanPrimary=Equipment&&!Locked&&!IsFixed(Item.Slot);CanDrop=!Locked&&!Hud.Tower.Rules.GetPlayerPools().bDead&&(!Equipment||!IsFixed(Item.Slot));
  FString Key=ItemId+Item.Name+Item.Icon+Worn+Item.Identified+VisibleCount+Item.Tier+Price+CanSell+CanPrimary+CanDrop;
  if(Key==Signature)return;Signature=Key;
  UOpusUI::SetImage(Panel,"context:icon",Item.Icon);UOpusUI::SetText(Panel,"context:name",Item.Name);
  UOpusUI::SetText(Panel,"context:meta",Hud.ItemHover.QualityName(Item.Rarity)+(Item.Tier>0?" · T"+Item.Tier:"")+(Item.Identified?(Worn?" · 已装备":""):" · 未鉴定"));
  UOpusUI::SetText(Panel,"context:primary-text",Item.Identified?(Worn?"卸下":"装备"):"鉴定");
  UOpusUI::SetShown(Panel,"context:identify",!Item.Identified);UOpusUI::SetShown(Panel,"context:unequip",Item.Identified&&Worn);
  UOpusUI::SetShown(Panel,"context:equip",Item.Identified&&!Worn);
  UOpusUI::SetText(Panel,"context:price","+"+Price);UOpusUI::SetText(Panel,"context:drop-text","丢到地面"+(VisibleCount>1?"  ×"+VisibleCount:""));
  for(auto B:PriceBindings())UOpusUI::SetShown(Panel,B,CanSell);
  Enable("primary",CanPrimary,Blocked);Enable("sell",CanSell,Blocked.IsEmpty()?Error:Blocked);Enable("drop",CanDrop,Blocked);
 }
 bool IsFixed(FString Slot) const {return Slot=="map"||Slot=="character"||Slot=="rarity"||Slot=="tower"||Slot=="lineage";}
 TArray<FString> PriceBindings(){TArray<FString> Out;Out.Add("context:price-box");Out.Add("context:stone");Out.Add("context:price");Out.Add("context:money");return Out;}
 void Place()
 {
  if(Panel==nullptr)return;auto View=UOpusHoverUI::ViewSize(Gameplay::GetPlayerController(0));auto Size=Panel.TemplateSize;
  double Scale=Math::Min(1.0,Math::Min((View.X-16)/Size.X,(View.Y-16)/Size.Y));
  UOpusHoverUI::Place(Panel,FVector2D(Math::Clamp(Position.X,8.0,Math::Max(8.0,View.X-Size.X*Scale-8)),Math::Clamp(Position.Y,8.0,Math::Max(8.0,View.Y-Size.Y*Scale-8))),float32(Scale));
 }
 TArray<FString> Operations() const {TArray<FString> Out;if(CanPrimary)Out.Add("primary");if(CanSell)Out.Add("sell");if(CanDrop)Out.Add("drop");return Out;}
 void Focus(int Step)
 {
  auto Ops=Operations();if(Ops.Num()==0)return;FocusIndex=(FocusIndex+Step+Ops.Num())%Ops.Num();auto B=Input(Button(Ops[FocusIndex]));if(B!=nullptr)B.SetKeyboardFocus();
 }
 UFUNCTION() bool FilterKey(FKey Key)
 {
  if(Panel==nullptr)return false;
  if(Key==EKeys::Escape||Key==EKeys::Tab){Close();return true;}
  if(Key==EKeys::LeftMouseButton){if(!ContainsPointer())Close();return false;}
  if(Key==EKeys::RightMouseButton){if(ContainsPointer())return true;Close();return false;}
  if(Key==EKeys::Down||Key==EKeys::Up){Focus(Key==EKeys::Down?1:-1);return true;}
  if(Key==EKeys::Home||Key==EKeys::End){auto Ops=Operations();FocusIndex=Key==EKeys::Home?0:Math::Max(0,Ops.Num()-1);Focus(0);return true;}
  if(Key==EKeys::Enter||Key==EKeys::SpaceBar){auto Ops=Operations();if(Ops.IsValidIndex(FocusIndex))Execute(Ops[FocusIndex]);return true;}return false;
 }
 UFUNCTION() void OnAction(USvgCanvasNode Node){if(Panel!=nullptr)Panel.SourceActionHandled=true;Execute(UOpusUI::Action(Node).Op);}
 void Execute(FString Op)
 {
  if(Panel==nullptr)return;Refresh();if(Panel==nullptr)return;
  if((Op=="primary"&&!CanPrimary)||(Op=="sell"&&!CanSell)||(Op=="drop"&&!CanDrop))return;
  FString Error,DropKey;bool Good=false;
  if(Op=="primary")
  {
   if(!Item.Identified){FString Uid;Good=Hud.Tower.Rules.IdentifyPlayerEquipment(FName(ItemId),Uid,Error);if(Good)Hud.SelectedId=Uid;}
   else{Good=Hud.Tower.Rules.EquipPlayerItem(FName(Item.Slot),Worn?NAME_None:FName(ItemId),Error);}
  }
  else if(Op=="sell"){int64 Price=0;Good=Hud.Tower.Rules.SellPlayerEquipment(ItemId,Price,Error);}
  else if(Op=="drop")Good=Hud.Tower.Rules.DropPlayerInventory(FName(Item.Kind),ItemId,VisibleCount,Hud.Tower.Hero.CombatLocation(),DropKey,Error);
  else return;
  // A worn template represents one copy. Selling/dropping that copy must not
  // silently wear another identical spare remaining in ordinary inventory.
  if(Good&&Worn&&(Op=="sell"||Op=="drop"))for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==ItemId&&E.Equipped){FString WearError;Hud.Tower.Rules.EquipPlayerItem(FName(E.Slot),NAME_None,WearError);if(!WearError.IsEmpty())Error=WearError;break;}
  Hud.Tower.LastError=Error;Hud.RefreshIn=0;Hud.RefreshPart=0;
  if(Good){if(Op=="primary"&&Item.Identified)Hud.SelectedCompartment=Worn?"stock":"equipped";Hud.Tower.SaveProfile();if(Op=="drop"){LastDropKey=DropKey;Hud.ShowLoot=true;}Close();}else{Signature="";Refresh();}
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt){if(Panel!=nullptr){Refresh();Place();}}
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){Close();}
}
