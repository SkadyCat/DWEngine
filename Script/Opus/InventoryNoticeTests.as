// Real shop acquisition and native hover route; runs only with DWOTransient.
class UDWOInventoryNoticeTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 double Clock=0;bool Done=false,Prepared=false;
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 bool Check(bool Good,FString What){if(!Good){UOpusNative::WriteLog("INVENTORY_NOTICE_FAIL "+What);UOpusNative::Quit(false);}return Good;}
 UOpusActionNode Item(UWidget W,FString Id){if(W==nullptr||!W.IsVisible())return nullptr;auto N=Cast<UOpusActionNode>(W);if(N!=nullptr&&N.HoverId==Id)return N;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto R=Item(C,Id);if(R!=nullptr)return R;}return nullptr;}
 USvgCanvasMotionNode Edge(UWidget W){auto N=Cast<USvgCanvasMotionNode>(W);if(N!=nullptr&&N.Binding=="row:new")return N;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto R=Edge(C);if(R!=nullptr)return R;}return nullptr;}
 UButton Input(UWidget W){auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto R=Input(C);if(R!=nullptr)return R;}return nullptr;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;if(Done||Clock<1||Tower==nullptr||Tower.Hud.Screen==nullptr)return;
  auto H=Tower.Hud;FString Error;
  if(!Prepared)
  {
   H.SetComponentTickEnabled(false);H.Screen.ApplySceneState("inventory");
   H.Execute("buy","item:chaos_orb",Error);if(!Check(Error.IsEmpty(),"real item purchase "+Error))return;
   H.Execute("buy","equipment:frostthorn_staff",Error);if(!Check(Error.IsEmpty(),"real equipment purchase "+Error))return;
   H.RefreshDataPart(0);H.RefreshDataPart(1);Prepared=true;
  }
  if(UOpusNative::Flag("DWOInventoryNoticeVisual")&&Tower.CaptureClock<6)return;
  Done=true;
  FString Gear;
  for(auto E:H.Bag)if(E.Kind=="equipment"&&H.NewItems.IsNew(E.Id)){Gear=E.Id;break;}
  if(!Check(!Gear.IsEmpty()&&H.NewItems.IsNew("chaos_orb"),"acquisitions generate notices"))return;
  auto Row=Item(H.Screen.GetRootWidget(),Gear);auto Layer=Edge(Row);auto Button=Input(Row);
  if(!Check(Layer!=nullptr&&Layer.MotionEnabled&&Button!=nullptr&&Layer.MotionAsset!=nullptr&&Layer.MotionAsset.SourceAssetId=="motion-edge-orbit","saved shared asset drives actual inventory row"))return;
  auto Asset=Layer.MotionAsset;Button.OnHovered.Broadcast();
  if(!Check(!H.NewItems.IsNew(Gear)&&!Layer.MotionEnabled,"actual hover consumes UI notice and disables layer"))return;
  Button.OnUnhovered.Broadcast();H.RefreshDataPart(1);
  if(!Check(!Layer.MotionEnabled&&Layer.MotionAsset==Asset&&H.NewItems.IsNew("chaos_orb"),"mouse leave does not re-arm or consume another item"))return;
  H.Screen.ApplySceneState("character");H.RefreshDataPart(1);Row=Item(H.Screen.GetRootWidget(),Gear);Layer=Edge(Row);
  if(!Check(Layer!=nullptr&&!Layer.MotionEnabled,"character backpack shares seen state"))return;
  H.NewItems.MarkSeen("chaos_orb");H.Execute("buy","item:chaos_orb",Error);H.RefreshDataPart(0);H.RefreshDataPart(1);
  if(!Check(Error.IsEmpty()&&H.NewItems.IsNew("chaos_orb"),"stack increase re-arms notification"))return;
  UOpusNative::WriteLog("INVENTORY_NOTICE_PASS Acquisition SharedAsset HoverConsumed Character StackIncrease");UOpusNative::Quit(true);
 }
}
