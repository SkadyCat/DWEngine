// Transient tests exercise the actual AS controller and saved UICreator nodes.
class UDWOMapDeviceTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 float Clock=0,PreviewClock=0;bool Done=false;int Step=0,PreviewFrames=0;
 FString Rare,Before;
 bool Check(bool Ok,FString What){if(!Ok){Done=true;UOpusNative::WriteLog("MAP_DEVICE_FAIL "+What);UOpusNative::Quit(false);}return Ok;}
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;if(Done){if(UOpusNative::Flag("DWOMapDevicePreview")){PreviewClock+=Dt;if(PreviewFrames<2&&PreviewClock>=(PreviewFrames==0?.7f:1.25f)){System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/MapEntry/native-active-"+PreviewFrames+".png -nosuffix");PreviewFrames++;}}return;}if(Clock<1||Tower.Hud.Screen==nullptr)return;auto D=Tower.Hud.MapDevice;auto M=Tower.MapItems;auto Journey=UDWOTowerJourney::Get();FString Error;
  if(!Check(D!=nullptr&&D.Ready,"map device initialized"))return;
  if(Journey.MapItemTestStage==42||UOpusNative::Flag("DWOMapItemReloadTest")){
   if(!Check(Tower.Run.Floor==16&&Tower.Run.Phase==EDWOTowerPhase::Ready&&M.State.Active.Tier==16,"map entry travels to T16 and waits for manual start"))return;
   if(!Check(M.State.Items.Num()==2&&!M.Owns(M.State.Active.Uid)&&M.State.DeviceSlots[0].IsEmpty(),"only the used map is consumed and its reservation clears"))return;
   if(!Check(D.Atlas.Active=="0:0"&&D.Atlas.Completed.Num()==0,"active region survives level travel without fake completion"))return;
   auto Live=D.Atlas.Nodes[D.Atlas.Index(D.Atlas.Active)];
   if(!Check(Live.Floor==16&&D.RegionTier(Live)==16&&D.TierLabel(Live).Contains("T16"),"admitted T16 survives consumption and reload on the depth-zero region"))return;
   D.Open();if(!Check(D.PlaceItem(M.State.Items[0].Uid,"map")&&D.RegionTier(Live)==16&&DWOHoverTests::VisibleText(D.Screen.GetRootWidget()).Contains("返回战场不消耗地图"),"next map preview cannot alter the live T16 challenge or charge its return"))return;
   if(!Check(D.RemoveItem("map"),"remove next-map reservation after live-tier check"))return;D.Close();
   if(!Check(UOpusNative::Flag("DWOMapItemProfileTest")&&UOpusNative::WriteProfile(M.Snapshot())&&UOpusNative::ReadProfile()==M.Snapshot(),"isolated profile saves and reads back map and region state"))return;
   Done=true;UOpusNative::WriteLog(UOpusNative::Flag("DWOMapItemReloadTest")?"MAP_DEVICE_PASS Reload":"MAP_DEVICE_PASS Integration");UOpusNative::Quit(true);return;
  }
  // Each step follows a separate frame, just like the user's individual inputs.
  if(Step==0){
  auto A=NewObject(this,UDWOAtlasState);if(!Check(A.Restore(""),"new atlas"))return;int Count=0,Edges=0;for(auto N:A.Nodes)if(N.Status!="hidden"){Count++;for(auto B:N.Neighbors)if(N.Id<B&&A.Index(B)>=0&&A.Nodes[A.Index(B)].Status!="hidden")Edges++;}
  auto Origin=A.Nodes[A.Index("0:0")];
  if(!Check(Count==37&&Edges==54&&Origin.Name=="初始界域"&&Origin.Biome=="遗迹"&&Math::Abs(Origin.Position.X+28.43636485747993)<.00001&&Math::Abs(Origin.Position.Y-15.727030132897198)<.00001,"AS atlas matches RuleStudio seed1 geometry, links and visibility"))return;
  if(!Check(A.Enter(),"origin is reachable"))return;A.Complete();auto B=NewObject(this,UDWOAtlasState);if(!Check(B.Restore(A.Serialize())&&B.Completed.Num()==1,"completed atlas roundtrip"))return;
   Step++;return;
  }
  if(Step==1){
  if(!Check(M.Buy("item:map_t01_common",Error)&&M.Buy("item:map_t08_magic",Error)&&M.Buy("item:map_t16_rare",Error),"acquire real rolled maps through existing shop"))return;
  Rare=M.State.Items[2].Uid;Before=M.Snapshot();D.Open();
  auto View=UOpusMapUI::Bounds(D.Screen,"atlas-viewport");
  if(!Check(D.Screen.FindSourceNode("map-device-preview-atlas-plot")!=nullptr&&D.Screen.FindSourceNode("map-device-preview-battle-atlas-node-0")==nullptr&&View.Z==838&&View.W==600,"real engraved atlas surface replaces old card fixture"))return;
  if(!Check(D.PickRegion(FVector2D(View.X+View.Z*.5,View.Y+View.W*.5))=="0:0","round region selection uses actual world coordinates"))return;
  D.Atlas.Select("1:0");D.CenterAtlas();if(!Check(D.PickRegion(FVector2D(View.X+View.Z*.5,View.Y+View.W*.5))=="1:0","centering follows the selected region"))return;
  D.Atlas.Select("0:0");D.CenterAtlas();D.DrawAtlas();
  if(!Check(D.Visible&&D.Screen!=nullptr&&D.Screen.FindSourceNode("map-device-mounted-map")!=nullptr,"saved map WBP contains embedded sockets"))return;
  for(int I=0;I<6;I++)if(!Check(D.Screen.FindSourceNode("map-device-preview-map-device-slot-"+D.SlotId(I))!=nullptr,"saved socket "+I))return;
  if(!Check(DWOHoverTests::Action(D.Screen.GetRootWidget(),"select",Rare)!=nullptr,"real inventory projected into authored backpack"))return;
  auto Enter=Cast<USvgCanvasMotionNode>(D.Screen.FindSourceNode("map-device-preview-battle-atlas-enter"));
  if(!Check(Enter!=nullptr&&Enter.MotionAsset!=nullptr&&Enter.MotionAsset.SourceAssetId=="motion-atlas-entry-ready"&&D.CanEnter()&&DWOHoverTests::Input(Enter).IsEnabled&&!Enter.MotionEnabled,"saved entry motion loads; free origin is enabled without a map activation glow"))return;
  if(!Check(Enter.Normal.Texture!=Enter.Disabled.Texture&&Enter.Hovered.Texture!=Enter.Pressed.Texture,"disabled default retains distinct enabled, hovered and pressed artwork"))return;
  if(!Check(D.Screen.FindSourceNode("map-device-preview-battle-atlas-state")==nullptr&&D.Screen.FindSourceNode("map-device-preview-battle-atlas-neighbors")==nullptr&&D.Screen.FindSourceNode("map-device-preview-atlas-route")==nullptr,"crossed-out facts and region dropdown are removed from saved source"))return;
  D.Atlas.Select("1:0");D.Details();auto Snapshot=M.Snapshot();
  if(!Check(!D.CanEnter()&&!DWOHoverTests::Input(Enter).IsEnabled&&!Enter.MotionEnabled&&!DWOHoverTests::Click(Enter)&&Snapshot==M.Snapshot(),"unconnected region disables native button and cannot consume a ticket"))return;
  D.Atlas.Select("0:0");D.Details();
   Step++;return;
  }
  if(Step==2){
  if(!Check(!D.PlaceItem(Rare,"modifier-0")&&Before==M.Snapshot(),"map rejected by modifier socket without inventory or RNG mutation"))return;
   Step++;return;
  }
  if(Step==3){
  auto Target=UOpusMapUI::Bounds(D.Screen,"map-device-slot:map");D.DragId=Rare;D.DragSlot="";
  if(!Check(D.DropAt(FVector2D(Target.X+Target.Z*.5,Target.Y+Target.W*.5))&&M.Owns(Rare)&&M.State.Items.Num()==3&&D.Reserved(Rare)==1,"drag hit region reserves one map without consuming it"))return;D.CancelDrag();
  for(auto N:D.Atlas.Nodes)if(!Check(D.RegionTier(N)==16,"all region tier previews follow the mounted T16 map"))return;
  if(!Check(!D.PlaceItem(Rare,"map")&&D.Reserved(Rare)==1,"already reserved UID cannot be duplicated"))return;
  auto Enter=Cast<USvgCanvasMotionNode>(D.Screen.FindSourceNode("map-device-preview-battle-atlas-enter"));
  if(!Check(D.CanEnter()&&D.EntryActive()&&Enter.MotionEnabled&&DWOHoverTests::Input(Enter).IsEnabled,"placing an owned T16 map enables the saved flowing entry border"))return;
  DWOHoverTests::Input(Enter).OnHovered.Broadcast();DWOHoverTests::Input(Enter).OnUnhovered.Broadcast();if(!Check(Enter.MotionEnabled,"hover preserves activation flow"))return;
  D.Atlas.Select("1:0");D.Details();if(!Check(!D.CanEnter()&&!Enter.MotionEnabled&&!DWOHoverTests::Input(Enter).IsEnabled,"valid map cannot activate a locked destination"))return;D.Atlas.Select("0:0");D.Details();
  FOpusMapItemState Restored;if(!Check(UOpusMapItems::DecodeProfile(M.Snapshot(),Restored,Error)&&Restored.DeviceSlots[0]==Rare&&Restored.Items.Num()==3,"reservation persists with owned UID"))return;
   Step++;return;
  }
  if(Step==4){
  auto Anchor=DWOHoverTests::Find(D.Screen.GetRootWidget(),Rare);if(!Check(Anchor!=nullptr,"occupied socket exposes actual map hover UID"))return;
  DWOHoverTests::Input(Anchor).OnHovered.Broadcast();auto Hover=Tower.Hud.ItemHover;Hover.Pin();
  if(!Check(Hover.Panel!=nullptr&&Hover.ReadOnly&&Hover.Pinned&&DWOHoverTests::VisibleText(Hover.Panel.GetRootWidget()).Contains("地图 T16")&&!DWOHoverTests::VisibleText(Hover.Panel.GetRootWidget()).Contains("使用地图"),"socket hover uses real map, pins and has no accidental consume action"))return;
   Step++;return;
  }
  if(Step==5){
   auto Hover=Tower.Hud.ItemHover;
  if(!Check(D.FilterKey(EKeys::Escape)&&D.Visible&&Hover.Panel==nullptr,"Escape closes tooltip before atlas"))return;
  auto Target=UOpusMapUI::Bounds(D.Screen,"map-device-backpack");D.DragId=Rare;D.DragSlot="map";
  if(!Check(D.DropAt(FVector2D(Target.X+20,Target.Y+20))&&M.Owns(Rare)&&M.State.Items.Num()==3&&D.Reserved(Rare)==0,"drag back restores availability without creating a duplicate"))return;D.CancelDrag();
  auto Unentered=D.Atlas.Nodes[D.Atlas.Index("1:0")];if(!Check(D.RegionTier(Unentered)==0&&D.TierLabel(Unentered).Contains("地图决定"),"removing the map clears an unentered region tier without a distance fallback"))return;
  auto Enter=Cast<USvgCanvasMotionNode>(D.Screen.FindSourceNode("map-device-preview-battle-atlas-enter"));if(!Check(!D.EntryActive()&&!Enter.MotionEnabled,"taking the map back stops the activation flow"))return;
   Step++;return;
  }
  if(Step==6){
  if(!Check(D.PlaceItem(Rare,"map"),"reassemble for entry"))return;
  if(UOpusNative::Flag("DWOMapDevicePreview")){auto Enter=D.Screen.FindSourceNode("map-device-preview-battle-atlas-enter");auto Rect=UOpusHoverUI::AnchorRect(Enter,Gameplay::GetPlayerController(0));UOpusNative::WriteLog("MAP_ENTRY_FLOW_RECT "+Rect.X+","+Rect.Y+","+Rect.Z+","+Rect.W);Done=true;UOpusNative::WriteLog("MAP_DEVICE_PASS Preview");return;}
  Tower.Run.Phase=EDWOTowerPhase::Waves;Before=M.Snapshot();D.EnterRegion();
  if(!Check(Before==M.Snapshot()&&M.Owns(Rare)&&D.Atlas.Active.IsEmpty(),"enter during combat rejects and rolls back region state"))return;
   Tower.Run.Phase=EDWOTowerPhase::Ready;D.Details();Step++;return;
  }
  Done=true;Journey.MapItemTestStage=42;auto Enter=D.Screen.FindSourceNode("map-device-preview-battle-atlas-enter");
  if(!Check(Enter!=nullptr&&DWOHoverTests::Click(Enter)&&Tower.Traveling,"authored enter button invokes map use and travel"))return;
 }
}
