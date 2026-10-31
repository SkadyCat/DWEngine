// Click the actual saved right-rail inputs; opening panels must not apply a BD.
class UDWORightRailTest : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 int Stage=0,Frames=0;bool Done=false;FString Before;
 void Initialize(UDWOSvgHUD H){Hud=H;}
 USvgCanvasNode Entry(FString Id){return Hud.Screen.FindSourceNode("hud-screen-instance::rs-rail-instance::rs-function-"+Id+"::rs-function-button");}
 UButton Input(USvgCanvasNode N){if(N!=nullptr)for(auto W:N.GetAllChildren()){auto B=Cast<UButton>(W);if(B!=nullptr)return B;}return nullptr;}
 bool Check(bool Good,FString What){if(!Good){Done=true;UOpusNative::WriteLog("RIGHT_RAIL_FAIL "+What);UOpusNative::Quit(false);}return Good;}
 void Capture(FString Name){FString Dir=UOpusNative::StringOption("DWORightRailProof");if(!Dir.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Dir+"/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||++Frames<24)return;Frames=0;
  if(Stage==0)
  {
   auto BD=Entry("bd");auto Talent=Entry("talents");
   if(!Check(BD!=nullptr&&Talent!=nullptr&&Input(BD)!=nullptr&&Input(Talent)!=nullptr,"both independent native inputs exist"))return;
   if(!Check(BD.IsVisible()&&Talent.IsVisible(),"both entries are visible on the right rail"))return;
   Before=Hud.Tower.Rules.ExportPlayerState();Capture("hud");
  }
  else if(Stage==1){Input(Entry("bd")).OnClicked.Broadcast();if(!Check(Hud.BuildMenu.Visible&&Hud.BuildMenu.Rows.Num()==23,"BD button opens the Studio3D build list"))return;}
  else if(Stage==2)Capture("builds");
  else if(Stage==3){Hud.BuildMenu.Close();Hud.Screen.ApplySceneState("selected-1");Input(Entry("talents")).OnClicked.Broadcast();if(!Check(Hud.Talents.Visible&&Hud.Talents.Screen!=nullptr&&Hud.Talents.Nodes.Num()==1000,"talent button opens the full live constellation"))return;}
  else if(Stage==4)Capture("talents");
  else if(Stage==5)
  {
   Hud.Talents.Close();if(!Check(Hud.Tower.Rules.ExportPlayerState()==Before,"navigation does not change the equipped build"))return;
   Input(Entry("bd")).OnClicked.Broadcast();
  }
  else if(Stage==6)
  {
   USvgCanvasNode Family;for(auto Row:Hud.BuildMenu.Rows){auto N=Row.FindSourceNode("bd-family-row");if(N!=nullptr&&UOpusUI::Action(N).Value=="embers_echo")Family=N;}
   if(!Check(Input(Family)!=nullptr,"actual build-family input exists"))return;Input(Family).OnClicked.Broadcast();
   if(!Check(Hud.BuildMenu.Family=="embers_echo"&&Hud.BuildMenu.Rows.Num()==6,"all five Embers grades are available"))return;
  }
  else if(Stage==7)
  {
   USvgCanvasNode Grade;for(auto Row:Hud.BuildMenu.Rows){auto N=Row.FindSourceNode("bd-apply");if(N!=nullptr&&UOpusUI::Action(N).Value=="embers_echo_1")Grade=N;}
   if(!Check(Input(Grade)!=nullptr,"actual quick-apply input exists"))return;Input(Grade).OnClicked.Broadcast();
   if(!Check(Hud.BuildMenu.Visible&&Hud.Tower.LastError.IsEmpty(),"quick apply succeeds and retains the Studio3D menu"))return;
  }
  else if(Stage==8)
  {
   FString Applied=Hud.Tower.Rules.ExportPlayerState();int Equipped=0;for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Equipped&&E.Kind=="equipment")Equipped++;
   if(!Check(Applied!=Before&&Applied.Contains("legendary_embers_echo_v5_1_weapon_bdt1")&&Equipped==5&&UOpusData::SkillIcon(Hud.Tower.Rules,0).Contains("flame_burst"),"native equipment and skill match the applied build"))return;
   Hud.BuildMenu.Close();Hud.Screen.ApplySceneState("character");Hud.RefreshData();Capture("applied");
  }
  else if(Stage==9)
  {
   Done=true;UOpusNative::WriteLog("RIGHT_RAIL_PASS BDAndTalents preset=embers_echo_1 equipped=5");UOpusNative::Quit(true);return;
  }
  Stage++;
 }
}
