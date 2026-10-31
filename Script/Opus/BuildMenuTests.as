// Exercise saved UICreator inputs against native preset application, in transient profiles only.
class UDWOBuildMenuTests : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 int Stage=0,Frames=0;bool Done=false;FString Before;
 TArray<FString> BeforeEquipment;
 void Initialize(UDWOSvgHUD H){Hud=H;}
 UButton Input(UWidget W){auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto R=Input(C);if(R!=nullptr)return R;}return nullptr;}
 bool Check(bool Ok,FString What){if(!Ok){Done=true;UOpusNative::WriteLog("BUILD_MENU_FAIL "+What);UOpusNative::Quit(false);}return Ok;}
 bool Click(UOpusWorldScreen Row,FString Binding){if(Row==nullptr)return Check(false,"missing row "+Binding);auto N=Row.FindSourceNode(Binding);if(N==nullptr)return Check(false,"missing node "+Binding);auto B=Input(N);if(!Check(B!=nullptr,"native input "+Binding))return false;B.OnClicked.Broadcast();return true;}
 UOpusWorldScreen FamilyRow(FString Id){for(auto R:Hud.BuildMenu.Rows){auto N=R.FindSourceNode("bd-family-row");if(N!=nullptr&&UOpusUI::Action(N).Value==Id)return R;}return nullptr;}
 UOpusWorldScreen GradeRow(FString Id){for(auto R:Hud.BuildMenu.Rows){auto N=R.FindSourceNode("bd-apply");if(N!=nullptr&&UOpusUI::Action(N).Value==Id)return R;}return nullptr;}
 void Capture(FString Name){FString Dir=UOpusNative::StringOption("DWOBuildMenuProof");if(!Dir.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Dir+"/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||++Frames<30)return;Frames=0;auto M=Hud.BuildMenu;
  if(Stage==0)
  {
   if(!Check(M!=nullptr&&M.Catalogue.Num()==27&&M.FamilyIds.Num()==23,"same 27 presets and 23 families as Studio3D"))return;
   Before=Hud.Tower.Rules.ExportPlayerState();for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Kind=="equipment")BeforeEquipment.Add(E.Id);
   auto Entry=Hud.Screen.FindSourceNode("hud-screen-instance::rs-rail-instance::rs-function-bd::rs-function-button");if(!Check(Input(Entry)!=nullptr,"actual right-rail BD input"))return;Input(Entry).OnClicked.Broadcast();
   if(!Check(M.Visible&&M.Panel!=nullptr&&M.Rows.Num()==23&&Hud.Screen.CurrentState!="builds","BD opens Studio3D family popup rather than legacy six-family modal"))return;
   if(!Check(M.Backdrop!=nullptr,"original CSS backdrop blur has a native receiver"))return;
  }
  else if(Stage==1){if(!Check(Hud.Tower.Rules.ExportPlayerState()==Before,"browsing does not change profile"))return;auto R=UOpusHoverUI::AnchorRect(M.Backdrop,Gameplay::GetPlayerController(0));UOpusNative::WriteLog("BUILD_MENU_BACKDROP width="+(R.Z-R.X)+" height="+(R.W-R.Y)+" strength="+DWOBuildCatalogue::BackdropBlur());if(!Check(R.Z-R.X>299&&R.W-R.Y>505,"background blur has the full authored 300x506 geometry"))return;System::ExecuteConsoleCommand("Slate.AllowBackgroundBlurWidgets");System::ExecuteConsoleCommand("Slate.ForceBackgroundBlurLowQualityOverride");Capture("families");}
  else if(Stage==2)
  {if(!Click(FamilyRow("embers_echo"),"bd-family-row"))return;if(!Check(M.Rows.Num()==6&&GradeRow("embers_echo_1")!=nullptr&&GradeRow("embers_echo_5")!=nullptr,"all five grades shown without three-card pagination"))return;}
  else if(Stage==3){if(!Click(GradeRow("embers_echo_1"),"bd-details"))return;if(!Check(M.Expanded=="embers_echo_1"&&M.Rows.Num()==7,"inline equipment details expand"))return;Capture("grades-details");}
  else if(Stage==4)
  {
   if(!Click(GradeRow("embers_echo_1"),"bd-apply"))return;
   if(!Check(M.Visible&&Hud.Tower.LastError.IsEmpty(),"successful apply keeps quick menu visible"))return;
   bool Found=false;for(auto E:UOpusData::Inventory(Hud.Tower.Rules))if(E.Equipped&&E.Id=="legendary_embers_echo_v5_1_weapon_bdt1")Found=true;
   if(!Check(Found,"first-grade native equipment actually applied"))return;
  }
  else if(Stage==5){if(!Click(GradeRow("embers_echo_5"),"bd-apply"))return;if(!Check(Hud.Tower.LastError.IsEmpty()&&M.Equipped(M.Catalogue[M.Find("embers_echo_5")],UOpusData::Inventory(Hud.Tower.Rules)),"fifth grade applies and is marked equipped"))return;Capture("applied-fifth");}
  else if(Stage==6)
  {
   auto Bag=UOpusData::Inventory(Hud.Tower.Rules);for(auto Id:BeforeEquipment){bool Found=false;for(auto E:Bag)if(E.Kind=="equipment"&&E.Id==Id)Found=true;if(!Check(Found,"existing item retained: "+Id))return;}
   if(!Click(M.Rows[0],"bd-back"))return;if(!Check(M.Rows.Num()==23&&M.Family.IsEmpty(),"back returns all 23 families"))return;
   FString Last=M.FamilyIds.Last();if(!Click(FamilyRow(Last),"bd-family-row"))return;if(!Check(M.Family==Last,"newest final family reachable"))return;
  }
  else if(Stage==7){if(!Check(M.FilterKey(EKeys::Escape)&&!M.Visible,"Escape closes quick menu"))return;Done=true;UOpusNative::WriteLog("BUILD_MENU_PASS families=23 presets=27 grades=5 details=true nativeApply=true existingItemsRetained=true");UOpusNative::Quit(true);return;}
  Stage++;
 }
}
