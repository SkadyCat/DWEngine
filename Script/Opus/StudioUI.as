// UICreator owns every brush and layout. AS supplies state, data and transactions.
class UDWOStudioUI : UActorComponent
{
 UPROPERTY() UDWOStudioGameplay Game;
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UOpusWorldScreen Panel;
 UPROPERTY() UOpusWorldScreen MerchantTag;
 UPROPERTY() UOpusWorldScreen PortalTag;
 UPROPERTY() UOpusWorldScreen Rail;
 UPROPERTY() UOpusWorldScreen Hover;
 UPROPERTY() UOpusWorldScreen PurchaseMenu;
 int PurchaseIndex=-1;FString PurchaseKey;FVector2D PurchasePosition;bool CanPurchase=false;
 UPROPERTY() UOpusActionNode HoverAnchor;
 UPROPERTY() TArray<UOpusWorldScreen> Brushes;
 UPROPERTY() UOpusHoverEvents HoverEvents;
 FString View,Status,PortalMode;
 int Selected=-1,HoverIndex=-1;bool WasPaused=false,Suppress=false,HoverPinned=false;
 double Poll=0;
 void Initialize(UDWOStudioGameplay G)
 {
  Game=G;Tower=G.Tower;Rail=Create("studio-trait-launcher",125);
  for(auto Id:BrushIds()){auto B=Create(Id,150,false);Brushes.Add(B);}
 }
 TArray<FString> BrushIds(){TArray<FString> Out;Out.Add("merchant-jade-normal");Out.Add("merchant-jade-selected");Out.Add("merchant-gold-normal");Out.Add("merchant-gold-selected");return Out;}
 UOpusWorldScreen Create(FString Id,int Layer=150,bool Viewport=true)
 {
  auto S=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Id,"UICreatorStudio",Layer,Viewport);
  if(S==nullptr){Tower.LastError="缺少 UICreator 同步资源："+Id;return nullptr;}
  S.OnSourceAction.AddUFunction(this,n"Action");S.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");return S;
 }
 bool Open(FString Id)
 {
  if(Tower.Traveling||Tower.Preparing||Tower.Rules.GetPlayerPools().bDead||Tower.Events.IsOpen()||Tower.Hud.MapDevice.Visible)return false;
  if(Id=="merchant"&&!Game.EnsureStock())return false;
  if(Tower.Recent.UI.Panel!=nullptr||Tower.Hud.Talents.Visible)return false;
  Close();Tower.Hud.ItemHover.Close();Tower.Hud.Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Tower.Rules)+1));
  WasPaused=Tower.Paused;Tower.SetPaused(true);View=Id;Selected=-1;Status="";
  Panel=Create(Id=="merchant"?"merchant-shop":"trait-equipment");if(Panel==nullptr){View="";Tower.SetPaused(WasPaused);return false;}
  if(Id=="merchant")
  {
   for(int I=0;I<9;I++){UOpusUI::SetAction(Panel,"stock-"+I,"select",""+I);UOpusWorldUI::SetHover(Panel,"stock-"+I,""+I);}
   HoverEvents=UOpusUI::HoverEvents(Panel);HoverEvents.Entered.AddUFunction(this,n"EnterHover");HoverEvents.Left.AddUFunction(this,n"LeaveHover");
  }
  Refresh();UOpusMapUI::Fit(Panel,Gameplay::GetPlayerController(0));return true;
 }
 void CloseHover(){if(Hover!=nullptr)Hover.RemoveFromParent();Hover=nullptr;HoverAnchor=nullptr;HoverIndex=-1;HoverPinned=false;}
 void Close()
 {
  ClosePurchaseMenu();CloseHover();if(HoverEvents!=nullptr){HoverEvents.Entered.UnbindObject(this);HoverEvents.Left.UnbindObject(this);}HoverEvents=nullptr;
  if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();Tower.SetPaused(WasPaused);}Panel=nullptr;View="";Selected=-1;
 }
 void ShowMerchant()
 {
  if(!IsValid(Game.Merchant)||MerchantTag!=nullptr)return;
  MerchantTag=Create("merchant-launcher",120,false);if(MerchantTag==nullptr)return;
  Game.Merchant.Bubble.SetDrawSize(MerchantTag.TemplateSize);Game.Merchant.Bubble.SetWidget(MerchantTag);
 }
 bool BlocksPointer() const {return MerchantTag!=nullptr&&MerchantTag.IsVisible()&&IsValid(Game.Merchant)&&Game.Merchant.Bubble.IsVisible()&&MerchantTag.IsHovered();}
 void ShowPortal(){PortalMode="";RefreshPortal();}
 void RefreshPortal()
 {
  if(!Game.PortalReady)return;FString Mode=Game.Approaching?"travel":(Tower.Hero.ActorLocation-Game.PortalPosition).Size2D()<=220?"ready":"normal";
  if(Mode==PortalMode)return;PortalMode=Mode;if(PortalTag!=nullptr)PortalTag.RemoveFromParent();PortalTag=Create("portal-"+Mode,120);
 }
 void Refresh()
 {
  if(Panel==nullptr)return;
  if(View=="merchant")
  {
   auto Maps=Game.MapOffers();auto Stock=Tower.Recent.State.MerchantStock;
   UOpusUI::SetText(Panel,"shop-wallet","灵石 "+UOpusNative::Stones(Tower.Rules));
   int64 Left=Math::Max(int64(0),(Tower.Recent.State.MerchantPeriod+1)*1800-UOpusStudioBridge::UnixSeconds());
   UOpusUI::SetText(Panel,"shop-refresh","刷新 "+(Left/60)+"分"+(Left%60)+"秒");
   UOpusUI::SetText(Panel,"shop-message",Status.IsEmpty()?"左键选中 · 悬停查看详情 · 右键打开购买菜单 · Esc 返回":Status);
   for(int I=0;I<9;I++)
   {
    FOpusEntry E;int64 Cost=0;bool Sold=false,Found=false;
    if(I<Maps.Num()){E=Maps[I];Cost=E.Tier*20;Found=true;}else if(Stock.IsValidIndex(I-Maps.Num())){auto S=Stock[I-Maps.Num()];E=UOpusGameplayStore::Entry(Tower.Rules,S.Payload);Cost=S.Price;Sold=S.Claimed;Found=true;}
    FString B="stock-"+I;UOpusUI::SetShown(Panel,B,Found);if(!Found)continue;
    UOpusUI::SetText(Panel,B+"-name",E.Name);UOpusUI::SetText(Panel,B+"-meta",I<Maps.Num()?"无限供应":"传奇 · T"+E.Tier+" · 限量 1 件");UOpusUI::SetText(Panel,B+"-buy",Sold?"已售出":""+Cost+" 灵石");
    UOpusUI::SetImage(Panel,B+"-icon",E.Icon);
    int Brush=(I<Maps.Num()?0:2)+(I==Selected?1:0);if(Brushes.IsValidIndex(Brush)&&Brushes[Brush]!=nullptr)UOpusWorldUI::CopyBrush(Panel,B,Brushes[Brush],BrushIds()[Brush]);
    auto N=Panel.FindSourceNode("merchant-shop-stock-"+I);if(N!=nullptr)N.SetInteractionEnabled(!Sold);
   }
  }
 }
 UFUNCTION() void Action(USvgCanvasNode Node)
 {
  if(Node==nullptr)return;auto Source=Cast<UOpusActionNode>(Node);if(Source!=nullptr&&Source.Screen!=nullptr)Source.Screen.SourceActionHandled=true;auto A=UOpusUI::Action(Node);
  if(A.Op=="purchase-confirm"){ConfirmPurchase();return;}
  if(A.Op=="hover-close"){CloseHover();return;}
  if(A.Type=="studio-equipment"&&A.Op=="open"){Open("trait");return;}
  if(A.Op=="close"){Close();return;}
  if(A.Op=="open"||Node.Binding=="merchant:activate"){Open("merchant");return;}
  if(A.Op=="world-map"){Game.ActivatePortal();RefreshPortal();return;}
  if(View=="merchant"&&A.Op=="select"){ClosePurchaseMenu();int I=String::Conv_StringToInt(A.Value);Selected=Selected==I?-1:I;Refresh();return;}
  if(A.Type=="studio-equipment"&&A.Op=="drop"){FString Key;if(Game.DropTrait(A.Value,Key))Close();else Status=Tower.LastError;}
 }
 void ClosePurchaseMenu(){if(PurchaseMenu!=nullptr)PurchaseMenu.RemoveFromParent();PurchaseMenu=nullptr;PurchaseIndex=-1;PurchaseKey="";CanPurchase=false;}
 FVector2D Pointer(){if(PurchaseMenu!=nullptr&&PurchaseMenu.DispatchingPointer)return PurchaseMenu.PointerPosition;if(Panel!=nullptr&&Panel.DispatchingPointer)return Panel.PointerPosition;return Tower.Hud.ItemHover.ContextMenu.Pointer();}
 bool InsidePurchase(){if(PurchaseMenu==nullptr)return false;auto P=Pointer();auto R=UOpusHoverUI::AnchorRect(PurchaseMenu.GetRootWidget(),Gameplay::GetPlayerController(0));return P.X>=R.X&&P.X<=R.Z&&P.Y>=R.Y&&P.Y<=R.W;}
 void PlacePurchase(){if(PurchaseMenu==nullptr)return;auto V=UOpusHoverUI::ViewSize(Gameplay::GetPlayerController(0));float32 Scale=float32(Math::Min(1.0,Math::Min((V.X-16)/240,(V.Y-16)/108)));UOpusHoverUI::Place(PurchaseMenu,FVector2D(Math::Clamp(PurchasePosition.X,8.0,V.X-240*Scale-8),Math::Clamp(PurchasePosition.Y,8.0,V.Y-108*Scale-8)),Scale);}
 bool OpenPurchaseMenu(int Index)
 {
  if(Panel==nullptr||View!="merchant")return false;auto Maps=Game.MapOffers();FOpusEntry E;int64 Price=0;FString Key;
  if(Maps.IsValidIndex(Index)){E=Maps[Index];Price=E.Tier*20;}else{int At=Index-Maps.Num();if(!Tower.Recent.State.MerchantStock.IsValidIndex(At)||Tower.Recent.State.MerchantStock[At].Claimed)return false;auto S=Tower.Recent.State.MerchantStock[At];E=UOpusGameplayStore::Entry(Tower.Rules,S.Payload);Price=S.Price;Key=S.Key;}
  auto Position=Pointer();ClosePurchaseMenu();CloseHover();Selected=Index;Refresh();PurchaseIndex=Index;PurchaseKey=Key;PurchasePosition=Position;
  FString Q=E.Rarity;if(Q!="magic"&&Q!="rare"&&Q!="legendary"&&Q!="unique")Q="common";PurchaseMenu=Create("merchant-purchase-"+Q,220);if(PurchaseMenu==nullptr){ClosePurchaseMenu();return false;}
  UOpusUI::SetImage(PurchaseMenu,"merchant-purchase-item-icon",E.Icon);UOpusUI::SetText(PurchaseMenu,"merchant-purchase-item-name",E.Name);UOpusUI::SetText(PurchaseMenu,"merchant-purchase-item-meta",Tower.Hud.ItemHover.QualityName(E.Rarity)+" · T"+E.Tier);UOpusUI::SetText(PurchaseMenu,"merchant-purchase-price","−"+Price);
  int W=Math::Max(70,49+(""+Price).Len()*7),X=219-W;UOpusUI::SetRect(PurchaseMenu,"merchant-purchase-price-box",FVector4(X,8,W,20));UOpusUI::SetRect(PurchaseMenu,"merchant-purchase-stone-icon",FVector4(X+7,14,9,9));UOpusUI::SetRect(PurchaseMenu,"merchant-purchase-price",FVector4(X+19,8,W-44,20));
  CanPurchase=UOpusNative::Stones(Tower.Rules)>=Price;UOpusUI::SetEnabled(PurchaseMenu,"merchant-purchase-buy",CanPurchase);PlacePurchase();return true;
 }
 bool ConfirmPurchase(){if(PurchaseMenu==nullptr||!CanPurchase)return false;int Index=PurchaseIndex;FString Expected=PurchaseKey;ClosePurchaseMenu();CloseHover();bool Ok=Game.Buy(Index,Expected);Status=Ok?"已购入 · 放入背包":Tower.LastError;Refresh();return Ok;}
 bool Purchase(int Index)
 {if(Panel==nullptr||View!="merchant")return false;Selected=Index;FString Expected;int At=Index-Game.MapOffers().Num();if(Tower.Recent.State.MerchantStock.IsValidIndex(At))Expected=Tower.Recent.State.MerchantStock[At].Key;CloseHover();bool Ok=Game.Buy(Index,Expected);Status=Ok?"已购入 · 放入背包":Tower.LastError;Refresh();return Ok;}
 UFUNCTION() bool FilterKey(FKey Key)
 {
  if(Panel==nullptr)return false;
  if(PurchaseMenu!=nullptr){if(Key==EKeys::Escape){ClosePurchaseMenu();Suppress=true;return true;}if(Key==EKeys::Enter||Key==EKeys::SpaceBar){ConfirmPurchase();Suppress=true;return true;}if(Key==EKeys::LeftMouseButton&&!InsidePurchase()){ClosePurchaseMenu();return false;}}
  if((Key==EKeys::LeftControl||Key==EKeys::RightControl)&&Hover!=nullptr){HoverPinned=true;PopulateHover();Suppress=true;return true;}
  if(Key==EKeys::Escape){if(HoverPinned)CloseHover();else Close();Suppress=true;return true;}
  if(Key==EKeys::RightMouseButton)
  {if(Hover!=nullptr&&Hover.DispatchingPointer){Suppress=true;return true;}auto N=UOpusMapUI::Hit(Panel,Gameplay::GetPlayerController(0));if(N!=nullptr){auto A=UOpusUI::Action(N);if(A.Op=="select")OpenPurchaseMenu(String::Conv_StringToInt(A.Value));}Suppress=true;return true;}return false;
 }
 bool HandleKeys(APlayerController PC)
 {
  if(Suppress){Suppress=false;return true;}if(Panel==nullptr)return false;
  if(PC.WasInputKeyJustPressed(EKeys::Escape))FilterKey(EKeys::Escape);else if(PC.WasInputKeyJustPressed(EKeys::LeftControl)||PC.WasInputKeyJustPressed(EKeys::RightControl))FilterKey(EKeys::LeftControl);else if(PurchaseMenu!=nullptr&&PC.WasInputKeyJustPressed(EKeys::Enter))FilterKey(EKeys::Enter);else if(PC.WasInputKeyJustPressed(EKeys::RightMouseButton))FilterKey(EKeys::RightMouseButton);return true;
 }
 UFUNCTION() void EnterHover(USvgCanvasNode Node)
 {
  if(View!="merchant"||HoverPinned||PurchaseMenu!=nullptr)return;auto N=Cast<UOpusActionNode>(Node);if(N==nullptr||N.HoverId.IsEmpty())return;
  CloseHover();HoverIndex=String::Conv_StringToInt(N.HoverId);HoverAnchor=N;PopulateHover();
 }
 void PopulateHover()
 {
  auto Maps=Game.MapOffers();FOpusEntry E;TArray<FOpusHoverLine> Lines;
  if(Maps.IsValidIndex(HoverIndex))E=Maps[HoverIndex];
  else {int At=HoverIndex-Maps.Num();if(!Tower.Recent.State.MerchantStock.IsValidIndex(At)||Tower.Recent.State.MerchantStock[At].Claimed){CloseHover();return;}auto S=Tower.Recent.State.MerchantStock[At];E=UOpusGameplayStore::Entry(Tower.Rules,S.Payload);Lines=UOpusGameplayStore::Lines(Tower.Rules,S.Payload);}
  if(Hover==nullptr){Hover=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),E.Kind=="equipment"?"hover-legendary":"hover-common","UICreatorHover",180);if(Hover==nullptr)return;Hover.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");Hover.OnSourceAction.AddUFunction(this,n"Action");}
  TArray<FOpusEntry> Empty;FOpusCraftState Craft;UOpusHoverUI::Populate(Hover,E,(E.Kind=="equipment"?"传奇 · "+Tower.Hud.SlotName(E.Slot):"地图物品")+" · T"+E.Tier,Lines,HoverPinned,Empty,Craft,"","",(E.Kind=="item"?E.Description+"\n":"")+"右键打开购买菜单",true);PlaceHover();
 }
 UFUNCTION() void LeaveHover(USvgCanvasNode Node){if(Node==HoverAnchor&&!HoverPinned)CloseHover();}
 void PlaceHover()
 {
  if(Hover==nullptr||HoverAnchor==nullptr)return;auto PC=Gameplay::GetPlayerController(0);auto A=UOpusHoverUI::AnchorRect(HoverAnchor,PC);auto V=UOpusHoverUI::ViewSize(PC);auto S=Hover.TemplateSize;float Scale=float32(Math::Min(1.0,(V.Y-24)/S.Y));double X=A.Z+10;if(X+S.X*Scale>V.X-12)X=A.X-S.X*Scale-10;UOpusHoverUI::Place(Hover,FVector2D(Math::Max(12.0,X),Math::Clamp(A.Y,12.0,Math::Max(12.0,V.Y-S.Y*Scale-12))),Scale);
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Tower==nullptr||Tower.Traveling)return;auto PC=Gameplay::GetPlayerController(0);auto V=UOpusHoverUI::ViewSize(PC);
  if(Rail!=nullptr)UOpusHoverUI::Place(Rail,FVector2D(V.X-45,V.Y-50),1);
  bool Visible=Panel==nullptr&&!Tower.Paused&&!Tower.Hud.MapDevice.Visible&&!Tower.Hud.Talents.Visible&&!UOpusUI::BlocksCombat(Tower.Hud.Screen);
  if(MerchantTag!=nullptr){MerchantTag.SetVisibility(Visible?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);if(IsValid(Game.Merchant))Game.Merchant.Bubble.SetVisibility(Visible);}
  if(Game.PortalReady){RefreshPortal();if(PortalTag!=nullptr){PortalTag.SetVisibility(Visible?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);if(Visible)UOpusWorldUI::Place(PortalTag,PC,Game.PortalPosition+FVector(0,0,170),FVector2D::ZeroVector);}}
  Poll-=Dt;if(Panel!=nullptr&&Poll<=0){Poll=.25;UOpusMapUI::Fit(Panel,PC);PlaceHover();PlacePurchase();}
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){Close();for(auto S:Brushes)if(S!=nullptr)S.RemoveFromParent();if(IsValid(Game.Merchant))Game.Merchant.Bubble.SetWidget(nullptr);MerchantTag=nullptr;if(PortalTag!=nullptr)PortalTag.RemoveFromParent();if(Rail!=nullptr)Rail.RemoveFromParent();}
}
