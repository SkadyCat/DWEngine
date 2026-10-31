// Port of UICreator ItemInspector. State/actions stay in AS; the panel uses the
// saved UICreator inspector; crafting uses the existing native transactions.
class UDWOItemHover : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() UOpusHoverEvents Events;
 UPROPERTY() UOpusActionNode Anchor;
 UPROPERTY() UOpusWorldScreen Panel;
 UPROPERTY() UOpusWorldScreen DetailPanel;
 UPROPERTY() UOpusActionNode DetailAnchor;
 UPROPERTY() UOpusHoverEvents PanelEvents;
 UPROPERTY() UOpusActionNode PointerAnchor;
 UPROPERTY() UDWOItemContextMenu ContextMenu;
 FString ItemId,Context,Rarity,Signature;
 FString CurrencyId,OmenId,Feedback,DetailKey;
 bool DetailPinned=false;
 TArray<FOpusHoverLine> CurrentLines;
 TArray<FOpusEntry> Materials;
 FOpusCraftState Craft;
 bool Pinned=false,ReadOnly=false;
 double RefreshIn=0;
 void Initialize(UDWOSvgHUD InHud)
 {
  Hud=InHud;Events=UOpusUI::HoverEvents(Hud.Screen);
  ContextMenu=UDWOItemContextMenu::Create(Hud.Tower);ContextMenu.Initialize(Hud);
  Events.Entered.AddUFunction(this,n"Enter");Events.Left.AddUFunction(this,n"Leave");
  Hud.Screen.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");
 }
 UFUNCTION() void Enter(USvgCanvasNode Node)
 {
  auto N=Cast<UOpusActionNode>(Node);
  PointerAnchor=N;
  if(Hud.InventoryDrag!=nullptr&&Hud.InventoryDrag.Dragged)return;
  if(N!=nullptr&&Hud.NewItems!=nullptr&&Hud.NewItems.MarkSeen(N.HoverId)){Hud.RefreshDataPart(1);}
  if(Pinned||(ContextMenu!=nullptr&&ContextMenu.IsOpen())||N==nullptr||N.HoverId.IsEmpty())return;
  if(ItemId==N.HoverId&&Anchor==N)return;
  Close();Anchor=N;ItemId=N.HoverId;Context=Hud.Screen.CurrentState;ReadOnly=N.Screen!=Hud.Screen;
  auto PC=Gameplay::GetPlayerController(0);Pinned=PC!=nullptr&&(PC.IsInputKeyDown(EKeys::LeftControl)||PC.IsInputKeyDown(EKeys::RightControl));
  Refresh();Place();
 }
 UFUNCTION() void Leave(USvgCanvasNode Node){auto N=Cast<UOpusActionNode>(Node);if(PointerAnchor==N)PointerAnchor=nullptr;if(!Pinned&&N==Anchor)Close();}
 void CloseDetail(){if(DetailPanel!=nullptr)DetailPanel.RemoveFromParent();DetailPanel=nullptr;DetailAnchor=nullptr;DetailKey="";DetailPinned=false;}
 void Close(){CloseDetail();if(PanelEvents!=nullptr){PanelEvents.Entered.UnbindObject(this);PanelEvents.Left.UnbindObject(this);}PanelEvents=nullptr;if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();}Panel=nullptr;Anchor=nullptr;ItemId="";Signature="";Pinned=false;CurrencyId="";OmenId="";Feedback="";}
 void Pin(){if(ItemId.IsEmpty()||Pinned)return;Pinned=true;Signature="";Refresh();Place();}
 bool SelectPinned(USvgCanvasNode Node,FString Id)
 {
  UOpusActionNode Target=nullptr;UWidget Current=Node;
  while(Current!=nullptr){auto Candidate=Cast<UOpusActionNode>(Current);if(Candidate!=nullptr&&Candidate.HoverId==Id){Target=Candidate;break;}Current=Current.GetParent();}
  if(Target==nullptr)return false;
  Close();Enter(Target);Pin();return Panel!=nullptr;
 }
 bool HandleKeys(APlayerController PC)
 {
  if(ContextMenu!=nullptr&&ContextMenu.IsOpen())
  {
   TArray<FKey> MenuKeys;MenuKeys.Add(EKeys::Escape);MenuKeys.Add(EKeys::Tab);MenuKeys.Add(EKeys::Up);MenuKeys.Add(EKeys::Down);MenuKeys.Add(EKeys::Home);MenuKeys.Add(EKeys::End);MenuKeys.Add(EKeys::Enter);MenuKeys.Add(EKeys::SpaceBar);
   for(auto Key:MenuKeys)if(PC.WasInputKeyJustPressed(Key))return ContextMenu.FilterKey(Key);
  }
  if(ItemId.IsEmpty())return false;
  if(DetailPanel!=nullptr)
  {
   if(PC.WasInputKeyJustPressed(EKeys::LeftControl)||PC.WasInputKeyJustPressed(EKeys::RightControl)){DetailPinned=true;RefreshDetail();return true;}
   if(PC.WasInputKeyJustPressed(EKeys::Escape)){CloseDetail();return true;}
  }
  if(PC.WasInputKeyJustPressed(EKeys::LeftControl)||PC.WasInputKeyJustPressed(EKeys::RightControl))Pin();
  if(PC.WasInputKeyJustPressed(EKeys::Escape)){Close();return true;}
  return false;
 }
 UFUNCTION() bool FilterKey(FKey Key)
 {
  if(Key==EKeys::RightMouseButton||(Key==EKeys::F10&&(Gameplay::GetPlayerController(0).IsInputKeyDown(EKeys::LeftShift)||Gameplay::GetPlayerController(0).IsInputKeyDown(EKeys::RightShift))))
  {
   if(IsValid(PointerAnchor)&&ContextMenu.Open(PointerAnchor))return true;
  }
  if(ContextMenu!=nullptr&&ContextMenu.FilterKey(Key))return true;
  if(ItemId.IsEmpty())return false;
  if(Key==EKeys::LeftMouseButton&&Pinned&&!ContainsPanel(Panel)&&!ContainsPanel(DetailPanel)){Close();return false;}
  if(DetailPanel!=nullptr)
  {
   if(Key==EKeys::LeftControl||Key==EKeys::RightControl){DetailPinned=true;RefreshDetail();return true;}
   if(Key==EKeys::Escape){CloseDetail();return true;}
  }
  if(Key==EKeys::LeftControl||Key==EKeys::RightControl){Pin();return true;}
  if(Key==EKeys::Escape){Close();return true;}return false;
 }
 bool ContainsPanel(UOpusWorldScreen S){if(S==nullptr)return false;auto P=ContextMenu.Pointer();auto R=UOpusHoverUI::AnchorRect(S.GetRootWidget(),Gameplay::GetPlayerController(0));return P.X>=R.X&&P.X<=R.Z&&P.Y>=R.Y&&P.Y<=R.W;}
 FString Quality(FString V) const {return V=="normal"||V=="magic"||V=="rare"||V=="legendary"||V=="unique"||V=="skill"?V:"common";}
 FString QualityName(FString V) const {return V=="legendary"?"传奇":V=="rare"?"稀有":V=="magic"?"魔法":V=="unique"?"独特":V=="skill"?"技能":"普通";}
 void Refresh()
 {
  if(ItemId.IsEmpty())return;
  FOpusEntry Item;bool Found=false;for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==ItemId){Item=E;Found=true;break;}
  if(!Found||Context!=Hud.Screen.CurrentState){Close();return;}
  FString Q=Item.Kind=="skill"?"skill":Quality(Item.Rarity);
  CurrentLines=UOpusData::HoverLines(Hud.Tower.Rules,Item.Id);Materials.Empty();bool HasCurrency=false,HasOmen=false;
  for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Count>0&&(E.Category=="currency"||E.Category=="omen")){Materials.Add(E);if(E.Id==CurrencyId)HasCurrency=true;if(E.Id==OmenId)HasOmen=true;}
  if(!HasCurrency)CurrencyId="";if(!HasOmen)OmenId="";
  FString Key=Item.Id+Item.Name+Item.Icon+Item.Description+Q+Item.Count+Item.Tier+(Item.Identified?"i":"u")+(Item.Equipped?"1":"0")+(Pinned?"p":"h")+CurrencyId+OmenId+Feedback;
  for(auto L:CurrentLines)Key+=L.Text+L.Tier+L.Grade;for(auto M:Materials)Key+=M.Id+M.Count;
  if(Signature==Key&&Panel!=nullptr)return;
  if(Panel==nullptr||Rarity!=Q)
  {
   if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();}
   Panel=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"hover-"+Q,"UICreatorHover",ReadOnly?180:120);
   if(Panel==nullptr){Hud.Tower.LastError="缺少 UICreator 物品悬停模板";Close();return;}
   Panel.OnSourceAction.AddUFunction(this,n"OnAction");Rarity=Q;
   Panel.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");
   PanelEvents=UOpusUI::HoverEvents(Panel);PanelEvents.Entered.AddUFunction(this,n"EnterDetail");PanelEvents.Left.AddUFunction(this,n"LeaveDetail");
  }
  FString Type=Item.Kind=="equipment"?QualityName(Q)+" · "+Hud.SlotName(Item.Slot):Item.Kind=="skill"?"技能":Item.Category=="map"?"地图物品 · T"+Item.Tier:Item.Category=="omen"?"预兆":Item.Category=="currency"?"通货":"消耗品";
  Craft=UOpusData::CraftState(Hud.Tower.Rules,Item.Id,FName(CurrencyId),FName(OmenId));
  UOpusHoverUI::Populate(Panel,Item,Type+" · 数量 "+Item.Count,CurrentLines,Pinned,Materials,Craft,CurrencyId,OmenId,Feedback,ReadOnly);Signature=Key;
  if(!DetailKey.IsEmpty()){DetailAnchor=FindHover(Panel.GetRootWidget(),DetailKey);if(DetailAnchor==nullptr)CloseDetail();else RefreshDetail();}
 }
 void Place()
 {
  if(Panel==nullptr||Anchor==nullptr)return;auto PC=Gameplay::GetPlayerController(0);
  FVector4 A=UOpusHoverUI::AnchorRect(Anchor,PC);if(A.Z<=A.X||A.W<=A.Y)return;
  FVector2D V=UOpusHoverUI::ViewSize(PC),Size=Panel.TemplateSize;
  double Scale=Math::Min(1.0,Math::Min((V.X-24)/Size.X,(V.Y-24)/Size.Y));Scale=Math::Max(0.1,Scale);
  double W=Size.X*Scale,H=Size.Y*Scale,X=A.Z+10,Y=A.Y;
  if(X+W>V.X-12){X=A.X-W-10;if(X<12){X=A.X;Y=A.Y-H-10>=12?A.Y-H-10:A.W+10;}}
  X=Math::Max(12.0,Math::Min(X,V.X-W-12));Y=Math::Max(12.0,Math::Min(Y,V.Y-H-12));
  UOpusHoverUI::Place(Panel,FVector2D(X,Y),float32(Scale));
  PlaceDetail();
 }
 UOpusActionNode FindHover(UWidget Widget,FString Key)
 {
  auto N=Cast<UOpusActionNode>(Widget);if(N!=nullptr&&N.HoverId==Key)return N;
  auto P=Cast<UPanelWidget>(Widget);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=FindHover(C,Key);if(Found!=nullptr)return Found;}return nullptr;
 }
 UFUNCTION() void EnterDetail(USvgCanvasNode Node)
 {
  if(!Pinned)return;auto N=Cast<UOpusActionNode>(Node);if(N==nullptr||N.HoverId.IsEmpty()||(DetailPinned&&DetailKey.StartsWith("line:")))return;
  DetailAnchor=N;DetailKey=N.HoverId;RefreshDetail();
 }
 UFUNCTION() void LeaveDetail(USvgCanvasNode Node){auto N=Cast<UOpusActionNode>(Node);if(!DetailPinned&&N==DetailAnchor)CloseDetail();}
 void RefreshDetail()
 {
  FString Title,Meta,Description,Icon,Hint="Ctrl 固定说明";
  if(DetailKey.StartsWith("line:"))
  {
   int I=String::Conv_StringToInt(DetailKey.Mid(5));if(!CurrentLines.IsValidIndex(I)){CloseDetail();return;}
   auto L=CurrentLines[I];if(L.Description.IsEmpty()||L.Description==L.Text){CloseDetail();return;}Title=L.Text;Description=L.Description;Meta=L.Style==2?"随机词条":"固定属性";Hint=DetailPinned?"已固定 · Esc 或 × 关闭":"点击词条固定说明";
  }
  else
  {
   bool Found=false;for(auto E:Materials)if(DetailKey==E.Category+":"+E.Id){Found=true;Title=E.Name;Description=E.Description;Icon=E.Icon;Meta=(E.Category=="currency"?"通货":"预兆")+" · 持有 "+E.Count;Hint=(E.Id==(E.Category=="currency"?CurrencyId:OmenId)?"已激活 · 再次点击取消":"点击图标激活")+(DetailPinned?" · 已固定":" · Ctrl 固定说明");break;}
   if(!Found){CloseDetail();return;}
  }
  if(DetailPanel==nullptr){DetailPanel=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"hover-detail","UICreatorHover",ReadOnly?190:130);if(DetailPanel==nullptr)return;DetailPanel.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");DetailPanel.OnSourceAction.AddUFunction(this,n"OnDetailAction");}
  UOpusHoverUI::PopulateDetail(DetailPanel,Title,Meta,Description,Icon,Hint,DetailPinned);PlaceDetail();
 }
 UFUNCTION() void OnDetailAction(USvgCanvasNode Node){if(UOpusUI::Action(Node).Op=="detail-close")CloseDetail();}
 void PlaceDetail()
 {
  if(DetailPanel==nullptr||Panel==nullptr||DetailAnchor==nullptr)return;auto PC=Gameplay::GetPlayerController(0);
  auto Owner=UOpusHoverUI::AnchorRect(Panel.GetRootWidget(),PC);auto A=UOpusHoverUI::AnchorRect(DetailAnchor,PC);auto V=UOpusHoverUI::ViewSize(PC);auto Size=DetailPanel.TemplateSize;
  double Scale=Math::Min(1.0,(V.Y-24)/Size.Y),W=Size.X*Scale,H=Size.Y*Scale,X=Owner.Z+10,Y=A.Y;
  if(X+W>V.X-12)X=Owner.X-W-10;X=Math::Clamp(X,12.0,Math::Max(12.0,V.X-W-12));Y=Math::Clamp(Y,12.0,Math::Max(12.0,V.Y-H-12));UOpusHoverUI::Place(DetailPanel,FVector2D(X,Y),float32(Scale));
 }
 FString MaterialName(FString Id) const {for(auto E:Materials)if(E.Id==Id)return E.Name;return Id;}
 void RunCraft(int Index)
 {
  if(!Pinned||!Craft.Blocked.IsEmpty()||(Index>=0&&!Craft.EligibleAffixes.Contains(Index)))return;
  FString CurrencyName=MaterialName(CurrencyId),OmenName=MaterialName(OmenId),Error;
  if(Hud.Tower.Rules.CraftPlayerEquipment(ItemId,FName(CurrencyId),FName(OmenId),Index,Error))
  {
   Feedback=(Craft.Operation=="roll_affixes"?"已重新随机生成 "+Craft.Count+" 条词缀":Craft.Operation=="add_affixes"?"已随机增加 "+Craft.Count+" 条词条":Craft.Operation=="remove_affixes"?"已随机移除 "+Craft.Count+" 条词缀":"第 "+(Index+1)+" 条词条已洗炼")+"，消耗 1 "+CurrencyName+(OmenId.IsEmpty()?"":"和 1 个"+OmenName);
   Hud.Tower.SaveProfile();Hud.RefreshIn=0;Hud.RefreshPart=0;
  }
  else Feedback=Error;
  Hud.Tower.LastError=Error;Signature="";Refresh();Place();
 }
 UFUNCTION() void OnAction(USvgCanvasNode Node)
 {
  auto A=UOpusUI::Action(Node);if(A.Op=="hover-close"){Close();return;}if(!Pinned)return;
  if(ReadOnly&&A.Op!="hover-effect"&&A.Op!="hover-affix")return;
  Hud.SelectedId=ItemId;FString Error;
  if(A.Op=="hover-currency"||A.Op=="hover-omen")
  {
   bool Currency=A.Op=="hover-currency",Found=false;for(auto E:Materials)if(E.Id==A.Value&&E.Category==(Currency?"currency":"omen"))Found=true;if(!Found)return;
   if(Currency)CurrencyId=CurrencyId==A.Value?"":A.Value;else OmenId=OmenId==A.Value?"":A.Value;
   Feedback="";Signature="";Refresh();Place();return;
  }
  if(A.Op=="hover-whole"){RunCraft(-1);return;}
  if(A.Op=="hover-affix")
  {
   int Index=String::Conv_StringToInt(A.Value);if(Craft.Blocked.IsEmpty()&&Craft.EligibleAffixes.Contains(Index)){CloseDetail();RunCraft(Index);return;}
   for(int I=0;I<CurrentLines.Num();I++)if(CurrentLines[I].Style==2&&CurrentLines[I].Index==Index){DetailKey="line:"+I;DetailAnchor=Cast<UOpusActionNode>(Node);DetailPinned=true;RefreshDetail();break;}return;
  }
  if(A.Op=="hover-effect"){DetailKey="line:"+A.Value;DetailAnchor=Cast<UOpusActionNode>(Node);DetailPinned=true;RefreshDetail();return;}
  if(A.Op=="hover-use-map"){if(Hud.Tower.MapItems.Use(ItemId,Error)){Close();return;}Hud.Tower.LastError=Error;Feedback=Error;Signature="";Refresh();return;}
  if(A.Op=="hover-equip"){Hud.EquipSelected(Error);Hud.Tower.LastError=Error;Hud.RefreshIn=0;Hud.RefreshPart=0;Signature="";Refresh();Hud.Tower.SaveProfile();}
  else if(A.Op=="hover-identify")
  {
   if(!Pinned)return;
   bool Found=false,Identified=false;for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==ItemId){Found=true;Identified=E.Identified;break;}
   if(!Found){Refresh();return;}
   // RuleStudio identifies the selected item and retargets this pinned hover
   // to the resulting instance. Keep the current page, anchor and panel alive.
   FString Uid;if(!Hud.Tower.Rules.IdentifyPlayerEquipment(FName(ItemId),Uid,Error)){Hud.Tower.LastError=Error;return;}
   ItemId=Uid;Hud.SelectedId=Uid;Hud.Tower.LastError="";
   Hud.RefreshIn=0;Hud.RefreshPart=0;Signature="";Refresh();Place();Hud.Tower.SaveProfile();
  }
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Hud==nullptr||Hud.Screen==nullptr||ItemId.IsEmpty())return;
  if(Context!=Hud.Screen.CurrentState||!IsValid(Anchor)||(!Pinned&&Anchor.HoverId!=ItemId)){Close();return;}
  RefreshIn-=DeltaSeconds;if(RefreshIn<=0){RefreshIn=0.1;Refresh();}Place();
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){Close();if(Hud!=nullptr&&IsValid(Hud.Screen))Hud.Screen.InputFilter=FSvgCanvasInputFilter();if(Events!=nullptr){Events.Entered.UnbindObject(this);Events.Left.UnbindObject(this);}}
}
