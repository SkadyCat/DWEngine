namespace DWOSvgTests
{
 bool Check(bool Good,FString Message){if(!Good)UOpusNative::WriteLog("SVG_TEST_FAIL "+Message);return Good;}
 USvgCanvasNode FindLiveAction(UWidget Widget,FString Op,FString Value)
 {
  auto Node=Cast<USvgCanvasNode>(Widget);if(Node!=nullptr){auto A=UOpusUI::Action(Node);if(A.Type=="opus"&&A.Op==Op&&A.Value==Value)return Node;}
  auto Panel=Cast<UPanelWidget>(Widget);if(Panel!=nullptr)for(auto Child:Panel.GetAllChildren()){auto Found=FindLiveAction(Child,Op,Value);if(Found!=nullptr)return Found;}return nullptr;
 }
 bool Run(ADWOTowerGameMode T)
 {
  bool Ok=true;FString Error;auto H=T.Hud;
  Ok=Check(T.Run.Phase==EDWOTowerPhase::Ready&&T.Enemies.Num()==0,"default is manual Ready")&&Ok;
  int EquippedSkills=0;for(int I=0;I<4;I++)if(UOpusData::SkillName(T.Rules,I)!="空槽"&&!UOpusData::SkillName(T.Rules,I).IsEmpty())EquippedSkills++;
  Ok=Check(EquippedSkills==1,"one starting skill")&&Ok;
  auto Bag=UOpusData::Inventory(T.Rules);bool Starter=false;for(auto E:Bag)if(E.Equipped&&E.Tier==1)Starter=true;Ok=Check(Starter,"T1 starter equipped")&&Ok;
  Ok=Check(H.Screen!=nullptr&&UOpusUI::StateCount(H.Screen)==45,"current SVG document including map core selection and map device")&&Ok;if(H.Screen==nullptr)return false;
  Ok=Check(H.Screen.FindSourceNode("hud-screen-instance::rs-statistics-instance::rs-wallet-value")==nullptr&&H.Screen.FindSourceNode("hud-screen-instance::rs-reset")==nullptr&&H.Screen.FindSourceNode("hud-screen-instance::battle-hud-atlas")==nullptr,"removed combat HUD controls are absent from saved document")&&Ok;
  Ok=Check(H.Screen.FindSourceNode("hud-screen-instance::rs-statistics-instance::rs-stats-dps")!=nullptr,"DPS is in top statistics")&&Ok;
  auto Builds=H.Screen.FindSourceNode("hud-screen-instance::rs-rail-instance::rs-function-bd::rs-function-button");if(Builds!=nullptr)H.Screen.HandleInvoke(Builds);
  Ok=Check(Builds!=nullptr&&H.Screen.CurrentState=="builds","right rail directly opens actual build selector")&&Ok;
  auto Reset=H.Screen.FindSourceNode("hud-screen-instance::rs-rail-instance::rs-function-restart::rs-function-button");
  Ok=Check(Reset!=nullptr&&UOpusUI::Action(Reset).Type=="restart","right rail uses current-floor restart transaction")&&Ok;
  auto Gear=H.Screen.FindSourceNode("hud-screen-instance::rs-rail-instance::rs-function-settings::rs-function-button");if(Gear!=nullptr)H.Screen.HandleInvoke(Gear);
  Ok=Check(Gear!=nullptr&&H.Screen.CurrentState=="settings","gear opens star map challenge settings")&&Ok;H.Screen.ApplySceneState("selected-1");
  Ok=Check(H.Screen.FindSourceNode("battle_live_modal_page::battle-bag-wallet")!=nullptr,"inventory owns wallet display")&&Ok;
  H.RefreshData();H.Screen.HandleInputKey(EKeys::B);Ok=Check(H.Screen.CurrentState=="inventory","inventory keyboard navigation")&&Ok;H.Screen.HandleInputKey(EKeys::Escape);Ok=Check(H.Screen.CurrentState=="selected-1","escape returns to combat")&&Ok;
  H.Execute("buy","skill:flame_burst",Error);Ok=Check(Error.IsEmpty(),"AS shop transaction "+Error)&&Ok;
  bool SkillHasIcon=false;for(auto E:UOpusData::Inventory(T.Rules))if(E.Id=="flame_burst")SkillHasIcon=E.Icon.Contains("flame_burst.png");Ok=Check(SkillHasIcon,"owned skill keeps its SVG inventory icon")&&Ok;
  T.Rules.SelectPlayerSkill(1,Error);H.Screen.ApplySceneState("picker-2");H.RefreshData();auto Choice=FindLiveAction(H.Screen.FindSourceNode("hud-screen-instance::rs-picker-instance::rs-picker-list"),"assign","flame_burst");Ok=Check(Choice!=nullptr,"authored SVG skill row receives live action")&&Ok;if(Choice!=nullptr)H.Screen.HandleInvoke(Choice);Ok=Check(UOpusData::SkillIcon(T.Rules,1).Contains("flame_burst"),"SVG skill row assigns the actual selected slot")&&Ok;
  H.Screen.ApplySceneState("inventory");H.RefreshData();auto Cell=FindLiveAction(H.Screen.FindSourceNode("battle_live_modal_page::battle-bag-grid"),"select",Bag[0].Id);Ok=Check(Cell!=nullptr,"authored SVG inventory cell receives live action")&&Ok;if(Cell!=nullptr)H.Screen.HandleInvoke(Cell);Ok=Check(H.SelectedId==Bag[0].Id,"SVG inventory cell selects the actual item")&&Ok;H.Screen.HandleInputKey(EKeys::Escape);
  FString Saved=T.Rules.ExportPlayerState();Ok=Check(T.Rules.ImportPlayerState(Saved,Error)&&T.Rules.ExportPlayerState()==Saved,"profile roundtrip")&&Ok;
  auto Button=H.Screen.FindSourceNode("battle_live_modal_page::battle-settings-challenge");Ok=Check(Button!=nullptr,"saved SVG challenge button exists")&&Ok;if(Button!=nullptr)H.Screen.HandleInvoke(Button);Ok=Check(T.Run.Phase==EDWOTowerPhase::Waves&&T.Preparing&&T.Enemies.Num()==0,"SVG button queues first native encounter")&&Ok;
  return Ok;
 }
 bool Complete(ADWOTowerGameMode T)
 {
  bool Ok=Check(T.Run.Phase==EDWOTowerPhase::Waves&&T.Enemies.Num()==7,"SVG button enters first native encounter across frames");auto H=T.Hud;
  auto Next=H.Screen.FindSourceNode("battle_live_modal_page::battle-completion-explore");Ok=Check(Next!=nullptr,"saved completion button exists")&&Ok;if(Next!=nullptr){T.Run.Phase=EDWOTowerPhase::Cleared;H.Screen.HandleInvoke(Next);Ok=Check(UDWOTowerJourney::Get().Floor==2&&H.Screen.CurrentState!="atlas","completion button challenges next floor through AS")&&Ok;}
  UOpusNative::WriteLog(Ok?"SVG_TEST_PASS RuntimeBindings":"SVG_TEST_FAIL RuntimeBindings");return Ok;
 }
}
class UDWOSvgContractTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 bool Done=false,Started=false;
 float ReadyWait=0;
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;
  if(!Started)
  {
   ReadyWait+=Dt;
   if(!Tower.DropService.Ready||!Tower.Dungeon.Ready){if(ReadyWait>30){Done=true;DWOSvgTests::Check(false,"native services did not become ready");UOpusNative::Quit(false);}return;}
   Started=true;if(!DWOSvgTests::Run(Tower)){Done=true;UOpusNative::Quit(false);}return;
  }
  if(Tower.Preparing)return;Done=true;UOpusNative::Quit(DWOSvgTests::Complete(Tower));
 }
}
