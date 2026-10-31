// Real equipment, saved UICreator motion and aura toggles, transient profile only.
class UDWOAuraActivationTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 double Clock=0,Age=0;int Stage=0;bool Done=false;
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 bool Check(bool Good,FString Detail)
 {UOpusNative::WriteLog((Good?"AURA_FLOW_PASS ":"AURA_FLOW_FAIL ")+Detail);if(!Good){Done=true;UOpusNative::Quit(false);}return Good;}
 USvgCanvasMotionNode Edge(int Slot=0){return Cast<USvgCanvasMotionNode>(Tower.Recent.UI.Slots[Slot].FindSourceNode("aura-slot-active"));}
 UButton Input(UWidget W){auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Input(C);if(Found!=nullptr)return Found;}return nullptr;}
 void Next(int N){Stage=N;Age=0;}
 void Shot(FString Name){System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/AuraActivation/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Clock+=Dt;Age+=Dt;
  if(Clock>100){Check(false,"timeout stage="+Stage);return;}
  auto T=Tower;auto G=T.Recent;auto R=T.Rules;
  if(G==nullptr||G.UI==nullptr||!G.UI.Started||!T.Dungeon.Ready||Age<.2)return;
  if(Stage==0)
  {
   T.Hero.ControlProbe=true;FString Error;TArray<FString> Old;
   for(auto E:UOpusData::Inventory(R))Old.Add(E.Id);
   if(!Check(R.BuyPlayerListing("equipment:wind_breath_ring",Error),"buy aura ring "+Error))return;
   bool Equipped=false;
   for(auto E:UOpusData::Inventory(R))if(E.Kind=="equipment"&&!Old.Contains(E.Id))
   {FString Uid=E.Id;if(!E.Identified&&!Check(R.IdentifyPlayerEquipment(FName(E.Id),Uid,Error),Error))return;Equipped=R.EquipPlayerItem(n"ring",FName(Uid),Error);break;}
   if(!Check(Equipped,"equip real aura ring "+Error))return;
   G.SyncAuras();if(!Check(G.EquipAura(0,n"wind_breath")&&!G.Enabled[0],"assign without activation"))return;Next(1);return;
  }
  if(Stage==1)
  {
   auto N=Edge();
   if(!Check(N!=nullptr&&N.MotionAsset!=nullptr&&N.MotionAsset.SourceAssetId=="motion-aura-active"&&!N.MotionEnabled,"saved motion reference reloads and defaults off"))return;
   for(int I=1;I<4;I++)if(!Check(Edge(I)!=nullptr&&!Edge(I).MotionEnabled,"empty slot "+I+" has no flow"))return;
   if(Age<.6)return;
   auto Rect=UOpusHoverUI::AnchorRect(G.UI.Slots[0],Gameplay::GetPlayerController(0));
   UOpusNative::WriteLog("AURA_FLOW_RECT "+Rect.X+","+Rect.Y+","+Rect.Z+","+Rect.W);
   Shot("inactive");Next(11);return;
  }
  if(Stage==11){if(Age<.3)return;if(!Check(G.Toggle(0)&&G.Enabled[0]&&UOpusRecentGameplay::ManaReservation(R)==50,"activation keeps existing reservation"))return;Next(2);return;}
  if(Stage==2)
  {
   if(Age<.7)return;
   if(!Check(Edge().MotionEnabled,"native edge motion enabled on active aura"))return;
   Shot("active-a");Next(3);return;
  }
  if(Stage==3)
  {
   if(Age<.55)return;Shot("active-b");
   auto N=G.UI.Slots[0].FindSourceNode("hotbar-quick-slot");
   auto B=Input(N);
   if(!Check(B!=nullptr,"actual slot input retained"))return;
   B.OnHovered.Broadcast();if(!Check(Edge().MotionEnabled,"hover does not dismiss active flow"))return;B.OnUnhovered.Broadcast();Next(4);return;
  }
  if(Stage==4){if(Age<.3)return;G.Toggle(0);Next(5);return;}
  if(Stage==5){if(!Check(!G.Enabled[0]&&!Edge().MotionEnabled&&UOpusRecentGameplay::ManaReservation(R)==0,"deactivation stops flow and releases reservation"))return;Shot("deactivated");Next(6);return;}
  if(Stage==6){if(Age<.3)return;G.Toggle(0);Next(7);return;}
  if(Stage==7){if(!Check(Edge().MotionEnabled,"reactivation restarts flow"))return;G.EquipAura(0,FName());Next(8);return;}
  if(Stage==8)
  {
   if(!Check(G.AuraSlots[0].IsNone()&&!Edge().MotionEnabled&&G.UI.Slots[0].IsVisible(),"clearing stops flow and restores the visible empty slot"))return;
   Done=true;UOpusNative::WriteLog("AURA_FLOW_DONE");UOpusNative::Quit(true);
  }
 }
}
