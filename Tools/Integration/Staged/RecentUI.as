// All panels and cells are frozen UICreator templates. Interaction and persistence live here.
class UDWOStorageDrag : UObject
{
 UPROPERTY() UDWORecentUI UI;
 FString Side;int Cell=0;
 UFUNCTION() void Press(){if(UI!=nullptr)UI.BeginDrag(Side,Cell);}
 UFUNCTION() void Release(){if(UI!=nullptr)UI.EndDrag();}
}
class UDWORecentUI : UActorComponent
{
 UPROPERTY() UDWORecentGameplay Game;
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UOpusWorldScreen Panel;
 UPROPERTY() UOpusWorldScreen Launcher;
 UPROPERTY() UOpusWorldScreen Tracker;
 UPROPERTY() UOpusWorldScreen Ghost;
 UPROPERTY() TArray<UOpusWorldScreen> Rows;
 UPROPERTY() TArray<UOpusWorldScreen> Slots;
 UPROPERTY() TArray<UDWOStorageDrag> Drags;
 UPROPERTY() TArray<UOpusWorldScreen> PlaceLabels;
 FString View,Status,StoreKind="stash",DragSide;
 TArray<FOpusEntry> Bag;
 int AuraSlot=0,BagPage=0,OtherPage=0,DragCell=-1;
 bool WasPaused=false,Started=false,Dragged=false;
 double Clock=0,RefreshAt=0,SuppressUntil=0,ClosedUntil=0;
 FVector2D DragStart;
 void Initialize(UDWORecentGameplay G){Game=G;Tower=G.Tower;}
 UOpusWorldScreen Create(FString Id,int Layer=150,bool Viewport=true)
 {
  auto W=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Id,"UICreatorRecent",Layer,Viewport);
  if(W==nullptr){Tower.Fail("Missing saved UICreatorRecent "+Id);return nullptr;}
  W.OnSourceAction.AddUFunction(this,n"Action");W.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");return W;
 }
 bool Open(FString Id)
 {
  if(Tower.Traveling||Tower.Preparing||Tower.Events.IsOpen())return false;
  if((Tower.Hud.Talents!=nullptr&&Tower.Hud.Talents.Visible)||(Tower.Hud.Screen!=nullptr&&UOpusUI::BlocksCombat(Tower.Hud.Screen))){Tower.LastError="请先关闭当前界面";return false;}
  if(Panel!=nullptr)Close();WasPaused=Tower.Paused;Tower.SetPaused(true);View=Id;Status="";
  Panel=Create(Id=="auras"?"aura-panel":Id=="quests"?"quest-panel":"hideout-panel");if(Panel==nullptr)return false;
  Refresh();return true;
 }
 void Close()
 {
  EndDrag(false);if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();Panel=nullptr;Tower.Hud.ModalPaused=false;Tower.SetPaused(WasPaused);ClosedUntil=Clock+.1;}
  Rows.Empty();Drags.Empty();View="";
 }
 UFUNCTION() bool FilterKey(FKey Key){if(Key==EKeys::Escape){Close();return true;}return false;}
 bool HandleKeys(APlayerController PC)
 {
  if(Clock<ClosedUntil)return true;
  if(Panel!=nullptr){if(PC.WasInputKeyJustPressed(EKeys::Escape))Close();return true;}
  if(PC.WasInputKeyJustPressed(EKeys::O)){Open("auras");return true;}
  if(PC.WasInputKeyJustPressed(EKeys::J)){Open("quests");return true;}
  if(PC.WasInputKeyJustPressed(EKeys::H)){TravelHideout();return true;}return false;
 }
 void TravelHideout()
 {
  if(Tower.CanFight()||Tower.Preparing||Tower.Events.HoldsTravel()){Tower.LastError="请在挑战结束后进入藏身处";return;}
  auto J=UDWOTowerJourney::Get();J.Hideout=!J.Hideout;J.StartOnArrival=false;Close();Tower.Travel(true);
 }
 UFUNCTION() void Action(USvgCanvasNode N)
 {
  auto A=UOpusUI::Action(N);FString Op=A.Op;
  if(Op=="close"){Close();return;}
  if(Op=="auras"||Op=="quests"){Open(Op);return;}
  if(Op=="hideout"){TravelHideout();return;}
  if(Op=="open-stash"||Op=="open-shop"){StoreKind=Op=="open-stash"?"stash":"shop";Open("hideout");return;}
  if(Op.StartsWith("slot-"))AuraSlot=String::Conv_StringToInt(Op.Mid(5));
  if(Op.StartsWith("toggle-")){Game.Toggle(String::Conv_StringToInt(Op.Mid(7)));Status=Tower.LastError;}
  if(Op.StartsWith("clear-"))Game.EquipAura(String::Conv_StringToInt(Op.Mid(6)),FName());
  if(Op=="choose-aura")Game.EquipAura(AuraSlot,FName(A.Value));
  if(Op=="toggle-slot"){int I=String::Conv_StringToInt(A.Value);auto PC=Gameplay::GetPlayerController(0);if(PC.IsInputKeyDown(EKeys::LeftShift)||PC.IsInputKeyDown(EKeys::RightShift)){AuraSlot=I;Open("auras");return;}Game.Toggle(I);}
  if(Op.StartsWith("claim-")){Game.Claim(String::Conv_StringToInt(Op.Mid(6)));Status=Tower.LastError;}
  if(Op=="stash"||Op=="shop"){StoreKind=Op;OtherPage=0;}
  if(Op=="bag-prev")BagPage=Math::Max(0,BagPage-1);if(Op=="bag-next")BagPage++;
  if(Op=="other-prev")OtherPage=Math::Max(0,OtherPage-1);if(Op=="other-next")OtherPage++;
  if(Op=="storage-cell"&&Clock>=SuppressUntil)
  {int At=A.Value.Find(":");if(At>0)Move(A.Value.Left(At),String::Conv_StringToInt(A.Value.Mid(At+1)),-1);}
  Refresh();
 }
 void Refresh()
 {
  if(Panel==nullptr)return;
  if(View=="auras")
  {
   TArray<FString> Keys;Keys.Add("Q");Keys.Add("E");Keys.Add("F");Keys.Add("G");
   for(int I=0;I<4;I++)
   {
    FString Name="空槽";for(auto C:Game.Choices)if(C.Id==Game.AuraSlots[I])Name=C.Name;
    UOpusUI::SetText(Panel,"aura:slot-"+I+":text",Keys[I]+" · "+Name+(AuraSlot==I?" ◇":""));
    UOpusUI::SetText(Panel,"aura:toggle-"+I+":text",Game.Enabled[I]?"关闭":"开启");
   }
   UOpusUI::SetText(Panel,"aura:reservation","魔力封印 "+int(Math::RoundToInt(UOpusRecentGameplay::ManaReservation(Tower.Rules)))+"% · 可用上限 "+int(Math::RoundToInt(UOpusRecentGameplay::UsableMana(Tower.Rules))));
   Rows.Empty();for(auto C:Game.Choices){auto R=Create("aura-choice-row",150,false);if(R==nullptr)return;UOpusUI::SetText(R,"row:name",C.Name+" · 封印 "+int(C.Reservation)+"%");UOpusUI::SetText(R,"row:description",C.Description);UOpusUI::SetAction(R,"aura-choice-row","choose-aura",C.Id.ToString());Rows.Add(R);}
   UOpusWorldUI::MountRows(Panel,"aura:rows",Rows);UOpusUI::SetText(Panel,"aura:status",Status.IsEmpty()?(Game.Choices.IsEmpty()?"当前非武器装备尚未授予光环。":"选中一个槽，再选择光环；关闭不会立即回复魔力。"):Status);
  }
  if(View=="quests")
  {
   UOpusUI::SetText(Panel,"quest:progress",Progress());
   for(int I=0;I<3;I++)
   {
    UOpusUI::BindNode(Panel,"quest-card-"+I,"quest-live-card-"+I);UOpusUI::SetShown(Panel,"quest-live-card-"+I,!Game.State.RewardClaimed);
    bool Found=Game.State.RewardCandidates.IsValidIndex(I);FOpusEntry E;if(Found)E=UOpusGameplayStore::Entry(Tower.Rules,Game.State.RewardCandidates[I]);
    UOpusUI::SetImage(Panel,"quest:icon-"+I,E.Icon);UOpusUI::SetText(Panel,"quest:name-"+I,Found?E.Name:"待解锁");
    UOpusUI::SetText(Panel,"quest:meta-"+I,Found?"T"+E.Tier+" · "+E.Rarity:"T1 武器");FString Traits;
    if(Found)for(auto L:UOpusGameplayStore::Lines(Tower.Rules,Game.State.RewardCandidates[I]))Traits+=L.Text+"\n";
    UOpusUI::SetText(Panel,"quest:traits-"+I,Traits);UOpusUI::SetText(Panel,"quest:claim-"+I+":text",Game.State.RewardClaimed?"奖励已领取":"选择这件武器");
    UOpusUI::SetShown(Panel,"quest:claim-"+I,Found&&!Game.State.RewardClaimed);
   }
   FString Claimed;if(Game.State.RewardClaimed&&Game.State.RewardCandidates.IsValidIndex(Game.State.ClaimedChoice))Claimed="初次试炼完成，已领取「"+UOpusGameplayStore::Entry(Tower.Rules,Game.State.RewardCandidates[Game.State.ClaimedChoice]).Name+"」。武器已放入背包。";
   UOpusUI::SetText(Panel,"quest:status",Status.IsEmpty()?(Claimed.IsEmpty()?"三选一；完整候选已经保存，关闭界面不会重新生成。":Claimed):Status);
  }
  if(View=="hideout")RefreshStorage();
 }
 FString Progress() const
 {
  if(Game.State.RewardClaimed)return "已完成 · 奖励已领取";if(Game.State.FirstFloorComplete)return "挑战完成 · 选择一件奖励";
  if(Tower.Run.Floor!=1)return "返回第一层完成三波战斗与首领";
  return Tower.Run.Phase==EDWOTowerPhase::Ready?"第一层 · 等待挑战":Tower.Run.Phase==EDWOTowerPhase::Boss?"第一层 · 击败首领":"第一层 · 第 "+Tower.Run.Wave+" / 3 波";
 }
 int FirstFree(const TArray<FOpusStoredStack>& Store) const
 {int Cell=0;while(true){bool Used=false;for(auto S:Store)if(S.Cell==Cell)Used=true;if(!Used)return Cell;Cell++;}return 0;}
 void Move(FString Side,int Cell,int ToCell)
 {
  if(!UDWOTowerJourney::Get().Hideout){Status="请先进入藏身处";return;}
  auto Saved=Game.State;auto Pools=Tower.Rules.GetPlayerPools();FString Player=Tower.Rules.ExportPlayerState(),Error;bool Ok=false;
  TArray<FOpusStoredStack> Store=StoreKind=="stash"?Game.State.Stash:Game.State.Goods;
  if(Side=="bag")
  {
   if(!Bag.IsValidIndex(Cell))return;auto E=Bag[Cell];FOpusStoredStack S;
   int Destination=ToCell<0?FirstFree(Store):ToCell;for(auto Existing:Store)if(Existing.Cell==Destination){Status="目标格子已有物品";return;}
   auto PC=Gameplay::GetPlayerController(0);int Count=PC.IsInputKeyDown(EKeys::LeftShift)||PC.IsInputKeyDown(EKeys::RightShift)?1:E.Count;
   Ok=UOpusGameplayStore::Store(Tower.Rules,E.Key,Count,S,Error);if(Ok){S.Cell=Destination;Store.Add(S);}
  }
  else
  {
   int At=-1;for(int I=0;I<Store.Num();I++)if(Store[I].Cell==Cell)At=I;if(At<0)return;
   Ok=UOpusGameplayStore::Restore(Tower.Rules,Store[At],Error);if(Ok)Store.RemoveAt(At);
  }
  if(Ok){if(StoreKind=="stash")Game.State.Stash=Store;else Game.State.Goods=Store;Ok=Game.Persist(Error);}
  if(!Ok){FString Ignored;Tower.Rules.ImportPlayerState(Player,Ignored);UOpusNative::CarryPools(Tower.Rules,Pools);Game.State=Saved;Status=Error;}
  else {Status=Side=="bag"?(StoreKind=="stash"?"物品已存入仓库":"商品已放上摊位"):"物品已取回背包";Tower.Hud.RefreshIn=0;}
  Refresh();
 }
 UButton Input(UWidget W){auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto R=Input(C);if(R!=nullptr)return R;}return nullptr;}
 void RefreshStorage()
 {
  Bag.Empty();for(auto E:UOpusData::Inventory(Tower.Rules))if(!E.Equipped&&!E.Locked&&(E.Kind=="equipment"||E.Kind=="item")&&E.Slot!="map"&&E.Slot!="character"&&E.Slot!="lineage")Bag.Add(E);
  TArray<FOpusStoredStack> Store=StoreKind=="stash"?Game.State.Stash:Game.State.Goods;
  int Last=0;for(auto S:Store)Last=Math::Max(Last,S.Cell);BagPage=Math::Clamp(BagPage,0,Math::Max(0,(Bag.Num()-1)/48));OtherPage=Math::Clamp(OtherPage,0,Last/48);
  Rows.Empty();Drags.Empty();
  for(int Side=0;Side<2;Side++)
  {
   TArray<UOpusWorldScreen> Grid;FString Name=Side==0?"bag":"other";int Page=Side==0?BagPage:OtherPage;
   for(int Row=0;Row<6;Row++)
   {
    auto W=Create("stored-grid-row",150,false);if(W==nullptr)return;Rows.Add(W);Grid.Add(W);
    for(int Col=0;Col<8;Col++)
    {
     int Index=Page*48+Row*8+Col;FOpusEntry E;if(Side==0){if(Bag.IsValidIndex(Index))E=Bag[Index];}else for(auto S:Store)if(S.Cell==Index)E=UOpusGameplayStore::Entry(Tower.Rules,S.Payload);
     UOpusUI::SetImage(W,"cell:icon-"+Col,E.Icon);UOpusUI::SetText(W,"cell:count-"+Col,E.Count>1?""+E.Count:"");UOpusUI::SetAction(W,"stored:cell-"+Col,"storage-cell",Name+":"+Index);
     auto N=W.FindSourceNode("stored-cell-"+Col);if(N==nullptr)N=W.FindSourceNode("stored:cell-"+Col);auto Button=Input(N);
     if(Button!=nullptr){auto D=Cast<UDWOStorageDrag>(NewObject(this,UDWOStorageDrag));D.UI=this;D.Side=Name;D.Cell=Index;Button.OnPressed.AddUFunction(D,n"Press");Button.OnReleased.AddUFunction(D,n"Release");Drags.Add(D);}
    }
   }
   UOpusWorldUI::MountRows(Panel,"hideout:"+Name,Grid);UOpusUI::SetText(Panel,"hideout:"+Name+"-page",""+(Page+1)+" / "+(Side==0?Math::Max(1,(Bag.Num()+47)/48):Last/48+1));
  }
  UOpusUI::SetText(Panel,"hideout:other-title",StoreKind=="stash"?"仓库":"我的商品");UOpusUI::SetText(Panel,"hideout:status",Status.IsEmpty()?"拖入存放 / 上架，拖回取出 / 下架；Shift 点击存放 1 个。H 返回地牢。":Status);
 }
 void BeginDrag(FString Side,int Cell)
 {
  EndDrag(false);DragSide=Side;DragCell=Cell;float32 X=0,Y=0;Gameplay::GetPlayerController(0).GetMousePosition(X,Y);DragStart=FVector2D(X,Y);Dragged=false;
 }
 void EndDrag(bool Commit=true)
 {
  if(DragCell<0)return;
  if(Commit&&Dragged&&Panel!=nullptr)
  {
   auto PC=Gameplay::GetPlayerController(0);float32 X=0,Y=0;PC.GetMousePosition(X,Y);auto R=UOpusHoverUI::AnchorRect(Panel,PC);double Scale=(R.Z-R.X)/1060;
   FVector2D P((X-R.X)/Scale,(Y-R.Y)/Scale);int Side=P.X>=548&&P.X<1036?1:P.X>=24&&P.X<512?0:-1;
   if(P.Y>=160&&P.Y<544&&Side>=0&&((Side==0&&DragSide=="other")||(Side==1&&DragSide=="bag")))
   {int Col=Math::Clamp(int((P.X-(Side==0?24:548))/60),0,7),Row=Math::Clamp(int((P.Y-160)/64),0,5);int Destination=(Side==0?BagPage:OtherPage)*48+Row*8+Col;int Source=DragCell;FString From=DragSide;DragCell=-1;SuppressUntil=Clock+.15;Move(From,Source,Destination);}
   else SuppressUntil=Clock+.15;
  }
  DragCell=-1;Dragged=false;if(Ghost!=nullptr)Ghost.RemoveFromParent();Ghost=nullptr;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Game==nullptr||!Game.Ready||Tower.Hud.Screen==nullptr||(UOpusNative::Flag("DWOTransient")&&!UOpusNative::Flag("DWORecentTest")&&!UOpusNative::Flag("DWORecentPreview")))return;Clock+=Dt;auto PC=Gameplay::GetPlayerController(0);auto Size=UOpusHoverUI::ViewSize(PC);
  if(!Started)
  {
   Started=true;Launcher=Create("recent-launcher",40);Tracker=Create("quest-tracker",30);
   for(int I=0;I<4;I++){auto W=Create("aura-hud-slot",42);UOpusUI::SetAction(W,"aura-hud-slot","toggle-slot",""+I);Slots.Add(W);}
   if(UDWOTowerJourney::Get().Hideout)
   {
    for(int I=0;I<2;I++){auto W=Create("recent-item-row",40);UOpusUI::SetText(W,"row:item-name",I==0?"仓库":"商家商品");UOpusUI::SetText(W,"row:item-meta","点击直接打开");UOpusUI::SetAction(W,"recent-item-row",I==0?"open-stash":"open-shop","");PlaceLabels.Add(W);}
   }
  }
  UOpusHoverUI::Place(Launcher,FVector2D(14,Size.Y-44),.75f);UOpusHoverUI::Place(Tracker,FVector2D(14,365),.75f);UOpusUI::SetText(Tracker,"quest:tracker-text",Progress());Tracker.SetVisibility(Game.State.RewardClaimed?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible);
  for(int I=0;I<Slots.Num();I++)
  {
   auto W=Slots[I];bool Shown=!Game.AuraSlots[I].IsNone()&&!Tower.Paused;W.SetVisibility(Shown?ESlateVisibility::Visible:ESlateVisibility::Collapsed);if(!Shown)continue;
   FString Key=I==0?"Q":I==1?"E":I==2?"F":"G";
   auto Anchor=Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-hotbar-instance::rs-consumables::hud-quick-instance-"+Key);
   auto R=UOpusHoverUI::AnchorRect(Anchor,PC);double Scale=R.Z>R.X?(R.Z-R.X)/64:.8;
   UOpusHoverUI::Place(W,R.Z>R.X?FVector2D(R.X,R.Y):FVector2D(Size.X*.6+I*70,Size.Y-95),float32(Scale));
   UOpusUI::SetText(W,"aura-slot:key",Key);UOpusUI::SetText(W,"aura-slot:state",Game.Enabled[I]?"开启":"关闭");UOpusUI::SetImage(W,"aura-slot:icon",UOpusGameplayStore::SkillIcon(Game.AuraSlots[I]));
  }
  for(int I=0;I<PlaceLabels.Num();I++)UOpusWorldUI::Place(PlaceLabels[I],PC,I==0?FVector(-420,110,205):FVector(400,140,300),FVector2D(-120,-18));
  if(Panel!=nullptr){double Width=View=="auras"?940:View=="quests"?1020:1060,Height=View=="hideout"?700:630;double Scale=Math::Min(1.0,Math::Min((Size.X-24)/Width,(Size.Y-24)/Height));UOpusHoverUI::Place(Panel,FVector2D((Size.X-Width*Scale)/2,(Size.Y-Height*Scale)/2),float32(Scale));}
  if(DragCell>=0){float32 X=0,Y=0;PC.GetMousePosition(X,Y);if((FVector2D(X,Y)-DragStart).Size()>6)Dragged=true;if(Dragged){if(Ghost==nullptr){Ghost=Create("recent-item-row",300);Ghost.SetVisibility(ESlateVisibility::HitTestInvisible);UOpusUI::SetText(Ghost,"row:item-name","移动物品");UOpusUI::SetText(Ghost,"row:item-meta","松开以放入另一侧");}UOpusHoverUI::Place(Ghost,FVector2D(X+12,Y+12),.5f);}if(!PC.IsInputKeyDown(EKeys::LeftMouseButton))EndDrag();}
  if(Clock>=RefreshAt){RefreshAt=Clock+.5;if(Panel!=nullptr&&View!="hideout")Refresh();}
 }
}
