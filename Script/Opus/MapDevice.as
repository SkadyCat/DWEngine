// Authored UICreator visuals, real inventory reservations, AS-only interaction.
class UDWOMapDevice : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() UOpusWorldScreen Screen;
 UPROPERTY() UOpusWorldScreen Ghost;
 UPROPERTY() UOpusWorldScreen PlayerMarker;
 FString PlayerMarkerNode;
 UPROPERTY() UDWOAtlasState Atlas;
 UPROPERTY() UOpusHoverEvents Events;
 TArray<FOpusEntry> Bag;
 bool Visible=false,Held=false,Dragging=false,Panning=false,SkipClick=false,SuppressKeys=false,Ready=false,HoverAfterDrop=false;
 FString HoverRegion;
 FString DragId,DragSlot,Filter="all",Feedback,InventorySignature;
 FVector2D Press,Last,Pan;
 float Zoom=.48,Poll=0,FitIn=0;
 int Page=0;
 void Initialize(UDWOSvgHUD InHud)
 {
  Hud=InHud;Atlas=NewObject(this,UDWOAtlasState);
  if(!Atlas.Restore(Hud.Tower.MapItems.State.AtlasState)){Hud.Tower.LastError="星图存档无效，已保留原始存档";UOpusNative::WriteLog("MAP_DEVICE_ERROR invalid atlas state");return;}
  auto ActiveMap=Hud.Tower.MapItems.State.Active;if(!Atlas.Active.IsEmpty()&&ActiveMap.Tier>=1&&ActiveMap.Tier<=16&&!ActiveMap.Uid.IsEmpty()){Atlas.MapTiers.Add(Atlas.Active,ActiveMap.Tier);Atlas.Build();}
  Ready=true;
 }
 FString SlotId(int I) const{return I==0?"map":"modifier-"+(I-1);}
 int SlotIndex(FString Id) const {if(Id=="map")return 0;for(int I=1;I<6;I++)if(Id==SlotId(I))return I;return -1;}
 bool Inside(FVector2D P,FVector4 R) const{return R.Z>0&&R.W>0&&P.X>=R.X&&P.Y>=R.Y&&P.X<R.X+R.Z&&P.Y<R.Y+R.W;}
 bool Owned(FString Id,FOpusEntry& Item) const {for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Id==Id&&E.Count>0){Item=E;return true;}return false;}
 int Reserved(FString Id) const {int Count=0;for(auto V:Hud.Tower.MapItems.State.DeviceSlots)if(V==Id&&!Id.IsEmpty())Count++;return Count;}
 bool Save()
 {
  Hud.Tower.MapItems.State.AtlasState=Atlas.Serialize();FString Error;
  if(!Hud.Tower.MapItems.Persist(Error)){Hud.Tower.LastError=Error;Feedback=Error;return false;}
  Hud.Tower.MapItems.Publish();return true;
 }
 void Open()
 {
  if(!Ready||Atlas==nullptr)return;if(Hud.Talents!=nullptr)Hud.Talents.Close();Hud.ItemHover.Close();Hud.Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Hud.Tower.Rules)+1));
  if(Screen==nullptr){
   Screen=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"battle-map-device","UICreatorMaps",150);
   if(Screen==nullptr){Hud.Tower.LastError="地图 UICreator 资源尚未导入";return;}
   Screen.OnSourceAction.AddUFunction(this,n"OnAction");Screen.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");
   Events=UOpusUI::HoverEvents(Screen);Events.Entered.AddUFunction(this,n"EnterHover");Events.Left.AddUFunction(this,n"LeaveHover");
   // These outer buttons are socket hit regions. Their overlay must not cover
   // the mounted item button, which supplies the real hover and drag source.
   for(int I=0;I<6;I++){auto Socket=Screen.FindSourceNode("map-device-preview-map-device-slot-"+SlotId(I));if(Socket!=nullptr)for(auto W:Socket.GetAllChildren()){auto B=Cast<UButton>(W);if(B!=nullptr)B.SetVisibility(ESlateVisibility::HitTestInvisible);}}
  }
  Visible=true;Screen.SetVisibility(ESlateVisibility::SelfHitTestInvisible);Held=UOpusMapUI::MouseDown();FitIn=0;InventorySignature="";CenterAtlas();Feedback="拖入区域详情装配 · 拖回背包取下";Refresh();DrawAtlas();UOpusMapUI::Fit(Screen,Gameplay::GetPlayerController(0));
 }
 void CancelDrag(){if(Ghost!=nullptr)Ghost.RemoveFromParent();Ghost=nullptr;Dragging=false;Panning=false;DragId="";DragSlot="";}
 void Close(){if(PlayerMarker!=nullptr)PlayerMarker.SetVisibility(ESlateVisibility::Collapsed);CancelDrag();Visible=false;Hud.ItemHover.Close();if(Screen!=nullptr)Screen.SetVisibility(ESlateVisibility::Collapsed);}
 UFUNCTION() void EnterHover(USvgCanvasNode Node){if(!Dragging&&!Panning)Hud.ItemHover.Enter(Node);}
 UFUNCTION() void LeaveHover(USvgCanvasNode Node){Hud.ItemHover.Leave(Node);}
 UFUNCTION() bool FilterKey(FKey Key)
 {
  if(!Visible)return false;
  if(Key==EKeys::Escape&&(Dragging||Panning)){CancelDrag();SkipClick=true;SuppressKeys=true;return true;}
  if(Hud.ItemHover.FilterKey(Key)){SuppressKeys=true;return true;}
  if(Key==EKeys::M||Key==EKeys::Escape){Close();SuppressKeys=true;return true;}
  if(Key==EKeys::F){CenterAtlas();DrawAtlas();return true;}return false;
 }
 bool HandleKeys(APlayerController PC)
 {
  if(SuppressKeys){SuppressKeys=false;return true;}if(!Visible)return false;
  if(PC.WasInputKeyJustPressed(EKeys::M)||PC.WasInputKeyJustPressed(EKeys::Escape)){FilterKey(PC.WasInputKeyJustPressed(EKeys::M)?EKeys::M:EKeys::Escape);return true;}
  Hud.ItemHover.HandleKeys(PC);if(PC.WasInputKeyJustPressed(EKeys::F)){CenterAtlas();DrawAtlas();}
  if(PC.WasInputKeyJustPressed(EKeys::MouseScrollUp)){ZoomAtlas(1.15f);}
  if(PC.WasInputKeyJustPressed(EKeys::MouseScrollDown)){ZoomAtlas(1/1.15f);}return true;
 }
 FOpusUIRow Row(FOpusEntry E,int Count) const
 {FOpusUIRow R;R.Label=E.Name;R.Detail=E.Description;R.Value=E.Id;R.Op="select";R.Kind=E.Kind;R.Rarity=E.Rarity;R.Icon=E.Icon;R.Tier=E.Tier;R.Count=Count;return R;}
 void Text(FString Binding,FString Value){UOpusUI::SetText(Screen,Binding,Value);}
 void Refresh()
 {
  if(Screen==nullptr||!Visible)return;Bag.Empty();TArray<FOpusEntry> All=UOpusData::Inventory(Hud.Tower.Rules);int Total=0;
  for(auto E:All){int Count=E.Count-Reserved(E.Id);if(Count<=0)continue;Total++;if((Filter=="map"&&E.Category!="map")||Filter=="modifier")continue;Bag.Add(E);}
  int Pages=Math::Max(1,(Bag.Num()+34)/35);Page=Math::Clamp(Page,0,Pages-1);TArray<FOpusUIRow> Rows;
  for(int I=Page*35;I<Math::Min(Bag.Num(),Page*35+35);I++)Rows.Add(Row(Bag[I],Bag[I].Count-Reserved(Bag[I].Id)));
  UOpusUI::SetRows(Screen,"map-device-bag",Rows,7);Text("map-device-bag-count",""+Total+" 项");Text("map-device-page",""+(Page+1)+" / "+Pages);Text("map-device-feedback",Feedback);UOpusUI::SetShown(Screen,"map-device-bag-empty",Bag.Num()==0);
  UOpusWorldUI::Emphasize(Screen,"map-device-filter:all",Filter=="all");UOpusWorldUI::Emphasize(Screen,"map-device-filter:map",Filter=="map");UOpusWorldUI::Emphasize(Screen,"map-device-filter:modifier",Filter=="modifier");
  auto Slots=Hud.Tower.MapItems.State.DeviceSlots;FString MapName="尚未放入地图";
  for(int I=0;I<6;I++){
   auto Id=SlotId(I);FOpusEntry E;bool Present=Slots.IsValidIndex(I)&&Owned(Slots[I],E);TArray<FOpusUIRow> One;if(Present)One.Add(Row(E,1));
   UOpusUI::SetRows(Screen,"map-device-mounted:"+Id,One);
   UOpusUI::SetShown(Screen,"map-device-empty:"+Id,!Present);UOpusUI::SetShown(Screen,"map-device-empty-label:"+Id,!Present);
   if(I==0&&Present)MapName=E.Name;
  }
  Text("map-device-selection",MapName);Details();
 }
 bool PlaceItem(FString Id,FString Slot,FString SourceSlot="")
 {
  int At=SlotIndex(Slot),From=SlotIndex(SourceSlot);FOpusEntry E;if(At<0||!Owned(Id,E))return false;
  if(At!=0||E.Category!="map"){Feedback=At==0?"中央格只能放地图":"地图物品尚未制作，此槽暂时保留";Refresh();return false;}
  if(From>=0&&Hud.Tower.MapItems.State.DeviceSlots[From]!=Id)return false;
  if(From<0&&Reserved(Id)>=E.Count){Feedback="该物品已经装配";Refresh();return false;}
  auto Before=Hud.Tower.MapItems.State;auto Slots=Before.DeviceSlots;if(From==At)return true;
  if(From>=0)Slots[From]="";Slots[At]=Id;Hud.Tower.MapItems.State.DeviceSlots=Slots;
  if(!Save()){Hud.Tower.MapItems.State=Before;return false;}Feedback="已装配 "+E.Name;Refresh();return true;
 }
 bool RemoveItem(FString Slot)
 {
  int At=SlotIndex(Slot);if(At<0)return false;auto Before=Hud.Tower.MapItems.State;if(Before.DeviceSlots[At].IsEmpty())return false;
  Hud.Tower.MapItems.State.DeviceSlots[At]="";if(!Save()){Hud.Tower.MapItems.State=Before;return false;}Feedback="已放回背包";Refresh();return true;
 }
 void EnterRegion()
 {
  if(!CanEnter()){Feedback=EntryError();Details();return;}
  auto M=Hud.Tower.MapItems;FOpusEntry MapItem;
  if(Atlas.Active==Atlas.Selected&&!Atlas.Active.IsEmpty()){Close();return;}
  if(Atlas.Selected=="0:0"&&Atlas.Active.IsEmpty()&&(!M.State.DeviceSlots.IsValidIndex(0)||!Owned(M.State.DeviceSlots[0],MapItem)||MapItem.Category!="map"))
  {
   auto Previous=Atlas.Serialize();if(!Atlas.Enter(1)){Feedback="初始地域当前不可进入";Refresh();return;}
   auto Old=M.State;M.State.Active=FOpusMapItem();M.State.AtlasState=Atlas.Serialize();M.Publish();FString Error;
   if(!M.Persist(Error)){M.State=Old;M.Publish();Atlas.Restore(Previous);Feedback=Error;Refresh();return;}
   Hud.Tower.Run.Floor=1;UDWOTowerJourney::Get().StartOnArrival=false;Close();Hud.Tower.Travel(false);return;
  }
  if(!M.State.DeviceSlots.IsValidIndex(0)||!Owned(M.State.DeviceSlots[0],MapItem)||MapItem.Category!="map"){Feedback="请先把一张地图拖入中央格";Refresh();return;}
  auto OldAtlas=Atlas.Serialize();auto OldState=M.State;
  if(!Atlas.Enter(MapItem.Tier)){Feedback="请选择可探索或挑战失败的相连地域";Refresh();return;}
  M.State.AtlasState=Atlas.Serialize();FString Error;
  if(!M.Use(MapItem.Id,Error)){M.State=OldState;Atlas.Restore(OldAtlas);Feedback=Error;Refresh();return;}
  Close();
 }
 UFUNCTION() void OnAction(USvgCanvasNode Node)
 {
  if(!Visible)return;auto A=UOpusUI::Action(Node);Screen.SourceActionHandled=true;if(SkipClick)return;
  if(A.Op=="close"||A.Op=="back"){Close();return;}
  if(A.Op=="map-device-filter"){Filter=A.Value;Page=0;Refresh();return;}
  if(A.Op=="map-device-page"){Page=Math::Max(0,Page+String::Conv_StringToInt(A.Value));Refresh();return;}
  if(A.Op=="atlas-region"||A.Op=="region"){if(Atlas.Select(A.Value)){Save();DrawAtlas();Details();}return;}
  if(A.Op=="atlas-center"){CenterAtlas();DrawAtlas();return;}
  if(A.Op=="atlas-zoom"){ZoomAtlas(A.Value=="-1"?1/1.25f:1.25f);return;}
  if(A.Op=="region-enter")EnterRegion();
 }
 FString Status(FString V) const{return V=="completed"?"已通关":V=="active"?"进行中":V=="available"?"可探索":V=="failed"?"挑战失败":"未连通";}
 int RegionTier(FDWOAtlasNode N) const
 {
  if(N.Id==Atlas.Active){auto A=Hud.Tower.MapItems.State.Active;return !A.Uid.IsEmpty()&&A.Tier>=1&&A.Tier<=16?A.Tier:N.Floor;}
  FOpusEntry M;if(Hud.Tower.MapItems.State.DeviceSlots.IsValidIndex(0)&&Owned(Hud.Tower.MapItems.State.DeviceSlots[0],M)&&M.Category=="map"&&M.Tier>=1&&M.Tier<=16)return M.Tier;
  return N.Floor;
 }
 FString TierLabel(FDWOAtlasNode N) const {int T=RegionTier(N);return T>0?"T"+T+" · 第 "+T+" 层":"层级由放入的地图决定";}
 bool MountedMap(FOpusEntry& Item) const
 {return Hud.Tower.MapItems.State.DeviceSlots.IsValidIndex(0)&&Owned(Hud.Tower.MapItems.State.DeviceSlots[0],Item)&&Item.Category=="map"&&Item.Tier>=1&&Item.Tier<=16;}
 FString EntryError() const
 {
  int I=Atlas.Index(Atlas.Selected);if(I<0)return "请选择区域";
  if(Atlas.Selected==Atlas.Active&&!Atlas.Active.IsEmpty())return "";
  if(!Atlas.Active.IsEmpty())return "请先完成当前挑战";
  if(Hud.Tower.Traveling||Hud.Tower.Preparing||Hud.Tower.Run.Phase==EDWOTowerPhase::Waves||Hud.Tower.Run.Phase==EDWOTowerPhase::Boss||Hud.Tower.Rules.GetPlayerPools().bDead)return "请在战斗结束后进入";
  auto N=Atlas.Nodes[I];if(N.Status!="available"&&N.Status!="failed"&&!(N.Id=="0:0"&&N.Status=="completed"))return N.Status=="completed"?"区域已通关":"尚未连通";
  FOpusEntry Item;bool Present=Hud.Tower.MapItems.State.DeviceSlots.IsValidIndex(0)&&Owned(Hud.Tower.MapItems.State.DeviceSlots[0],Item)&&Item.Category=="map";
  if(Present)return Item.Tier>=1&&Item.Tier<=16?"":"地图层级必须为 T1–T16";
  return N.Id=="0:0"?"":"请拖入地图";
 }
 bool CanEnter() const{return EntryError().IsEmpty();}
 bool EntryActive() const{FOpusEntry Item;return CanEnter()&&Atlas.Selected!=Atlas.Active&&MountedMap(Item);}
 void Details()
 {
  int I=Atlas.Index(Atlas.Selected);if(I<0)return;auto N=Atlas.Nodes[I];Text("region-name",N.Name);Text("region-biome",N.Biome);Text("atlas-coordinates",""+int(Math::RoundToInt(N.Position.X))+" · "+int(Math::RoundToInt(N.Position.Y)));Text("region-floor",TierLabel(N)+" · "+N.Biome);
  FOpusEntry MapItem;bool HasMap=MountedMap(MapItem),Enabled=CanEnter();
  Text("region-note",!Enabled?EntryError():N.Id==Atlas.Active?"返回战场不消耗地图":HasMap?"进入消耗 1 张 T"+MapItem.Tier+" 地图 · 第 "+MapItem.Tier+" 层":"起始地图 T1 无需地图票");
  Text("map-device-enter-label",!Enabled?EntryError():N.Id==Atlas.Active?"返回战场 →":HasMap?"使用地图进入 →":"进入起始地图 T1 →");
  auto Enter=Cast<USvgCanvasMotionNode>(Screen.FindSourceNode("map-device-preview-battle-atlas-enter"));if(Enter!=nullptr){Enter.SetInteractionEnabled(Enabled);Enter.SetMotionEnabled(EntryActive());}
 }
 uint VisualHash(FString Value) const {uint V=2166136261;for(int I=0;I<Value.Len();I++){V^=uint(Value[I]);V*=16777619;}return V;}
 FString Tone(FString Status) const {return Status=="completed"?"#c4ddaa":Status=="available"?"#d8b570":Status=="active"?"#8de1d1":Status=="failed"?"#d78970":"#79847a";}
 bool OpenStatus(FString S) const {return S!="hidden"&&S!="locked";}
 void CenterAtlas(){int I=Atlas.Index(Atlas.Current);if(I<0)I=Atlas.Index("0:0");if(I>=0)Pan=-Atlas.Nodes[I].Position*Zoom;}
 void ZoomAtlas(float Factor)
 {
  auto R=UOpusMapUI::Bounds(Screen,"atlas-viewport");auto P=UOpusTalentBridge::Pointer(Screen,Gameplay::GetPlayerController(0));auto Offset=Inside(P,R)?P-FVector2D(R.X+R.Z*.5,R.Y+R.W*.5):FVector2D();
  float Next=Math::Clamp(Zoom*Factor,.28f,2.4f);Pan=Offset-(Offset-Pan)*(Next/Zoom);Zoom=Next;DrawAtlas();
 }
 FString PickRegion(FVector2D P) const
 {
  auto R=UOpusMapUI::Bounds(Screen,"atlas-viewport");if(!Inside(P,R))return "";
  auto World=(P-FVector2D(R.X+R.Z*.5,R.Y+R.W*.5)-Pan)/Zoom;float Best=Math::Max(16.f/Zoom,22.f);FString Id;
  for(auto N:Atlas.Nodes)if(N.Status!="hidden"){float Distance=(N.Position-World).Size();if(Distance<Best){Best=Distance;Id=N.Id;}}return Id;
 }
 void DrawAtlas()
 {
  if(Screen==nullptr||!Visible)return;TArray<FOpusPlotPoint> Points;TArray<FOpusPlotPath> Paths;int Available=0;FOpusEntry Ticket;bool HasTicket=MountedMap(Ticket);
  for(auto N:Atlas.Nodes){
   bool Reachable=Atlas.Active.IsEmpty()&&(N.Status=="available"||N.Status=="failed"||(N.Id=="0:0"&&N.Status=="completed"))&&(HasTicket||N.Id=="0:0");if(Reachable)Available++;
   bool Hidden=N.Status=="hidden",Locked=N.Status=="locked",Selected=N.Id==Atlas.Selected,IsCurrent=N.Id==Atlas.Current,Highlight=Selected||IsCurrent||N.Id==HoverRegion||Reachable;
   FOpusPlotPoint P;P.Position=N.Position;P.Radius=(Hidden?3.f:Selected||IsCurrent?17.f:Reachable?15.f:Locked?9.f:13.f)/Math::Sqrt(Zoom);P.Width=Selected?2:1;P.Stroke=Hidden?"":Tone(N.Status);P.Fill=Hidden?"rgba(112,148,126,.3)":Locked?"#172623":"#152a25";
   if(!Hidden){P.FogRadius=Locked?120:195;P.FogStrength=Locked?.6f:1.f;}
   if(Highlight&&!Hidden){P.Ring=P.Radius+6/Zoom;P.RingColor=IsCurrent?"#9ae9d8":"#e3c083";P.Glow=IsCurrent?"rgba(104,230,204,.32)":"rgba(231,184,94,.3)";}
   if(!Hidden){P.Glyph=N.Status=="completed"?-3:Locked?-2:int(VisualHash(N.Biome=="密林"?"forest":N.Biome=="沼泽"?"marsh":N.Biome=="遗迹"?"ruins":N.Biome=="荒漠"?"desert":N.Biome=="雪域"?"snow":"coast")%4);P.GlyphRadius=Locked?2/Zoom:P.Radius*.8f;}
   if(!Hidden&&(Highlight||(Zoom>.82f&&!Locked))){P.Label=N.Name;P.LabelColor=Selected?"#e6d7b1":"#adbcb0";}
   if(IsCurrent){P.Radius=0;P.Width=0;P.Fill="";P.Stroke="";P.Ring=0;P.Glow="";P.Glyph=-1;P.GlyphRadius=0;P.Label="";}
   Points.Add(P);
   for(auto Id:N.Neighbors)if(N.Id<Id){int I=Atlas.Index(Id);if(I<0)continue;auto B=Atlas.Nodes[I];auto D=B.Position-N.Position;float Curve=(int(VisualHash(N.Id+Id)%21)-10)*.9f;auto C=(N.Position+B.Position)*.5f+FVector2D(-D.Y,D.X)/140*Curve;
    bool Obscured=Hidden||B.Status=="hidden",Open=OpenStatus(N.Status)&&OpenStatus(B.Status),Cleared=N.Status=="completed"&&B.Status=="completed";
    FOpusPlotPath L;L.Color=Obscured?"rgba(120,152,136,.115)":Cleared?"#8ea889":Open?"rgba(205,174,108,.65)":"rgba(121,143,123,.3)";L.Width=Open?1.5f:.8f;L.Dash=Obscured?2:0;
    for(int Step=0;Step<=12;Step++){float T=float(Step)/12;L.Points.Add(N.Position*((1-T)*(1-T))+C*(2*(1-T)*T)+B.Position*(T*T));}Paths.Add(L);
   }
  }
  UOpusMapUI::Plot(Screen,"map-device-preview-atlas-plot",Points,Paths,-Pan/Zoom,Zoom);
  PlacePlayerMarker();
  int CurrentIndex=Atlas.Index(Atlas.Current);Text("atlas-summary","玩家位置："+(CurrentIndex>=0?Atlas.Nodes[CurrentIndex].Name:"初始界域")+"　 |　 "+Available+" 处可前往 · 金色高亮");Text("atlas-zoom",""+int(Zoom*100)+"%");Details();
 }
 void PlacePlayerMarker()
 {
  if(Screen==nullptr||!Visible)return;int I=Atlas.Index(Atlas.Current);if(I<0)return;auto R=UOpusMapUI::Bounds(Screen,"atlas-viewport");auto P=FVector2D(R.X+R.Z*.5,R.Y+R.W*.5)+Pan+Atlas.Nodes[I].Position*Zoom;
  bool Show=P.X>=R.X&&P.X<=R.X+R.Z&&P.Y>=R.Y&&P.Y<=R.Y+R.W;
  auto Host=Screen.FindSourceNode("map-device-preview-battle-atlas-map-viewport");if(Host==nullptr)return;
  if(PlayerMarker==nullptr){PlayerMarker=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"atlas-player-location","UICreatorMaps",170,false);if(PlayerMarker!=nullptr){auto Slot=Host.AddChildToCanvas(PlayerMarker);Slot.SetSize(FVector2D(176,112));Slot.SetZOrder(100);}}
  if(PlayerMarker==nullptr)return;PlayerMarkerNode=Atlas.Current;UOpusUI::SetText(PlayerMarker,"atlas-player-name",Atlas.Nodes[I].Name);PlayerMarker.SetVisibility(Show?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);auto Slot=Cast<UCanvasPanelSlot>(PlayerMarker.Slot);if(Slot!=nullptr)Slot.SetPosition(P-FVector2D(R.X+88,R.Y+76));
 }
 void PointerFrame()
 {
  auto PC=Gameplay::GetPlayerController(0);auto P=UOpusTalentBridge::Pointer(Screen,PC);bool Down=UOpusMapUI::MouseDown();
  if(HoverAfterDrop&&!Down){HoverAfterDrop=false;EnterHover(UOpusMapUI::Hit(Screen,PC));}
  if(Down&&!Held){
   SkipClick=false;Press=P;Last=P;auto N=UOpusMapUI::Hit(Screen,PC);auto A=UOpusUI::Action(N);DragId=A.Op=="select"?A.Value:"";DragSlot="";
   UWidget W=N;while(W!=nullptr){auto C=Cast<USvgCanvasNode>(W);if(C!=nullptr&&C.Binding.StartsWith("map-device-slot:")){DragSlot=C.Binding.Mid(16);break;}W=W.GetParent();}
  }
  if(Down&&Held&&!Panning&&!Dragging&&(P-Press).Size()>6){
   if(!DragId.IsEmpty()){Dragging=true;Hud.ItemHover.Close();Ghost=UOpusWorldUI::CreateTemplate(PC,"map-device-drag-template","UICreatorMaps",220);FOpusEntry E;if(Ghost!=nullptr&&Owned(DragId,E)){UOpusUI::SetImage(Ghost,"map-device-drag-icon",E.Icon);Ghost.SetVisibility(ESlateVisibility::HitTestInvisible);}}
   else if(Inside(Press,UOpusMapUI::Bounds(Screen,"atlas-viewport"))){Panning=true;Hud.ItemHover.Close();}
   SkipClick=Dragging||Panning;
  }
  if(Panning&&Down){Pan+=P-Last;DrawAtlas();}
  if(Dragging&&Ghost!=nullptr){auto V=UOpusHoverUI::AnchorRect(Screen,PC);float32 X=0,Y=0;PC.GetMousePosition(X,Y);auto Size=UOpusHoverUI::ViewSize(PC);FVector2D At=FVector2D(X,Y);int VX=0,VY=0;PC.GetViewportSize(VX,VY);if(VX>0)At=FVector2D(X*Size.X/VX,Y*Size.Y/VY);UOpusHoverUI::Place(Ghost,At+FVector2D(10,10),1);}
  if(!Down&&Held){
   if(Dragging){DropAt(P);HoverAfterDrop=true;}
   else if(!Panning&&Inside(Press,UOpusMapUI::Bounds(Screen,"atlas-viewport"))&&UOpusMapUI::Hit(Screen,PC)==nullptr){auto Id=PickRegion(P);if(!Id.IsEmpty()&&Atlas.Select(Id)){Save();DrawAtlas();}}
   CancelDrag();
  }
  if(!Down&&!Dragging&&!Panning){auto Id=PickRegion(P);if(Id!=HoverRegion){HoverRegion=Id;DrawAtlas();}}
  Held=Down;Last=P;
 }
 bool DropAt(FVector2D P)
 {
  for(int I=0;I<6;I++)if(Inside(P,UOpusMapUI::Bounds(Screen,"map-device-slot:"+SlotId(I))))return PlaceItem(DragId,SlotId(I),DragSlot);
  if(!DragSlot.IsEmpty()&&Inside(P,UOpusMapUI::Bounds(Screen,"map-device-backpack")))return RemoveItem(DragSlot);return false;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Hud==nullptr||Atlas==nullptr||!Ready)return;
  if(!Atlas.Active.IsEmpty()&&!Hud.Tower.Traveling){if(Hud.Tower.Run.Phase==EDWOTowerPhase::Cleared){Atlas.Complete();Save();if(Visible)DrawAtlas();}else if(Hud.Tower.Run.Phase==EDWOTowerPhase::Failed){Atlas.Fail();Save();if(Visible)DrawAtlas();}}
  if(!Visible||Screen==nullptr)return;PointerFrame();FitIn-=Dt;if(FitIn<=0){FitIn=.5;UOpusMapUI::Fit(Screen,Gameplay::GetPlayerController(0));Details();PlacePlayerMarker();}
  Poll-=Dt;if(Poll>0)return;Poll=.15;FString Signature;for(auto E:UOpusData::Inventory(Hud.Tower.Rules))Signature+=E.Id+E.Count+E.Rarity+E.Equipped;for(auto S:Hud.Tower.MapItems.State.DeviceSlots)Signature+=S;
  if(Signature!=InventorySignature&&!Dragging){InventorySignature=Signature;Refresh();}
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason)
 {if(PlayerMarker!=nullptr)PlayerMarker.RemoveFromParent();PlayerMarker=nullptr;CancelDrag();if(Events!=nullptr){Events.Entered.UnbindObject(this);Events.Left.UnbindObject(this);}if(Screen!=nullptr){Screen.OnSourceAction.UnbindObject(this);Screen.RemoveFromParent();}Screen=nullptr;}
}
