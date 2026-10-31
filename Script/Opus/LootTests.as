// Transient integration fixture: a real native cast kills a registered boss;
// the actual UICreator button then walks the hero to the native pickup radius.
class UDWOLootTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Target;
 FOpusEntry Drop;
 int Stage=0,BeforeCount=0;
 bool SawFlight=false;
 bool SawHighlight=false;
 bool PreviewCaptured=false;
 double PreviewClock=0;
 double Clock=0,LastCast=-10;
 UButton NativeInput(USvgCanvasNode Node){for(auto W:Node.GetAllChildren()){auto B=Cast<UButton>(W);if(B!=nullptr)return B;}return nullptr;}
 bool Check(bool Good,FString Message)
 {
  if(!Good){UOpusNative::WriteLog("LOOT_TEST_FAIL "+Message);UOpusNative::Quit(false);}return Good;
 }
 int CountItem(FString Id)
 {
  for(auto E:UOpusData::Inventory(Tower.Rules))if(E.Id==Id)return E.Count;return 0;
 }
 void Initialize(ADWOTowerGameMode InTower)
 {
  Tower=InTower;Tower.StartChallenge();Stage=-2;
 }
 void BeginProbe()
 {
  Target=Tower.Enemies.Last();Stage=0;
  for(auto E:Tower.Enemies)E.Speed=0;
  if(UOpusNative::Flag("DWOLootPreview")){FVector Near;if(UOpusNative::Flag("DWOHeroArena"))Target.SetActorLocation(Tower.Hero.ActorLocation+FVector(350,0,-40));else if(UOpusDungeon::Project(Tower,Tower.Hero.ActorLocation+FVector(350,0,0),Near))Target.SetActorLocation(Near+FVector(0,0,60));}
  Check(!Tower.Hud.HunterEnabled&&Tower.Hud.ShowLoot,"fresh run shows ground loot without overriding Hunter settings");
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Tower==nullptr)return;
  if(Stage==-2){if(Tower.Dungeon==nullptr||!Tower.Dungeon.Ready||!Tower.Hero.Visual.Ready||Tower.Preparing)return;Tower.SpawnEncounter(EDWOTowerCommand::Boss);Stage=-1;return;}
  if(Stage==-1){if(Tower.Preparing)return;BeginProbe();return;}
  if(Target==nullptr)return;Clock+=DeltaSeconds;
  if(Stage==2&&UOpusNative::Flag("DWOLootPreview"))
  {
   PreviewClock+=DeltaSeconds;FString File=UOpusNative::StringOption("DWOLootCapture");
   bool Ready=UOpusWorldUI::VisualReady(Tower.WorldUI.Hunter.Panel)&&UOpusWorldUI::VisualReady(Tower.WorldUI.DetailPanel);
   if(!File.IsEmpty()&&!PreviewCaptured&&PreviewClock>=1.0&&Ready){PreviewCaptured=true;System::ExecuteConsoleCommand("Shot ShowUI filename="+File+" -nosuffix");UOpusNative::WriteLog("LOOT_TEST_PASS VisualPreviewReady");}
   if(!PreviewCaptured&&PreviewClock>12){Check(false,"exported UI images or geometry were not ready for capture");return;}
   if(PreviewCaptured&&PreviewClock>=1.5)UOpusNative::Quit(true);return;
  }
  if(Clock>25){Check(false,"native death/pickup timed out");return;}
  if(Stage==0)
  {
   if(!Target.Dead){if(Clock-LastCast>1&&Tower.Hero.CastAt(Target.ActorLocation-FVector(60,0,0),Target.ActorLocation,100000))LastCast=Clock;return;}
   auto Ground=UOpusData::Ground(Tower.Rules);if(!Check(Ground.Num()>=2,"native boss generates equipment and item drops"))return;
   Drop=Ground[0];Tower.WorldUI.Refresh();
   if(!Tower.WorldUI.Labels.Contains(Drop.Key)){if(!SawFlight){SawFlight=true;if(!Check(Tower.WorldUI.Markers.Contains(Drop.Key),"drop flight marker exists before its label"))return;UOpusNative::WriteLog("LOOT_TEST_PASS FlightBeforeLabel");}return;}
   if(!Check(SawFlight,"labels remain hidden during flight"))return;
   auto Label=Tower.WorldUI.Labels[Drop.Key];FString ExpectedHash=UOpusNative::StringOption("DWOExpectedWorldUIHash");if(ExpectedHash.IsEmpty())ExpectedHash="2d013876962f9cb2b5607d7925341e45175f44baf7052cca32ba4ceda225c583";if(!Check(Label.SourceDocument.SourceHash==ExpectedHash,"UICreator export is the label source"))return;
   FVector At;for(auto L:UOpusNative::Loot(Tower.Rules))if(L.Key==Drop.Key)At=L.Position;
   auto Button=Label.FindSourceNode(DWOLoot::Family(Drop)+"-label");if(!Check(Button!=nullptr&&Button.Normal.Texture!=nullptr,"saved UE brush and pickup button exist"))return;
   TArray<FString> Families;Families.Add("common");Families.Add("magic");Families.Add("rare");Families.Add("legendary");Families.Add("unique");Families.Add("currency-1");Families.Add("currency-2");Families.Add("currency-3");Families.Add("currency-special");Families.Add("omen");
   for(auto Family:Families){auto Control=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Family+"-template","UICreatorLoot",60);if(!Check(Control!=nullptr,"family template "+Family))return;auto N=Control.FindSourceNode(Family+"-label");if(!Check(N!=nullptr&&N.Normal.Texture!=nullptr&&N.Hovered.Texture!=nullptr&&N.Pressed.Texture!=nullptr&&N.Disabled.Texture!=nullptr,"four saved brush states "+Family))return;Control.RemoveFromParent();}
   UOpusNative::WriteLog("LOOT_TEST_PASS TenFamiliesAndFourStates");
   NativeInput(Button).OnHovered.Broadcast();if(!Check(Tower.WorldUI.DetailPanel!=nullptr&&!Tower.WorldUI.Pinned,"actual UICreator hover event shows ground details"))return;
   Tower.WorldUI.FilterKey(EKeys::LeftControl);Tower.WorldUI.Leave(Button);if(!Check(Tower.WorldUI.Pinned&&Tower.WorldUI.DetailKey==Drop.Key,"Ctrl pins ground details across pointer leave"))return;
   if(UOpusNative::Flag("DWOLootPreview")){FVector Near;if(UOpusDungeon::Project(Tower,At+FVector(0,330,0),Near))Tower.Hero.SetActorLocation(Near+FVector(0,0,101));Tower.WorldUI.Hunter.Open();NativeInput(Button).OnHovered.Broadcast();Tower.WorldUI.Pin();Stage=2;return;}
   Tower.WorldUI.FilterKey(EKeys::Escape);if(!Check(Tower.WorldUI.DetailPanel==nullptr,"Escape dismisses ground details"))return;
   for(auto P:Tower.Dungeon.Points)if((P-At).Size2D()>450&&(P-At).Size2D()<800){Tower.Hero.SetActorLocation(P+FVector(0,0,41));break;}
   FString Error,Snapshot=Tower.Rules.ExportPlayerState();
   if(!Check(!UOpusNative::Pickup(Tower.Rules,Drop.Key,Tower.Hero.ActorLocation,Error)&&Snapshot==Tower.Rules.ExportPlayerState(),"far pickup rejected without changing inventory"))return;
   BeforeCount=CountItem(Drop.Id);
   if(!Check(Button!=nullptr,"actual exported pickup button exists"))return;
   Label.HandleInvoke(Button);if(!Check(Tower.Hero.PickupTarget==Drop.Key,"exported button requests pickup movement"))return;
   NativeInput(Button).OnHovered.Broadcast();Tower.WorldUI.Pin();
   UOpusNative::WriteLog("LOOT_TEST_PASS NativeDeathAndLabel count="+Ground.Num()+" key="+Drop.Key);Stage=1;
  }
  else if(Stage==1)
  {
   if(Tower.WorldUI.Labels.Contains(Drop.Key)){auto N=Tower.WorldUI.Labels[Drop.Key].FindSourceNode(DWOLoot::Family(Drop)+"-label");if(N!=nullptr&&N.Normal.Texture==N.Hovered.Texture)SawHighlight=true;}
   bool Claimed=false;for(auto L:UOpusNative::Loot(Tower.Rules))if(L.Key==Drop.Key)Claimed=L.Claimed;if(!Claimed)return;
   if(!Check(SawHighlight,"picking uses the authored hover highlight"))return;
   if(!Check(CountItem(Drop.Id)==BeforeCount+Drop.Count,"walking pickup grants the actual item once"))return;
   FString Error,Snapshot=Tower.Rules.ExportPlayerState();
   if(!Check(!UOpusNative::Pickup(Tower.Rules,Drop.Key,Tower.Hero.ActorLocation,Error)&&Snapshot==Tower.Rules.ExportPlayerState(),"repeated pickup cannot duplicate rewards"))return;
   Tower.WorldUI.Refresh();if(!Check(!Tower.WorldUI.Labels.Contains(Drop.Key),"claimed drop removes its ground label"))return;
   if(!Check(!Tower.WorldUI.Markers.Contains(Drop.Key)&&Tower.WorldUI.DetailPanel==nullptr,"claim also removes the marker and pinned details"))return;
   Tower.Hud.HunterEnabled=true;Tower.WorldUI.Refresh();if(!Check(Tower.WorldUI.Labels.Num()==0,"empty Hunter filter hides unselected drops"))return;
   Tower.Hud.HunterEnabled=false;Tower.WorldUI.Refresh();if(!Check(Tower.WorldUI.Labels.Num()>0,"disabling Hunter restores remaining drops"))return;
   auto Hunter=Tower.WorldUI.Hunter;Hunter.Open();if(!Check(Hunter.Visible()&&Hunter.AffixRows.Num()==12&&Hunter.CurrencyRows.Num()>0,"UICreator Hunter mounts authored affix and currency rows"))return;
   auto First=Hunter.AffixRows[0];First.HandleInvoke(First.FindSourceNode("hunter-row"));if(!Check(Tower.Hud.HunterTargets.Num()==1,"exported Hunter row changes the actual selection"))return;
   Hunter.Panel.HandleInvoke(Hunter.Panel.FindSourceNode("hunter-next"));if(!Check(Hunter.Page==1,"exported Hunter next-page button changes data page"))return;
   UEditableTextBox Query;for(auto W:Hunter.Panel.FindSourceNode("hunter-search").GetAllChildren()){auto I=Cast<UEditableTextBox>(W);if(I!=nullptr)Query=I;}
   if(!Check(Query!=nullptr,"authored Hunter field has a native text input"))return;Query.SetText(FText::FromString("冰缓"));Hunter.Refresh();if(!Check(Hunter.Query=="冰缓"&&Hunter.Page==0&&Hunter.Matches>0&&Hunter.AffixRows.Num()<=12,"Chinese query filters real native affixes and resets paging"))return;
   Hunter.Close();if(!Check(Tower.Hud.HunterTargets.Num()==1,"closing Hunter preserves selection"))return;
   UOpusNative::WriteLog("LOOT_TEST_PASS HunterAndPinnedHover");
   FString Saved=Tower.Rules.ExportPlayerState();if(!Check(Tower.Rules.ImportPlayerState(Saved,Error)&&Tower.Rules.ExportPlayerState()==Saved,"picked inventory survives profile roundtrip"))return;
   UOpusNative::WriteLog("LOOT_TEST_PASS WalkingPickupAndInventory");UOpusNative::WriteLog("LOOT_TEST_PASS RuntimeIntegration");UOpusNative::Quit(true);Stage=2;
  }
 }
}
