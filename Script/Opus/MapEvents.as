// AS owns encounter state and random draws. UICreator owns every UI control.
class UDWOMapEventState : UObject
{
 UPROPERTY() FOpusMapEventDefinition Definition;
 UPROPERTY() FVector Position;
 UPROPERTY() ADWOMapEventMarker Marker;
 UPROPERTY() UOpusWorldScreen Label;
 UPROPERTY() UOpusEventOffer Offer;
 FString Token,Left,Right,Caption;
 FName Currency;
 FRandomStream Random;
 bool Locked=false,Spent=false,Engaged=false,GuardsStarted=false,GuardsCleared=false;
 int Crafts=0,DrawLeft=0,DrawRight=0;
}
class ADWOMapEventMarker : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Base;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Core;
 default Base.SetCollisionProfileName(n"NoCollision");
 default Core.SetCollisionProfileName(n"NoCollision");
 void Initialize(FString Kind)
 {
  Base.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cylinder.Cylinder")));Base.SetRelativeScale3D(FVector(1.5,1.5,0.25));
  Core.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,Kind=="vault"?"/Engine/BasicShapes/Cube.Cube":"/Engine/BasicShapes/Sphere.Sphere")));Core.SetRelativeLocation(FVector(0,0,65));Core.SetRelativeScale3D(FVector(0.65));
  UOpusNative::Tint(Base,FLinearColor(0.2,0.25,0.32));UOpusNative::Tint(Core,Kind=="fusion"?FLinearColor(0.6,0.3,0.9):Kind=="altar"?FLinearColor(0.2,0.8,0.7):FLinearColor(0.95,0.65,0.15));
 }
 void Finish(){UOpusNative::Tint(Core,FLinearColor(0.12,0.15,0.18));}
}
class UDWOMapEvents : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TArray<UDWOMapEventState> States;
 UPROPERTY() UOpusWorldScreen Panel;
 UPROPERTY() UDWOEventCraftUI CraftUI;
 UPROPERTY() TArray<UOpusWorldScreen> Rows;
 UPROPERTY() TArray<UOpusWorldScreen> ItemRows;
 UPROPERTY() TArray<UOpusWorldScreen> LineRows;
 UPROPERTY() TArray<FName> CandidateSnapshot;
 int ItemUsed=0,LineUsed=0,TierSnapshot=1;
 int Active=-1,SelectedAffix=-1,SelectedReward=-1,DepositSide=0;
 bool Planned=false,WasPaused=false;
 double Clock=0;
 FString Status;
 void Initialize(ADWOTowerGameMode Value){Tower=Value;CraftUI=UDWOEventCraftUI::Create(Tower);CraftUI.Initialize(this);}
 bool IsOpen() const{return Panel!=nullptr;}
 bool CombatActive() const{for(auto S:States)if(S.GuardsStarted&&!S.GuardsCleared&&!S.Spent)return true;return false;}
 bool HoldsTravel() const{for(auto S:States)if(!S.Spent&&(S.Engaged||S.Locked||S.GuardsStarted))return true;return IsOpen();}
 bool Enabled() const{return !UOpusNative::Flag("DWOTransient")||UOpusNative::Flag("DWOMapEventTest")||UOpusNative::Flag("DWOEventsPreview");}
 void Plan()
 {
  Planned=true;if(!Enabled())return;FOpusMapEventConfig Config;FString Error;
  if(!UOpusMapEvents::ReadConfig(Tower.DropService.EventConfigJson,Config,Error)){Tower.Fail(Error);return;}
  CandidateSnapshot=Tower.DropService.EquipmentIds;TierSnapshot=Math::Clamp(Tower.Map.Tier,1,10);
  auto Journey=UDWOTowerJourney::Get();FRandomStream R;R.Initialize(Journey.Seed+Tower.Run.Floor*1297+Journey.Visit*7919);
  bool Force=UOpusNative::Flag("DWOMapEventTest")||UOpusNative::Flag("DWOEventsPreview");if(!Force&&R.RandRange(0,99)>=Config.ChancePercent)return;
  TArray<FOpusMapEventDefinition> Eligible;for(auto D:Config.Definitions)if(D.MinFloor<=Tower.Run.Floor)Eligible.Add(D);
  int Count=Force?Eligible.Num():Math::Min(Eligible.Num(),R.RandRange(Config.MinEvents,Config.MaxEvents));
  for(int N=0;N<Count;N++)
  {
   int Total=0;for(auto D:Eligible)Total+=D.Weight;int At=R.RandRange(0,Math::Max(0,Total-1)),Index=0;for(int I=0;I<Eligible.Num();I++){At-=Eligible[I].Weight;if(At<0){Index=I;break;}}
   auto S=Cast<UDWOMapEventState>(NewObject(this,UDWOMapEventState));S.Definition=Eligible[Index];Eligible.RemoveAt(Index);S.Token="map-event-"+Journey.Visit+"-"+S.Definition.Id;S.Random.Initialize(R.RandRange(1,2000000000));
   TArray<FVector> Points;for(auto P:Tower.Dungeon.Points){bool Good=(P-Tower.Hero.ActorLocation).Size2D()>300;for(auto Existing:States)if((P-Existing.Position).Size2D()<650)Good=false;if(Good)Points.Add(P);}
   if(Points.Num()==0){UOpusNative::WriteLog("EVENT_NO_SPACE "+S.Definition.Id);continue;}S.Position=Points[R.RandRange(0,Points.Num()-1)];
   if(S.Definition.Currencies.Num()>0)S.Currency=S.Definition.Currencies[S.Random.RandRange(0,S.Definition.Currencies.Num()-1)];
   S.Marker=Cast<ADWOMapEventMarker>(SpawnActor(ADWOMapEventMarker,S.Position+FVector(0,0,14)));S.Marker.Initialize(S.Definition.Kind);
   S.Label=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"event-label","UICreatorEvents",35);if(S.Label==nullptr){Tower.Fail("Missing UICreator event-label");return;}
   UOpusUI::SetAction(S.Label,"event-label","open",""+States.Num());S.Label.OnSourceAction.AddUFunction(this,n"OnAction");States.Add(S);
   UOpusNative::WriteLog("EVENT_SPAWN id="+S.Definition.Id+" at="+S.Position+" currency="+S.Currency);
  }
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Tower==nullptr||Tower.Rules==nullptr||!Tower.DropService.Ready||Tower.Dungeon==nullptr||!Tower.Dungeon.Ready)return;
  if(!Planned)Plan();Clock+=Dt;auto PC=Gameplay::GetPlayerController(0);
  for(auto S:States)
  {
   if(S.GuardsStarted&&!S.GuardsCleared){int Alive=0;for(auto E:Tower.Enemies)if(IsValid(E)&&!E.Dead&&E.EventId==S.Token)Alive++;if(!Tower.Preparing&&Alive==0){S.GuardsCleared=true;Status="守库者已击败，靠近宝库选择奖励";Tower.SaveProfile();}}
   if(S.Label==nullptr)continue;bool Near=(Tower.Hero.ActorLocation-S.Position).Size2D()<=260;
   FString Caption=S.Definition.Name+(S.Spent?" · 已完成":S.GuardsStarted&&!S.GuardsCleared?" · 守卫交战中":Near?" · [V] 交互":" · 靠近交互");if(Caption!=S.Caption){S.Caption=Caption;UOpusUI::SetText(S.Label,"event:label",Caption);}
   UOpusWorldUI::Place(S.Label,PC,S.Position+FVector(0,0,130),FVector2D(-125,-28));
  }
  if(Panel!=nullptr){auto Size=UOpusHoverUI::ViewSize(PC);double Width=Panel.TemplateSize.X,Height=Panel.TemplateSize.Y;double Scale=Math::Min(1.0,Math::Min((Size.X-32)/Width,(Size.Y-32)/Height));UOpusHoverUI::Place(Panel,FVector2D((Size.X-Width*Scale)/2,(Size.Y-Height*Scale)/2),float32(Scale));}
 }
 bool HandleKeys(APlayerController PC)
 {
  if(IsOpen()){if(PC.WasInputKeyJustPressed(EKeys::Escape))Close();return true;}
  if(!PC.WasInputKeyJustPressed(EKeys::V))return false;int Best=-1;double Distance=260;
  for(int I=0;I<States.Num();I++){double D=(States[I].Position-Tower.Hero.ActorLocation).Size2D();if(D<Distance&&!States[I].Spent){Best=I;Distance=D;}}
  if(Best>=0)Open(Best);return Best>=0;
 }
 UFUNCTION() bool FilterKey(FKey Key){if(Key==EKeys::Escape){Close();return true;}return false;}
 bool Open(int Index)
 {
  if(!States.IsValidIndex(Index)||States[Index].Spent||Tower.Preparing||Tower.Run.Phase==EDWOTowerPhase::Failed)return false;
  auto S=States[Index];if((S.Position-Tower.Hero.ActorLocation).Size2D()>260){Tower.LastError="请靠近奇遇后按 V";return false;}
  if(S.GuardsStarted&&!S.GuardsCleared){Tower.LastError="请先击败守库者";return false;}
  if(Tower.Hud.Screen!=nullptr&&UOpusUI::BlocksCombat(Tower.Hud.Screen))
  {
   if(Tower.Hud.Screen.CurrentState!="completion"){Tower.LastError="请先关闭其他界面";return false;}
   Tower.Hud.Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Tower.Rules)+1));Tower.Hud.ModalPaused=false;Tower.SetPaused(false);
  }
  if(Tower.Run.Phase==EDWOTowerPhase::Cleared)Tower.Hud.LastPhase=EDWOTowerPhase::Cleared;
  if(IsOpen())Close();Active=Index;SelectedReward=-1;SelectedAffix=-1;DepositSide=0;Status="";S.Engaged=true;WasPaused=Tower.Paused;
  bool Craft=S.Definition.Kind=="altar"||S.Definition.Kind=="fusion";
  Panel=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Craft?"event-craft-panel":"event-panel","UICreatorEvents",160);if(Panel==nullptr){Tower.Fail("Missing UICreator event-panel");return false;}
  if(Craft)CraftUI.Opened();
  Panel.OnSourceAction.AddUFunction(this,n"OnAction");Panel.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");Tower.SetPaused(true);Refresh();return true;
 }
 void Close(){if(CraftUI!=nullptr)CraftUI.Closed();if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();Panel=nullptr;Rows.Empty();ItemRows.Empty();LineRows.Empty();ItemUsed=0;LineUsed=0;Tower.SetPaused(WasPaused);}Active=-1;}
 void ReleaseLocks()
 {
  FString Error;for(auto S:States){if(S.Locked){if(!S.Left.IsEmpty())Tower.Rules.UnlockPlayerEventItem(S.Left,S.Token,Error);if(!S.Right.IsEmpty())Tower.Rules.UnlockPlayerEventItem(S.Right,S.Token,Error);S.Locked=false;}}Close();
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){if(Tower!=nullptr&&Tower.Rules!=nullptr)ReleaseLocks();}
 UOpusWorldScreen Row(FString Template)
 {
  bool Item=Template=="event-item-row";int At=Item?ItemUsed:LineUsed;if(Item)ItemUsed++;else LineUsed++;
  UOpusWorldScreen R=nullptr;if(Item&&ItemRows.IsValidIndex(At))R=ItemRows[At];if(!Item&&LineRows.IsValidIndex(At))R=LineRows[At];
  if(R==nullptr){R=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Template,"UICreatorEvents",160,false);if(R!=nullptr){R.OnSourceAction.AddUFunction(this,n"OnAction");if(Item)ItemRows.Add(R);else LineRows.Add(R);}}
  if(R!=nullptr)Rows.Add(R);return R;
 }
 void Text(FString Binding,FString Value){UOpusUI::SetText(Panel,Binding,Value);}
 FString Quality(FString V) const{return V=="legendary"?"传奇":V=="rare"?"稀有":V=="magic"?"魔法":"普通";}
 FString SlotName(FString V) const{return V=="weapon"?"武器":V=="offhand"?"副手":V=="helmet"?"头盔":V=="chest"?"胸甲":V=="gloves"?"手套":V=="boots"?"鞋子":V=="belt"?"腰带":V=="amulet"?"项链":V=="ring"?"戒指":"饰品";}
 FString Meta(FOpusEntry E) const{return SlotName(E.Slot)+" · T"+E.Tier+" · "+Quality(E.Rarity)+(E.Synthesized?" · 合成":"")+(E.FixedAffixCount>1?" · 传奇词条×"+E.FixedAffixCount:"");}
 FOpusEntry Equipment(FString Id){for(auto E:UOpusMapEvents::Equipment(Tower.Rules))if(E.Id==Id)return E;return FOpusEntry();}
 void Lines(int Col,const TArray<FOpusHoverLine>& Values,bool Selectable=false)
 {
  TArray<UOpusWorldScreen> List;
  TArray<FOpusHoverLine> Ordered;TArray<int> Order;if(Selectable){Order.Add(2);Order.Add(4);}else{Order.Add(4);Order.Add(2);}Order.Add(3);Order.Add(0);Order.Add(1);
  for(int Style:Order)for(auto L:Values)if(L.Style==Style)Ordered.Add(L);
  for(auto L:Values)if(!Order.Contains(L.Style))Ordered.Add(L);
  for(auto L:Ordered)
  {
   FString Title=(L.Style==4?"◆ 传奇 · ":L.Style==3?"基础 · ":"")+L.Text+(L.Tier>0?" · T"+L.Tier:"");FString Description=L.Description;
   if(Title.Len()>38){Description=L.Text+(Description.IsEmpty()?"":"\n"+Description);Title=L.Style==1?"装备说明":"词条说明";}
   int Parts=Math::Max(1,int(Math::CeilToInt(Description.Len()/88.0)));
   for(int Part=0;Part<Parts;Part++)
   {
    auto R=Row("event-line-row");if(R==nullptr)continue;UOpusUI::SetText(R,"row:text",Title+(Part>0?"（续）":""));UOpusUI::SetText(R,"row:description",Description.Mid(Part*88,88));UOpusUI::SetAction(R,"event-line-row",Selectable&&L.Index>=0?"affix":"noop",""+L.Index);UOpusWorldUI::Emphasize(R,"event-line-row",Selectable&&L.Index==SelectedAffix);List.Add(R);
   }
  }
  UOpusWorldUI::MountRows(Panel,"event:rows-"+Col,List);
 }
 void Bag(int Col,UDWOMapEventState S)
 {
  TArray<UOpusWorldScreen> List;for(auto E:UOpusMapEvents::Equipment(Tower.Rules))
  {
   if(E.Equipped||!E.Identified||E.Locked||E.Slot=="map"||E.Slot=="character"||E.Slot=="default"||E.Slot=="tower"||E.Slot=="rarity"||E.Slot=="lineage")continue;
   auto R=Row("event-item-row");if(R==nullptr)continue;UOpusUI::SetText(R,"row:name",E.Name);UOpusUI::SetText(R,"row:meta",Meta(E));UOpusUI::SetImage(R,"row:icon",E.Icon);UOpusUI::SetAction(R,"event-item-row","bag",E.Id);UOpusWorldUI::Emphasize(R,"event-item-row",E.Id==S.Left||E.Id==S.Right);List.Add(R);
  }
  UOpusWorldUI::MountRows(Panel,"event:rows-"+Col,List);if(List.Num()==0)Text("event:meta-"+Col,"没有可放入的装备，请先鉴定并卸下");
 }
 void Icon(int Col,FOpusEntry E){if(E.Id.IsEmpty())return;UOpusUI::SetImage(Panel,"event:icon-"+Col,E.Icon);UOpusUI::SetShown(Panel,"event:icon-"+Col,true);}
 void Item(int Col,FString Id,bool Selectable=false){auto E=Equipment(Id);Icon(Col,E);Text("event:heading-"+Col,Id.IsEmpty()?"尚未放入装备":E.Name);Text("event:meta-"+Col,Id.IsEmpty()?"从背包选择":Meta(E));Lines(Col,UOpusData::HoverLines(Tower.Rules,Id),Selectable);}
 bool EnsureOffer(UDWOMapEventState S)
 {
  if(S.Offer!=nullptr)return true;auto O=Cast<UOpusEventOffer>(NewObject(this,UOpusEventOffer));FString Error;
  if(!O.Generate(Tower.Rules,CandidateSnapshot,S.Random.RandRange(1,2000000000),TierSnapshot,S.Definition.Rarity,S.Definition.CandidateCount,S.Definition.ChooseCount,Error)){Status=Error;return false;}S.Offer=O;return true;
 }
 void Refresh()
 {
  if(CraftUI!=nullptr&&CraftUI.IsCraftOpen()){CraftUI.Refresh();return;}
  if(Panel==nullptr||!States.IsValidIndex(Active))return;auto S=States[Active];Rows.Empty();ItemUsed=0;LineUsed=0;Text("event:title",S.Definition.Name);Text("event:description",S.Definition.Description);
  for(int I=0;I<3;I++){UOpusUI::SetShown(Panel,"event:icon-"+I,false);Text("event:heading-"+I,"");Text("event:meta-"+I,"");TArray<UOpusWorldScreen> Empty;UOpusWorldUI::MountRows(Panel,"event:rows-"+I,Empty);UOpusUI::SetShown(Panel,"event:select-"+I,false);}
  UOpusUI::SetShown(Panel,"event:secondary",S.Definition.Kind=="altar"||S.Definition.Kind=="fusion");UOpusUI::SetShown(Panel,"event:primary",!S.Spent);
  Text("event:warning","");Text("event:secondary:text","收取装备并结束");
  if(S.Definition.Kind=="altar")
  {
   Text("event:heading-0",S.Locked?"装备已绑定本坛":"背包 · 选择一件装备");if(!S.Locked)Bag(0,S);
   Item(1,S.Left,S.Locked);Text("event:heading-2","法坛通货 · 无限使用");FString CurrencyName=S.Currency==n"chaos_orb"?"下品灵石":S.Currency==n"quad_affix_orb"?"陨星石":S.Currency.ToString();for(auto E:UOpusData::Merchant(Tower.Rules))if(E.Id==S.Currency.ToString()){CurrencyName=E.Name;UOpusUI::SetImage(Panel,"event:icon-2",E.Icon);UOpusUI::SetShown(Panel,"event:icon-2",true);break;}Text("event:meta-2",CurrencyName+" · 不消耗背包通货");
   auto Note=Row("event-line-row");TArray<UOpusWorldScreen> Notes;if(Note!=nullptr){UOpusUI::SetText(Note,"row:text","制作次数："+S.Crafts);UOpusUI::SetText(Note,"row:description","锁定后本图只能加工这一件。关闭界面仍保留锁定；收取后祭坛结束。离图时自动解除锁定，保留结果。");UOpusUI::SetAction(Note,"event-line-row","noop","");Notes.Add(Note);}UOpusWorldUI::MountRows(Panel,"event:rows-2",Notes);
   Text("event:primary:text",S.Locked?"使用法坛通货":"放入并锁定");Text("event:warning",S.Locked?"选择可洗炼词条后制作；整件重洗通货无需选择词条。":"只能放入已鉴定、未穿戴的背包装备。");
   if(S.Locked){auto Craft=UOpusData::CraftState(Tower.Rules,S.Left,S.Currency,n"",true);if(!Craft.Blocked.IsEmpty()&&Status.IsEmpty())Status=Craft.Blocked;}
  }
  else if(S.Definition.Kind=="fusion")
  {
   Item(0,S.Left);Item(2,S.Right);Text("event:heading-1",S.Spent?"合成结果":S.Locked?"准备合成":"背包 · 点击放入"+FString(DepositSide==0?"左件":"右件"));if(!S.Locked&&!S.Spent)Bag(1,S);
   if(S.Locked)Text("event:meta-1","已锁定两件装备，确认后一起消耗");
   if(S.Spent){Text("event:heading-0","左件已消耗");Text("event:meta-0","继承 "+S.DrawLeft+" 条词条");Text("event:heading-2","右件已消耗");Text("event:meta-2","继承 "+S.DrawRight+" 条词条");}
   if(S.Spent&&S.Offer!=nullptr){auto E=S.Offer.Entries()[0];Icon(1,E);Text("event:heading-1",E.Name);Text("event:meta-1","新装备 · "+Meta(E));Lines(1,S.Offer.Lines(0));}
   for(int I=0;I<3;I+=2)if(!S.Locked&&!S.Spent){UOpusUI::SetShown(Panel,"event:select-"+I,true);Text("event:select-"+I+":text",I==0?"下一件放左边":"下一件放右边");UOpusUI::SetAction(Panel,"event:select-"+I,"side",I==0?"0":"1");}
   Text("event:primary:text",S.Locked?"开始合成（消耗两件）":"锁定两件装备");Text("event:warning","仅限同部位。沿用左件底材和 T 阶。左右各抽 0～全部词条，固定传奇也参与，可继承多个传奇词条。");
  }
  else
  {
   bool Cleared=Tower.Run.Phase==EDWOTowerPhase::Cleared;
   if(!Cleared){Text("event:heading-1","击败本图首领后解封");Text("event:meta-1","奇遇已标记，完成后回来交互");Text("event:primary:text","继续挑战");}
   else if(S.Definition.Kind=="vault"&&!S.GuardsCleared){Text("event:heading-1","封印守库者");Text("event:meta-1","挑战 "+S.Definition.Guards+" 名精英守卫");Text("event:primary:text","解除封印并迎战");}
   else if(EnsureOffer(S))
   {
    auto Entries=S.Offer.Entries();for(int I=0;I<Math::Min(3,Entries.Num());I++){auto E=Entries[I];Icon(I,E);Text("event:heading-"+I,E.Name);Text("event:meta-"+I,Meta(E));Lines(I,S.Offer.Lines(I));UOpusUI::SetShown(Panel,"event:select-"+I,true);UOpusUI::SetAction(Panel,"event:select-"+I,"reward",""+I);Text("event:select-"+I+":text",I==SelectedReward?"✓ 已选择":"选择这件");}Text("event:primary:text","领取所选装备");Text("event:warning","三选一。关闭再打开不会改变候选，领取后其余候选消失。");
   }
  }
  Text("event:status",Status);
 }
 bool Deposit(UDWOMapEventState S,FString Uid)
 {
  if(S.Locked||S.Spent){Status="这座祭坛已经锁定装备";return false;}auto E=Equipment(Uid);if(E.Id.IsEmpty()||E.Equipped||!E.Identified||E.Locked){Status="只能放入已鉴定、未穿戴且未锁定的装备";return false;}
  if(S.Definition.Kind=="altar"){S.Left=Uid;return true;}if(S.Definition.Kind!="fusion")return false;
  FString Other=DepositSide==0?S.Right:S.Left;if(Other==Uid){Status="不能将同一件装备放入两侧";return false;}if(!Other.IsEmpty()&&Equipment(Other).Slot!=E.Slot){Status="两件装备必须为相同部位";return false;}
  if(DepositSide==0){S.Left=Uid;DepositSide=1;}else{S.Right=Uid;DepositSide=0;}return true;
 }
 bool Lock(UDWOMapEventState S)
 {
  if(S.Locked)return true;FString Error;if(S.Left.IsEmpty()||(S.Definition.Kind=="fusion"&&(S.Right.IsEmpty()||Equipment(S.Left).Slot!=Equipment(S.Right).Slot))){Status="请先放入所需的同部位装备";return false;}
  if(!Tower.Rules.LockPlayerEventItem(S.Left,S.Token,Error)){Status=Error;return false;}
  if(S.Definition.Kind=="fusion"&&!Tower.Rules.LockPlayerEventItem(S.Right,S.Token,Error)){FString Ignore;Tower.Rules.UnlockPlayerEventItem(S.Left,S.Token,Ignore);Status=Error;return false;}
  S.Locked=true;Status="已锁定装备，关闭界面仍可回来继续";return true;
 }
 TArray<int> Draw(FRandomStream& Stream,int Count)
 {
  TArray<int> Pool,Out;for(int I=0;I<Count;I++)Pool.Add(I);int Take=Stream.RandRange(0,Count);for(int I=0;I<Take;I++){int At=Stream.RandRange(0,Pool.Num()-1);Out.Add(Pool[At]);Pool.RemoveAt(At);}return Out;
 }
 bool Fuse(UDWOMapEventState S)
 {
  if(!S.Locked||S.Spent)return false;auto NextRandom=S.Random;auto Left=Draw(NextRandom,UOpusMapEvents::FusionLineCount(Tower.Rules,S.Left));auto Right=Draw(NextRandom,UOpusMapEvents::FusionLineCount(Tower.Rules,S.Right));
  auto O=Cast<UOpusEventOffer>(NewObject(this,UOpusEventOffer));FString Error;TArray<int> Choice;Choice.Add(0);
  if(!O.PrepareFusion(Tower.Rules,S.Left,S.Right,S.Token,Left,Right,Error)||!O.Claim(Choice,Error)){Status=Error;return false;}
  S.Random=NextRandom;S.DrawLeft=Left.Num();S.DrawRight=Right.Num();S.Offer=O;S.Locked=false;S.Spent=true;S.Left="";S.Right="";S.Marker.Finish();Status="合成完成：左侧继承 "+Left.Num()+" 条，右侧继承 "+Right.Num()+" 条。新装备已放入背包。";Tower.SaveProfile();return true;
 }
 UFUNCTION() void OnAction(USvgCanvasNode Node)
 {
  auto A=UOpusUI::Action(Node);if(A.Op=="open"){Open(String::Conv_StringToInt(A.Value));return;}if(A.Op=="close"){Close();return;}if(!States.IsValidIndex(Active))return;auto S=States[Active];Status="";
  if(CraftUI!=nullptr&&CraftUI.Handle(A))return;
  if(A.Op=="side")DepositSide=String::Conv_StringToInt(A.Value);
  if(A.Op=="bag")Deposit(S,A.Value);
  if(A.Op=="affix")SelectedAffix=String::Conv_StringToInt(A.Value);
  if(A.Op=="reward")SelectedReward=String::Conv_StringToInt(A.Value);
  if(A.Op=="collect")
  {
   if(S.Locked){FString Error;Tower.Rules.UnlockPlayerEventItem(S.Left,S.Token,Error);if(!S.Right.IsEmpty())Tower.Rules.UnlockPlayerEventItem(S.Right,S.Token,Error);S.Locked=false;S.Spent=true;S.Marker.Finish();Tower.SaveProfile();}else if(!S.Spent){Status="尚未锁定装备";Refresh();return;}Close();return;
  }
  if(A.Op=="primary")
  {
   if(S.Definition.Kind=="altar")
   {
    if(!S.Locked)Lock(S);else{FString Error;auto C=UOpusData::CraftState(Tower.Rules,S.Left,S.Currency,n"",true);int Index=C.Operation=="reroll"?SelectedAffix:-1;if(Tower.Rules.CraftPlayerEventItem(S.Left,S.Token,S.Currency,Index,Error)){S.Crafts++;Status="制作成功 · 第 "+S.Crafts+" 次";Tower.SaveProfile();}else Status=Error;}
   }
   else if(S.Definition.Kind=="fusion"){if(!S.Locked)Lock(S);else Fuse(S);}
   else if(Tower.Run.Phase!=EDWOTowerPhase::Cleared){Close();return;}
   else if(S.Definition.Kind=="vault"&&!S.GuardsCleared){Close();if(Tower.SpawnEventGuards(S.Token,S.Definition.Guards,S.Random.RandRange(1,2000000000),S.Position))S.GuardsStarted=true;return;}
   else if(S.Offer!=nullptr){TArray<int> Selection;Selection.Add(SelectedReward);FString Error;if(S.Offer.Claim(Selection,Error)){S.Spent=true;S.Marker.Finish();Tower.SaveProfile();Close();return;}Status=Error;}
  }
  Tower.Hud.RefreshIn=0;Refresh();
 }
}
