// Exercise the real saved HUD inputs and UICreator rows without touching the profile.
class UDWOAuraClickTest : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 double Clock=0,Age=0;int Stage=0;bool Done=false;
 void Initialize(UDWOSvgHUD H){Hud=H;}
 bool Check(bool Good,FString Detail)
 {UOpusNative::WriteLog((Good?"AURA_CLICK_PASS ":"AURA_CLICK_FAIL ")+Detail);if(!Good){Done=true;UOpusNative::Quit(false);}return Good;}
 UButton Input(UWidget W)
 {auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Input(C);if(Found!=nullptr)return Found;}return nullptr;}
 bool Click(UWidget W,FString Detail)
 {auto B=Input(W);if(!Check(B!=nullptr,Detail+" input"))return false;B.OnClicked.Broadcast();return true;}
 bool Equip(FName Base,FName Slot)
 {
  auto R=Hud.Tower.Rules;FString Error;TArray<FString> Old;
  for(auto E:UOpusData::Inventory(R))Old.Add(E.Id);
  if(!Check(R.BuyPlayerListing("equipment:"+Base,Error),"buy "+Base+" "+Error))return false;
  for(auto E:UOpusData::Inventory(R))if(E.Kind=="equipment"&&!Old.Contains(E.Id))
  {FString Uid=E.Id;if(!E.Identified&&!Check(R.IdentifyPlayerEquipment(FName(E.Id),Uid,Error),Error))return false;return Check(R.EquipPlayerItem(Slot,FName(Uid),Error),"equip "+Base+" "+Error);}
  return Check(false,"new equipment missing");
 }
 void Shot(FString Name)
 {if(UOpusNative::Flag("DWOAuraClickVisual"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/AuraClick/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Hud==nullptr||Hud.Tower==nullptr)return;Clock+=Dt;Age+=Dt;
  if(Clock>90){Check(false,"timeout stage="+Stage);return;}
  auto T=Hud.Tower;auto G=T.Recent;auto R=T.Rules;
  if(G==nullptr||G.UI==nullptr||!G.UI.Started||!T.Dungeon.Ready||Age<.5)return;
  if(Stage==0)
  {
   T.Hero.ControlProbe=true;
   if(!Equip(n"wind_breath_ring",n"ring")||!Equip(n"aura_fire_amulet",n"amulet"))return;
   G.SyncAuras();bool Wind=false,Fire=false;for(auto C:G.Choices){if(C.Id==n"wind_breath")Wind=true;if(C.Id==n"aura_fire")Fire=true;}
   if(!Check(Wind&&Fire,"all equipped auras available"))return;
   // Main HUD Q input, with no aura overlay present.
   auto Q=Hud.Screen.FindSourceNode("hud-screen-instance::rs-hotbar-instance::rs-consumables::hud-quick-instance-Q::hud-quick-button");
   if(!Click(Q,"empty Q"))return;
   if(!Check(Hud.Hotbar.Panel!=nullptr&&Hud.Hotbar.Mode=="quick"&&Hud.Hotbar.Slot==0&&!T.Paused,"empty Q opens compact configurable aura palette"))return;
   Stage=1;Age=0;return;
  }
  if(Stage==1)
  {
   Shot("empty-q-choices");
   Stage=11;Age=0;return;
  }
  if(Stage==11)
  {
   USvgCanvasNode Choice;for(int I=0;I<Hud.Hotbar.Choices.Num();I++)if(Hud.Hotbar.Choices[I].Id=="wind_breath")Choice=Hud.Hotbar.Panel.FindSourceNode("r"+Hud.Hotbar.RowCount+"-cell-"+I);
   if(!Click(Choice,"wind aura choice"))return;
   if(!Check(G.AuraSlots[0]==n"wind_breath"&&!G.Enabled[0],"click row assigns Q without enabling"))return;
   Hud.Hotbar.Close();Stage=2;Age=0;return;
  }
  if(Stage==2)
  {
   if(!Check(G.UI.Slots.Num()==4&&G.UI.Slots[0].IsVisible(),"equipped Q overlay is visible"))return;
   double Mana=R.GetPlayerPools().Mana;
   if(!Click(G.UI.Slots[0].FindSourceNode("hotbar-quick-slot"),"equipped Q"))return;
   if(!Check(Hud.Hotbar.Panel!=nullptr&&Hud.Hotbar.Slot==0&&!G.Enabled[0]&&R.GetPlayerPools().Mana==Mana,"equipped Q reopens palette without toggling or spending mana"))return;
   Stage=3;Age=0;return;
  }
  if(Stage==3)
  {
   Shot("equipped-q-choices");Stage=13;Age=0;return;
  }
  if(Stage==13)
  {
   Hud.Hotbar.Close();
   if(!Check(G.Toggle(0)&&G.Enabled[0]&&UOpusRecentGameplay::ManaReservation(R)==50,"existing keyboard toggle path retains reservation"))return;
   G.Toggle(0);Hud.Screen.ApplySceneState("picker-1");
   auto E=Hud.Screen.FindSourceNode("hud-screen-instance::rs-hotbar-instance::rs-consumables::hud-quick-instance-E::hud-quick-button");
   if(!Click(E,"empty E over skill picker"))return;
   if(!Check(Hud.Hotbar.Panel!=nullptr&&Hud.Hotbar.Slot==1&&Hud.Screen.CurrentState.StartsWith("selected-"),"E selects its own aura slot and closes skill picker"))return;
   Hud.Hotbar.Close();FString Error;R.EquipPlayerItem(n"ring",FName(),Error);G.SyncAuras();
   if(!Check(G.AuraSlots[0].IsNone()&&!G.Enabled[0],"unequip still revokes assigned aura"))return;
   UOpusNative::WriteLog("AURA_CLICK_DONE");Done=true;UOpusNative::Quit(true);
  }
 }
}
