class UDWOEventItemDrag : UObject
{
 UPROPERTY() UDWOEventCraftUI UI;
 UPROPERTY() UButton Button;
 FString Uid;int From=-1;
 UFUNCTION() void Press(){UI.BeginDrag(Uid,From,Button);}
 UFUNCTION() void Release(){UI.EndDrag();}
}

// Presentation and pointer handling for the shared UICreator crafting composition.
class UDWOEventCraftUI : UActorComponent
{
 UPROPERTY() UDWOMapEvents Owner;
 UPROPERTY() UOpusWorldScreen Ghost;
 UPROPERTY() UOpusWorldScreen Scrim;
 UPROPERTY() UButton ActiveButton;
 UPROPERTY() TArray<UDWOEventItemDrag> Drags;
 UPROPERTY() TArray<FOpusEntry> Bag;
 UPROPERTY() UOpusHoverEvents Hover;
 FString DragUid;int From=-1,Page=0,Drop=-1;
 FVector2D Start;bool Dragged=false;
 double Clock=0,SuppressUntil=0;
 void Initialize(UDWOMapEvents E){Owner=E;}
 bool IsCraftOpen() const{return Owner!=nullptr&&Owner.Panel!=nullptr&&Owner.States.IsValidIndex(Owner.Active)&&(Owner.States[Owner.Active].Definition.Kind=="altar"||Owner.States[Owner.Active].Definition.Kind=="fusion");}
 UDWOMapEventState State(){return Owner.States[Owner.Active];}
 UButton Input(UWidget W){auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto R=Input(C);if(R!=nullptr)return R;}return nullptr;}
 void ClearBindings(){for(auto D:Drags)if(D.Button!=nullptr){D.Button.OnPressed.UnbindObject(D);D.Button.OnReleased.UnbindObject(D);}Drags.Empty();}
 void Opened(){Page=0;Scrim=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"event-craft-scrim","UICreatorEvents",150);if(Scrim!=nullptr)Scrim.SetVisibility(ESlateVisibility::HitTestInvisible);Hover=UOpusUI::HoverEvents(Owner.Panel);Hover.Entered.AddUFunction(Owner.Tower.Hud.ItemHover,n"Enter");Hover.Left.AddUFunction(Owner.Tower.Hud.ItemHover,n"Leave");}
 void Closed(){Cancel();ClearBindings();if(Scrim!=nullptr)Scrim.RemoveFromParent();Scrim=nullptr;if(Hover!=nullptr){Hover.Entered.UnbindObject(Owner.Tower.Hud.ItemHover);Hover.Left.UnbindObject(Owner.Tower.Hud.ItemHover);}Hover=nullptr;}
 void Text(FString B,FString V){UOpusUI::SetText(Owner.Panel,B,V);}
 void Bind(USvgCanvasNode N,FString Uid,int Slot=-1){if(N==nullptr||Uid.IsEmpty())return;auto B=Input(N);if(B==nullptr)return;auto D=Cast<UDWOEventItemDrag>(NewObject(this,UDWOEventItemDrag));D.UI=this;D.Button=B;D.Uid=Uid;D.From=Slot;B.OnPressed.AddUFunction(D,n"Press");B.OnReleased.AddUFunction(D,n"Release");Drags.Add(D);}
 void BindBag(UWidget W){auto N=Cast<USvgCanvasNode>(W);if(N!=nullptr&&N.IsVisible()){auto A=UOpusUI::Action(N);if(A.Op=="select")Bind(N,A.Value);}auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren())BindBag(C);}
 void Refresh()
 {
  if(!IsCraftOpen())return;auto S=State();bool Fusion=S.Definition.Kind=="fusion";auto P=Owner.Panel;ClearBindings();
  Text("event-title",S.Definition.Name);Text("event-description",Fusion?"投入两件同部位装备；保留左件底材与 T 阶，随机合并左右词条。":"锁定一件装备后可免费反复制作；指定通货无限使用。");
  UOpusUI::SetShown(P,"event-samples",false);
  Bag=UOpusMapEvents::Equipment(Owner.Tower.Rules);TArray<FOpusEntry> Filtered;
  for(auto E:Bag)if(E.Slot!="map"&&E.Slot!="character"&&E.Slot!="default"&&E.Slot!="tower"&&E.Slot!="rarity"&&E.Slot!="lineage")Filtered.Add(E);Bag=Filtered;
  int Pages=Math::Max(1,(Bag.Num()+23)/24);Page=Math::Clamp(Page,0,Pages-1);TArray<FOpusUIRow> Rows;
  for(int I=Page*24;I<Math::Min(Bag.Num(),Page*24+24);I++){auto E=Bag[I];FOpusUIRow R;R.Value=E.Id;R.Op="select";R.Label=E.Name;R.Icon=E.Icon;R.Rarity=E.Rarity;R.Tier=E.Tier;R.Equipped=E.Equipped||E.Id==S.Left||E.Id==S.Right;Rows.Add(R);}
  UOpusUI::SetRows(P,"event-bag",Rows,8);BindBag(P.GetRootWidget());
  Text("bag-title","装备背包 · 当前放入"+FString(Owner.DepositSide==0?"左格":"右格"));Text("bag-meta",""+Bag.Num()+" 件 · 未穿戴装备可投入");Text("bag-page",""+(Page+1)+" / "+Pages);
  for(int I=0;I<2;I++)
  {
   FString Id=I==0?S.Left:S.Right;if(Fusion&&S.Spent&&I==0&&S.Offer!=nullptr&&S.Offer.Entries().Num()>0)Id=S.Offer.Entries()[0].Id;auto E=Owner.Equipment(Id);
   UOpusUI::SetImage(P,"craft:input-icon-"+I,E.Icon);Text("input-"+I+"-label",Id.IsEmpty()?"＋":"");
   UOpusUI::SetShown(P,"craft:input-icon-"+I,!Id.IsEmpty());
   Text("side-title-"+I,Fusion?(I==0?"左件 · 保留底材":"右件 · 贡献词条"):(I==0?"绑定装备":"指定通货 · 无限使用"));
   Text("item-name-"+I,Id.IsEmpty()?"点击装备格放入":"T"+E.Tier+" · "+E.Name);Text("item-meta-"+I,Id.IsEmpty()?"点击或拖入装备":S.Locked?"已锁定 · 悬停查看":"悬停查看装备词条");
   UOpusWorldUI::SetHover(P,"input-"+I,Id);Bind(P.FindSourceNode("craft-input-"+I),Id,I);
  }
  FString CurrencyName=S.Currency.ToString();if(!Fusion){UOpusUI::SetImage(P,"craft:input-icon-1",FPaths::ProjectContentDir()+"OpusIcons/item/"+S.Currency+".png");for(auto E:UOpusData::Listings(Owner.Tower.Rules))if(E.Id==S.Currency.ToString()){CurrencyName=E.Name;break;}UOpusUI::SetShown(P,"craft:input-icon-1",true);Text("input-1-label","");Text("item-name-1",CurrencyName);Text("item-meta-1","指定通货 · 不消耗背包材料");}
  UOpusUI::SetAction(P,"input-0","craft-slot","0");UOpusUI::SetAction(P,"input-1",Fusion?"craft-slot":"noop","1");
  Text("event-primary-label",Fusion?(S.Locked?"确认合成 · 消耗两件":"锁定两件装备"):(S.Locked?"免费制作 · "+CurrencyName:"锁定这件装备"));Text("event-secondary-label",S.Spent?"关闭":S.Locked?"领取并结束祭坛":"清空投入");
  UOpusUI::SetShown(P,"event-secondary",Fusion||S.Locked);UOpusUI::SetShown(P,"event-primary",!S.Spent);
  UOpusUI::SetShown(P,"event-affix-target",false);
  if(!Fusion&&S.Locked){auto C=UOpusData::CraftState(Owner.Tower.Rules,S.Left,S.Currency,n"",true);if(C.Operation=="reroll"){if(!C.EligibleAffixes.Contains(Owner.SelectedAffix)&&C.EligibleAffixes.Num()>0)Owner.SelectedAffix=C.EligibleAffixes[0];UOpusUI::SetShown(P,"event-affix-target",true);Text("event-affix-target-label","洗炼第 "+(Owner.SelectedAffix+1)+" 条 · 点击切换");}}
  Text("event-status",Owner.Status.IsEmpty()?(Fusion?"从左侧背包投入两件同部位装备":"法坛指定："+CurrencyName+" · 已制作 "+S.Crafts+" 次 · 免费使用"):Owner.Status);
  Text("event-warning",Fusion?"左右各随机抽取词条，固定传奇也参与；锁定后确认消耗两件。":"锁定后本地图不能换装；离开地图保留当前结果。");
 }
 bool Handle(FOpusUIAction A)
 {
  if(!IsCraftOpen())return false;if(Clock<SuppressUntil&&(A.Op=="select"||A.Op=="craft-slot"))return true;auto S=State();
  if(A.Op=="select"){Owner.Deposit(S,A.Value);Refresh();return true;}
  if(A.Op=="craft-slot"){Owner.DepositSide=String::Conv_StringToInt(A.Value);Refresh();return true;}
  if(A.Op=="craft-page"){Page+=String::Conv_StringToInt(A.Value);Refresh();return true;}
  if(A.Op=="craft-affix"){auto C=UOpusData::CraftState(Owner.Tower.Rules,S.Left,S.Currency,n"",true);int I=C.EligibleAffixes.FindIndex(Owner.SelectedAffix);if(C.EligibleAffixes.Num()>0)Owner.SelectedAffix=C.EligibleAffixes[(I+1)%C.EligibleAffixes.Num()];Refresh();return true;}
  if(A.Op=="collect"&&!S.Locked&&!S.Spent&&S.Definition.Kind=="fusion"){S.Left="";S.Right="";Owner.Status="已清空投入，装备保留在背包";Refresh();return true;}
  return false;
 }
 bool Inside(FVector2D P,FString Id){auto N=Owner.Panel.FindSourceNode(Id);if(N==nullptr)return false;auto R=UOpusHoverUI::AnchorRect(N,Gameplay::GetPlayerController(0));return R.Z>R.X&&P.X>=R.X&&P.X<=R.Z&&P.Y>=R.Y&&P.Y<=R.W;}
 int Target(FVector2D P){if(Inside(P,"craft-input-0"))return 0;if(State().Definition.Kind=="fusion"&&Inside(P,"craft-input-1"))return 1;return -1;}
 FVector2D Pointer(){auto PC=Gameplay::GetPlayerController(0);float32 X=0,Y=0;int W=0,H=0;PC.GetMousePosition(X,Y);PC.GetViewportSize(W,H);auto Size=UOpusHoverUI::ViewSize(PC);return FVector2D(W>0?X*Size.X/W:X,H>0?Y*Size.Y/H:Y);}
 void BeginDrag(FString Uid,int Slot=-1,UButton Button=nullptr){Cancel();if(!IsCraftOpen())return;auto S=State();auto E=Owner.Equipment(Uid);if(S.Locked||S.Spent||E.Id.IsEmpty()||E.Equipped||!E.Identified||E.Locked)return;DragUid=Uid;From=Slot;ActiveButton=Button;Start=Pointer();}
 void Cancel(){if(IsCraftOpen())for(int I=0;I<2;I++)UOpusWorldUI::Emphasize(Owner.Panel,"input-"+I,false);DragUid="";From=-1;ActiveButton=nullptr;Dragged=false;Drop=-1;if(Ghost!=nullptr)Ghost.RemoveFromParent();Ghost=nullptr;}
 void Finish(FVector2D P)
 {
  if(DragUid.IsEmpty()||!IsCraftOpen())return;auto S=State();int Index=Target(P);FString Uid=DragUid;int Source=From;
  if(Dragged){SuppressUntil=Clock+.2;if(!S.Locked&&!S.Spent){if(Index>=0){if(Source>=0&&Index!=Source&&S.Definition.Kind=="fusion"){FString Swap=S.Left;S.Left=S.Right;S.Right=Swap;}else if(Index!=Source){Owner.DepositSide=Index;Owner.Deposit(S,Uid);}}else if(Source>=0&&Inside(P,"craft-event-backpack")){if(Source==0)S.Left="";else S.Right="";Owner.Status="已取回装备";}}}
  bool RefreshNeeded=Dragged;Cancel();if(RefreshNeeded)Refresh();
 }
 void EndDrag(){Finish(Pointer());}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;if(!IsCraftOpen())return;auto PC=Gameplay::GetPlayerController(0);if(Scrim!=nullptr){auto Size=UOpusHoverUI::ViewSize(PC);double Scale=Math::Max(Size.X/16,Size.Y/9);UOpusHoverUI::Place(Scrim,(Size-FVector2D(16,9)*Scale)/2,float32(Scale));}if(DragUid.IsEmpty())return;FVector2D Pos=Pointer();if((Pos-Start).Size()>6)Dragged=true;
  if(Dragged){Owner.Tower.Hud.ItemHover.Close();if(Ghost==nullptr){Ghost=UOpusWorldUI::CreateTemplate(PC,"event-craft-drag","UICreatorEvents",300);Ghost.SetVisibility(ESlateVisibility::HitTestInvisible);UOpusUI::SetImage(Ghost,"craft:icon",Owner.Equipment(DragUid).Icon);}UOpusHoverUI::Place(Ghost,Pos+FVector2D(12,12),1);int At=Target(Pos);if(At!=Drop){Drop=At;for(int I=0;I<2;I++)UOpusWorldUI::Emphasize(Owner.Panel,"input-"+I,I==At);}}
  // UI-consumed mouse presses need not reach PlayerInput. Use the actual Slate button state.
  if(ActiveButton!=nullptr&&!ActiveButton.IsPressed())EndDrag();
 }
}
