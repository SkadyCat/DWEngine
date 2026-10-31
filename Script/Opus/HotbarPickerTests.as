class UDWOHotbarPickerTests : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 double Clock=0,Age=0;
 int Stage=0;
 bool Done=false;
 FString SkillId,SkillName;
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 bool Check(bool Ok,FString Detail){UOpusNative::WriteLog((Ok?"HOTBAR_PASS ":"HOTBAR_FAIL ")+Detail);if(!Ok){Done=true;UOpusNative::Quit(false);}return Ok;}
 UButton Input(UWidget W){auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Input(C);if(Found!=nullptr)return Found;}return nullptr;}
 bool Click(UWidget W,FString Detail){auto B=Input(W);if(!Check(B!=nullptr,Detail+" has a real native input"))return false;B.OnClicked.Broadcast();return true;}
 bool Equip(FString Base,FName Slot)
 {
  auto R=Tower.Rules;FString Error;TArray<FString> Old;for(auto E:UOpusData::Inventory(R))Old.Add(E.Id);
  if(!Check(R.BuyPlayerListing("equipment:"+Base,Error),"buy real aura equipment "+Error))return false;
  for(auto E:UOpusData::Inventory(R))if(E.Kind=="equipment"&&!Old.Contains(E.Id)){FString Uid=E.Id;if(!E.Identified&&!Check(R.IdentifyPlayerEquipment(FName(E.Id),Uid,Error),Error))return false;return Check(R.EquipPlayerItem(Slot,FName(Uid),Error),"equip real aura source "+Error);}return Check(false,"new aura source not found");
 }
 void Next(int Value){Stage=Value;Age=0;}
 void Shot(FString Name){if(UOpusNative::Flag("DWOHotbarRender"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/HotbarPickerUE/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Clock+=Dt;Age+=Dt;if(Clock>120){Check(false,"timeout stage="+Stage);return;}
  auto H=Tower.Hud.Hotbar;auto G=Tower.Recent;auto R=Tower.Rules;
  if(H==nullptr||G==nullptr||!G.Ready||!Tower.Dungeon.Ready||H.QuickSlots.Num()!=4||Age<.25)return;
  if(Stage==0)
  {
   Tower.Hero.ControlProbe=true;Tower.SetPaused(false);
   for(auto W:H.QuickSlots)if(!Check(W!=nullptr,"saved quick-slot template loads"))return;
   if(!Click(H.Anchor(false,2),"main HUD numbered slot"))return;
   if(!Check(H.Panel!=nullptr&&H.Slot==2&&H.Mode=="skill"&&H.Panel.TemplateSize.X==294&&!Tower.Paused,"skill click opens compact palette without pausing"))return;
   if(!Check(H.Choices.Num()>0,"skill choices come from owned and equipped sources"))return;
   SkillId=H.Choices[0].Id;SkillName=H.Choices[0].Name;
   if(!Click(H.Panel.FindSourceNode("r"+H.RowCount+"-cell-0"),"skill choice"))return;
   if(!Check(H.Panel==nullptr&&UOpusData::SkillName(R,2)==SkillName,"real icon click assigns selected numbered slot and closes"))return;
   if(!Check(H.Open("skill",2),"reopen assigned skill"))return;Next(10);return;
  }
  if(Stage==10)
  {
   if(UOpusNative::Flag("DWOHotbarRender")&&Age<1)return;
   auto Root=H.Panel.FindSourceNode("hotbar-picker-"+H.RowCount);
   if(!Check(Root!=nullptr&&Root.Normal.Texture!=nullptr,"palette background reloads from saved UICreator assets"))return;
   if(!Check(H.FilterKey(EKeys::Right)&&H.FocusedChoice==1%H.Choices.Num()&&H.FilterKey(EKeys::Home)&&H.FocusedChoice==0,"arrow keys navigate real icon inputs"))return;
   Shot("skill-palette");Next(11);return;
  }
  if(Stage==11)
  {
   auto B=Input(H.Panel.FindSourceNode("r"+H.RowCount+"-cell-0"));B.OnHovered.Broadcast();
   if(!Check(H.Tip!=nullptr,"native hover opens UICreator skill tooltip"))return;
   B.OnUnhovered.Broadcast();if(!Check(H.Tip==nullptr,"native unhover closes tooltip"))return;
   Next(1);return;
  }
  if(Stage==1)
  {
   if(!Click(H.Panel.FindSourceNode("r"+H.RowCount+"-clear"),"clear skill"))return;
   if(!Check(H.Panel==nullptr&&UOpusData::SkillName(R,2)=="空槽","clear removes only slot assignment"))return;
   if(!Equip("wind_breath_ring",n"ring"))return;
   G.SyncAuras();H.RefreshQuick();
   if(!Check(H.Open("quick",0)&&G.UI.Panel==nullptr&&!Tower.Paused,"Q opens shared compact aura and consumable palette"))return;
   int At=-1;for(int I=0;I<H.Choices.Num();I++)if(H.Choices[I].Id=="wind_breath")At=I;
   if(!Check(At>=0,"real granted aura is available"))return;
   if(!Click(H.Panel.FindSourceNode("r"+H.RowCount+"-cell-"+At),"aura choice"))return;
   if(!Check(G.AuraSlots[0]==n"wind_breath"&&!G.Enabled[0]&&H.Panel==nullptr,"assigning Q does not enable or spend mana"))return;
   if(!Check(G.Toggle(0)&&G.Enabled[0]&&UOpusRecentGameplay::ManaReservation(R)==50,"existing activation reserves real mana"))return;
   Next(2);return;
  }
  if(Stage==2)
  {
   auto Edge=Cast<USvgCanvasMotionNode>(H.QuickSlots[0].FindSourceNode("aura-slot-active"));
   if(!Check(Edge!=nullptr&&Edge.MotionAsset!=nullptr&&Edge.MotionEnabled,"enabled aura uses saved UICreator flow"))return;
   double Mana=R.GetPlayerPools().Mana;
   if(!Click(H.QuickSlots[0].FindSourceNode("hotbar-quick-slot"),"equipped Q"))return;
   if(!Check(H.Panel!=nullptr&&G.Enabled[0]&&R.GetPlayerPools().Mana==Mana,"clicking enabled Q configures without toggling"))return;
   Next(21);return;
  }
  if(Stage==21)
  {
   if(UOpusNative::Flag("DWOHotbarRender")&&Age<.6)return;
   Shot("aura-palette");Next(3);return;
  }
  if(Stage==3)
  {
   if(!Check(H.Choose("wind_breath")&&G.Enabled[0]&&UOpusRecentGameplay::ManaReservation(R)==50,"reselecting assigned aura preserves enabled state"))return;
   if(!Check(H.Open("quick",1)&&H.Choose("wind_breath")&&G.AuraSlots[0].IsNone()&&G.AuraSlots[1]==n"wind_breath"&&!G.Enabled[1],"aura moves between quick slots without duplication"))return;
   if(!Check(H.Open("quick",1)&&H.FilterKey(EKeys::Escape)&&H.Panel==nullptr&&H.HandleKeys(Gameplay::GetPlayerController(0))&&!Tower.Paused,"Escape closes palette and consumes shortcut without pausing"))return;
   FString Error;if(!Check(R.BuyPlayerListing("item:healing_draught",Error),"buy consumable "+Error))return;
   if(!Check(H.Open("quick",2),"open consumable palette"))return;
   bool Potion=false;for(auto E:H.Choices)if(E.Id=="healing_draught")Potion=true;
   if(!Check(Potion,"owned healing potion is available in palette"))return;
   bool Assigned=H.Choose("healing_draught");
   if(!Check(Assigned&&UOpusNative::Belt(R,2)==n"healing_draught"&&G.AuraSlots[2].IsNone(),"T accepts an owned consumable in same palette; success="+Assigned+" belt="+UOpusNative::Belt(R,2)+" aura="+G.AuraSlots[2]+" error="+Tower.LastError))return;
   H.RefreshQuick();Shot("quick-slots");Next(4);return;
  }
  if(Stage==4)
  {
   if(!Check(H.Open("quick",2)&&H.FilterKey(EKeys::T)&&H.HandleKeys(Gameplay::GetPlayerController(0))&&UOpusNative::Belt(R,2)==n"healing_draught","quick shortcut closes palette without consuming item"))return;
   if(!Check(H.Open("quick",2)&&H.Choose("")&&UOpusNative::Belt(R,2).IsNone(),"clear removes consumable assignment"))return;
   G.Toggle(1);FString Error;if(!Check(R.EquipPlayerItem(n"ring",FName(),Error),"unequip aura source "+Error))return;G.SyncAuras();H.RefreshQuick();
   if(!Check(G.AuraSlots[1].IsNone()&&!G.Enabled[1]&&UOpusRecentGameplay::ManaReservation(R)==0,"unequipping source revokes aura and reservation"))return;
   if(!Check(H.Open("skill",0)&&H.FilterKey(EKeys::Four)&&H.Panel==nullptr&&UOpusNative::Selected(R)==3,"number shortcut selects active slot and dismisses palette"))return;
   for(int I=1;I<=4;I++){auto P=H.Create("hotbar-picker-"+I);if(!Check(P!=nullptr,"saved palette row variant "+I+" reloads"))return;P.RemoveFromParent();}
   Done=true;UOpusNative::WriteLog("HOTBAR_DONE");UOpusNative::Quit(true);
  }
 }
}
