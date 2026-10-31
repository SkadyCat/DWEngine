class UDWOSvgHUD : UActorComponent
{
 UPROPERTY() USvgCanvasScreen Screen;
 UPROPERTY() UDWOItemHover ItemHover;
 UPROPERTY() UDWOInventoryDrag InventoryDrag;
 UPROPERTY() UDWOInventoryLayout InventoryLayout;
 UPROPERTY() UDWOMapCoreUI MapCoreUI;
 UPROPERTY() UDWOMapDevice MapDevice;
 UPROPERTY() UDWOTalentConstellation Talents;
 UPROPERTY() UDWOBuildMenu BuildMenu;
 UPROPERTY() UDWOPlayerEffects PlayerEffects;
 UPROPERTY() UDWOHotbarPicker Hotbar;
 UPROPERTY() UOpusInventoryNotices NewItems;
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TArray<FOpusEntry> Bag;
 FString SelectedId,CurrencyId,OmenId,SelectedCompartment,SelectedKey;
 int BagPage=0,AffixIndex=-1,ShopTab=0;
 bool OnlyAffordable=false,HunterEnabled=false,HunterOnly=false,ShowLoot=true;
 TArray<FString> HunterTargets,HunterCurrencies;
 TArray<FOpusEntry> HunterPool;
 int HunterPage=0;
 FString GearFilter="all";
 bool ModalPaused=false;
 int BuildFamily=1,BuildPage=0;
 FString BuildFeedback;
 double RefreshIn=0;
 int RefreshPart=0;
 FString RefreshedState;
 EDWOTowerPhase LastPhase=EDWOTowerPhase::Ready;
 void Initialize(ADWOTowerGameMode InTower)
 {
  Tower=InTower;if(UOpusNative::Flag("DWONoHUD"))return;
  auto SavedHunter=UDWOTowerJourney::Get();HunterEnabled=SavedHunter.HunterEnabled;ShowLoot=SavedHunter.ShowLoot;HunterTargets=SavedHunter.HunterTargets;HunterCurrencies=SavedHunter.HunterCurrencies;
  Screen=UOpusUI::CreateHUD(Gameplay::GetPlayerController(0));
  if(Screen==nullptr){UOpusNative::WriteLog("HUD_ERROR missing saved SVG WBP");return;}
  if(UOpusNative::Flag("DWOSvgReference")){FString ReferenceView=UOpusNative::StringOption("DWOView");Screen.ApplySceneState(ReferenceView.IsEmpty()?"selected-1":ReferenceView);return;}
  Screen.OnSourceAction.AddUFunction(this,n"OnAction");
  Hotbar=UDWOHotbarPicker::Create(Tower);Hotbar.Initialize(this);
  NewItems=UOpusInventoryNotices::Create(!UOpusNative::Flag("DWOTransient"));NewItems.Observe(UOpusData::Inventory(Tower.Rules));
  ItemHover=UDWOItemHover::Create(Tower);ItemHover.Initialize(this);
  InventoryLayout=UDWOInventoryLayout::Create(Tower);InventoryLayout.Initialize(this);
  InventoryDrag=UDWOInventoryDrag::Create(Tower);InventoryDrag.Initialize(this);
  MapCoreUI=UDWOMapCoreUI::Create(Tower);MapCoreUI.Initialize(this);
  MapDevice=UDWOMapDevice::Create(Tower);MapDevice.Initialize(this);
  if(Talents==nullptr){Talents=UDWOTalentConstellation::Create(Tower);Talents.Initialize(this);}
  BuildMenu=UDWOBuildMenu::Create(Tower);BuildMenu.Initialize(this);
  PlayerEffects=UDWOPlayerEffects::Create(Tower);PlayerEffects.Initialize(Tower);
  Screen.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");
  UOpusUI::HideSurface(Screen,"hud-scene-root");
  UOpusUI::HidePrefix(Screen,"battle-character-grid-scroll::native-scrollbar");
  UOpusUI::BindNode(Screen,"hud-screen-instance::rs-help","opus-live-help");UOpusUI::SetText(Screen,"opus-live-help","WASD 移动 · 鼠标瞄准 · 左键/空格施法 · 1–4 技能 · Q E T G 光环/消耗品 · Shift 闪避 · Z 掉落 · Tab 地牢地图 · M 世界地图 · P 天赋");
  UOpusUI::BindNode(Screen,"hud-screen-instance::rs-objective-instance::rs-floor","live-floor");UOpusUI::BindNode(Screen,"hud-screen-instance::rs-boss-title","live-boss-title");
  UOpusUI::BindNode(Screen,"hud-screen-instance::rs-hotbar-instance::rs-consumables::hud-quick-instance-F::hud-quick-key","live-third-belt-key");UOpusUI::SetText(Screen,"live-third-belt-key","T");
  UOpusUI::BindNode(Screen,"hud-screen-instance::rs-summons-instance::rs-summon-empty","live-summon-empty");
  for(int I=0;I<4;I++){FString Root="hud-screen-instance::rs-hotbar-instance::rs-abilities::hud-ability-instance-"+(I+1)+"::hud-ability-";UOpusUI::BindNode(Screen,Root+"name","live-skill-name-"+I);UOpusUI::BindNode(Screen,Root+"flame","live-skill-icon-"+I);UOpusUI::BindNode(Screen,Root+"plus","live-skill-empty-"+I);}
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-item-effects","inspector-effects");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-completion-stock","completion-offers");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-loot","ground-loot");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-craft","craft-materials");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-craft-affixes","craft-affixes");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-summons","summon-list");
  for(int I=0;I<3;I++)UOpusUI::BindNode(Screen,"hud-screen-instance::rs-map-instance::rs-map-affix-"+I,I==0?"live-map-packs":I==1?"live-map-life":"live-map-damage");UOpusUI::BindNode(Screen,"hud-screen-instance::rs-map-instance::rs-map-tier","live-map-tier");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-character-attributes","character-stats");UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-character-skill","live-character-skill");
  for(int I=0;I<6;I++){UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-craft-material-name-"+I,"live-material-name-"+I);UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-craft-material-n-"+I,"live-material-count-"+I);}
  HunterPool=UOpusData::AffixPool(Tower.Rules);
  TArray<FString> FilterLabels,FilterValues;FilterLabels.Add("全部物品");FilterValues.Add("all");FilterLabels.Add("全部装备");FilterValues.Add("equipment");for(auto Slot:DWOInventory::Slots()){FilterLabels.Add(SlotName(Slot));FilterValues.Add(Slot);}UOpusTalentBridge::Options(Screen,"battle_live_modal_page::battle-character-filter",FilterLabels,FilterValues,GearFilter);
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-bag-equip-label","live-equip-label");UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-item-equip-label","live-equip-label");
  TArray<FString> Results;Results.Add("completion");Results.Add("completion-empty");Results.Add("death");Results.Add("timeout");for(auto Result:Results){FString Root="battle_live_modal_page::battle-"+Result;UOpusUI::BindNode(Screen,Root+"-title","live-result-title");UOpusUI::BindNode(Screen,Root+"-unclaimed","live-unclaimed");UOpusUI::BindNode(Screen,Root+"-explore-label","live-next-floor");UOpusUI::BindNode(Screen,Root+"-atlas-label","live-return-settings");}
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-settings-monster-label","live-monster-choice");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-shop-owned","shop-owned");
  TArray<FString> Flags;Flags.Add("time-limit");Flags.Add("auto-cast");Flags.Add("god");Flags.Add("loot");for(auto Flag:Flags)UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-settings-"+Flag+"-label","live-setting-"+Flag);
  UOpusUI::HidePrefix(Screen,"battle-shop-pricing");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-item","inspector-materials");UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-item","inspector-omens");
  UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-hunter","hunter-currencies");
  for(int I=0;I<6;I++)UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-builds-family-"+I+"-label","live-family-"+I);
  for(int I=0;I<3;I++){UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-builds-grade-"+I+"-label","live-grade-"+I);UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-builds-details-"+I,"live-build-details-"+I);UOpusUI::BindNode(Screen,"battle_live_modal_page::battle-builds-details-"+I+"-label","live-build-details-label-"+I);}
  if(!UOpusNative::Flag("DWONoInputs")){UOpusUI::SetInput(Screen,"field:floor",""+Tower.Run.Floor);UOpusUI::SetInput(Screen,"field:monsterCount",""+UDWOTowerJourney::Get().MonsterCount);
  UOpusUI::SetInput(Screen,"field:bagQuery","");UOpusUI::SetInput(Screen,"field:shopQuery","");UOpusUI::SetInput(Screen,"field:craftQuery","");UOpusUI::SetInput(Screen,"field:characterQuery","");UOpusUI::SetInput(Screen,"field:hunterQuery","");}
  FString View=UOpusNative::StringOption("DWOView");if(!View.IsEmpty()){Screen.ApplySceneState(View);if(View.StartsWith("picker-")||View.StartsWith("selected-")){FString ViewError;Tower.Rules.SelectPlayerSkill(Screen.SelectedSlot,ViewError);}}
  UOpusNative::WriteLog("SVG_HUD_READY states="+UOpusUI::StateCount(Screen)+" source="+UOpusUI::SourceHash(Screen));
  if(UOpusNative::Flag("DWORightRailTest")&&UOpusNative::Flag("DWOTransient")){auto Test=UDWORightRailTest::Create(Tower);Test.Initialize(this);}
  if(UOpusNative::Flag("DWOBuildMenuTest")&&UOpusNative::Flag("DWOTransient")){auto Test=UDWOBuildMenuTests::Create(Tower);Test.Initialize(this);}
  if(UOpusNative::Flag("DWOAuraClickTest")&&UOpusNative::Flag("DWOTransient")){auto Test=UDWOAuraClickTest::Create(Tower);Test.Initialize(this);}
  if(UOpusNative::Flag("DWOHotbarTest")&&UOpusNative::Flag("DWOTransient")){auto Test=UDWOHotbarPickerTests::Create(Tower);Test.Initialize(Tower);}
  if(UOpusNative::Flag("DWOIconCompletionTest")&&UOpusNative::Flag("DWOTransient")){auto Test=UDWOIconCompletionTest::Create(Tower);Test.Initialize(this);}
 }
 UFUNCTION() void OnAction(USvgCanvasNode Node)
 {
  if(Tower==nullptr||Tower.Rules==nullptr)return;
  auto Action=UOpusUI::Action(Node);FString Error;
  if(Action.Type=="ability"&&Hotbar!=nullptr){Screen.SourceActionHandled=true;Hotbar.Open("skill",Action.Index);return;}
  if(Action.Type=="belt")
  {Screen.SourceActionHandled=true;if(Hotbar!=nullptr)Hotbar.Open("quick",Action.Index);return;}
  if(Action.Type=="battle-ui"&&Action.Op=="navigate"&&Action.Value=="builds")
  {Screen.SourceActionHandled=true;if(BuildMenu==nullptr){BuildMenu=UDWOBuildMenu::Create(Tower);BuildMenu.Initialize(this);}BuildMenu.Open();return;}
  if(BuildMenu!=nullptr&&BuildMenu.Visible&&(Action.Type=="battle-ui"||Action.Type=="ability"||Action.Type=="restart"))BuildMenu.Close();
  if(Action.Type=="battle-ui"&&Action.Op=="talents")
  {
   Screen.SourceActionHandled=true;
   if(Talents!=nullptr){if(Talents.Visible)Talents.Close();else{Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Tower.Rules)+1));Talents.Open();}}
   return;
  }
  if(Action.Type=="battle-ui"&&Action.Op=="navigate"&&Action.Value=="hunter")
  {Screen.SourceActionHandled=true;if(Tower.WorldUI.Hunter!=nullptr)Tower.WorldUI.Hunter.Open();return;}
  if(Action.Type=="opus"&&(Action.Op=="select"||Action.Op=="select-stock"||Action.Op=="select-equipped"))
  {
   Screen.SourceActionHandled=true;if(InventoryDrag!=nullptr&&InventoryDrag.ConsumeClick())return;
   UOpusActionNode N=nullptr;UWidget Ancestor=Node;while(Ancestor!=nullptr){auto Candidate=Cast<UOpusActionNode>(Ancestor);if(Candidate!=nullptr&&Candidate.HoverId==Action.Value){N=Candidate;break;}Ancestor=Ancestor.GetParent();}
   FString Role=DWOInventory::Role(Node),Key=N==nullptr?"":N.PlacementKey;
   bool Same=SelectedId==Action.Value&&SelectedCompartment==Role&&(Key.IsEmpty()||SelectedKey==Key);
   SelectedId=Same?"":Action.Value;SelectedCompartment=Same?"":Role;SelectedKey=Same?"":Key;
   Tower.LastError="";RefreshIn=0;RefreshPart=0;return;
  }
  if(Action.Type=="battle-ui"&&Action.Op=="navigate"&&Action.Value=="atlas")
  {Screen.SourceActionHandled=true;if(Node.SourceId.Contains("completion")&&Tower.Run.Phase==EDWOTowerPhase::Cleared){MapDevice.Open();}else MapDevice.Open();return;}
  if(Action.Type=="ability"){Tower.Rules.SelectPlayerSkill(Action.Index,Error);}
  else if(Action.Type=="assign"){Tower.Rules.ChangePlayerSkill(UOpusNative::Selected(Tower.Rules),FName(Action.Value),Error);if(Error.IsEmpty())Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Tower.Rules)+1));}
  else if(Action.Type=="pause")Tower.SetPaused(!Tower.Paused);
  else if(Action.Type=="restart")Tower.Retry(true);
  else if(Action.Type=="battle-ui"&&Action.Op=="challenge")
  {
   Tower.SetMonsterCount(UOpusUI::IntInput(Screen,"field:monsterCount",UDWOTowerJourney::Get().MonsterCount));
   int Requested=Node.SourceId.Contains("settings")?UOpusUI::IntInput(Screen,"field:floor",Tower.Run.Floor):Tower.Run.Phase==EDWOTowerPhase::Cleared?Tower.Run.Floor+1:Tower.Run.Floor;
   if(Tower.Run.Phase==EDWOTowerPhase::Failed&&Node.SourceId.Contains("restart")){Tower.Retry();return;}
   if(Tower.Run.Phase==EDWOTowerPhase::Ready&&Requested==Tower.Run.Floor){Tower.SetMonsterCount(UOpusUI::IntInput(Screen,"field:monsterCount",Tower.Run.MonsterCount));Screen.ApplySceneState("selected-1");Tower.StartChallenge();}
   else Tower.ChallengeFloor(Requested);
  }
  else if(Action.Type=="setting")
  {
   if(Action.Value=="loot"){ShowLoot=!ShowLoot;SaveHunter();}
   if(Action.Value=="time-limit"){Tower.Run.SetTimeLimit(!Tower.Run.TimeLimitEnabled);UDWOTowerJourney::Get().TimeLimit=Tower.Run.TimeLimitEnabled;}
   if(Action.Value=="auto-cast")Tower.Hero.AutoCast=!Tower.Hero.AutoCast;
   if(Action.Value=="god"){auto Journey=UDWOTowerJourney::Get();Journey.God=!Journey.God;Tower.Rules.SetPlayerGod(Journey.God);}
  }
  if(Action.Type=="hunter-toggle"){HunterEnabled=!HunterEnabled;SaveHunter();}
  if(Action.Type=="opus")Execute(Action.Op,Action.Value,Error);
  else if(Action.Type=="battle-ui")
  {
   if(Action.Op=="equip")EquipSelected(Error);
   if(Action.Op=="identify")IdentifySelected(Error);
   if(Action.Op=="sell"){int64 Price=0;Tower.Rules.SellPlayerEquipment(SelectedId,Price,Error);}
   if(Action.Op=="discard")Tower.Rules.DiscardPlayerEquipment(SelectedId,Error);
   if(Action.Op=="bag-craft"){Screen.ApplySceneState("craft");}
   if(Action.Op=="craft-run")Tower.Rules.CraftPlayerEquipment(SelectedId,FName(CurrencyId),FName(OmenId),AffixIndex,Error);
   if(Action.Op=="item-affix")AffixIndex=Action.Index;
   if(Action.Op=="belt-equip")Tower.Rules.SetPlayerBeltSlot(Action.Index,FName(SelectedId),Error);
   if(Action.Op=="bag-page")BagPage=Math::Max(0,BagPage+Action.Index);
   if(Action.Op=="shop-tab")ShopTab=Action.Index;
   if(Action.Op=="bd-family"){BuildFamily=Action.Index;BuildPage=0;BuildFeedback="";}
   if(Action.Op=="bd-apply"){auto Builds=BuildChoices();int At=BuildPage*3+Action.Index;if(Builds.IsValidIndex(At)){BuildFeedback="已应用 "+Builds[At].Name;Execute("preset",Builds[At].Id,Error);}}
   if(Action.Op=="affordable")OnlyAffordable=!OnlyAffordable;
   if(Action.Op=="monster-menu"){auto Journey=UDWOTowerJourney::Get();TArray<FName> Choices;Choices.Add(NAME_None);for(auto D:UOpusNative::Monsters(Tower.Rules))if(D.Enabled&&D.MinFloor<=Tower.Run.Floor)Choices.Add(D.Id);int At=Choices.FindIndex(Journey.MonsterType);Journey.MonsterType=Choices[(At+1)%Choices.Num()];}
   if(Action.Op=="gear-filter"){GearFilter=Action.Value;Screen.ApplySceneState("equipment");}
   if(Action.Op=="overlay"){if(Action.Value=="affix-book"||Action.Value=="affix")Screen.ApplySceneState("hunter");else if(Action.Value=="material")Screen.ApplySceneState("craft");}
   if(Action.Op=="inspect-selected")Screen.ApplySceneState("item");
   if(Action.Op=="progression"){if(Tower.Rules.StartPlayerProgression(Error)){Tower.Rules.RespawnPlayer();Tower.Retry();}}
   if(Action.Op=="map-info-tab")Screen.ApplySceneState(Action.Value=="lineage"?"map-lineage":"map-equipment");
   if(Action.Op=="hunter-only"){HunterOnly=!HunterOnly;HunterPage=0;}
   if(Action.Op=="hunter-clear")HunterTargets.Empty();
   if(Action.Op=="currency-clear")HunterCurrencies.Empty();
   if(Action.Op=="hunter-page")HunterPage=Math::Max(0,HunterPage+Action.Index);
  }
  Tower.LastError=Error;if(Error.IsEmpty())Tower.SaveProfile();
  RefreshIn=0;RefreshPart=0;
 }
 void EnsureInventoryInteraction()
 {
  // Existing HUD instances survive AS hot reload, so new components cannot
  // depend solely on the one-time Initialize call.
  if(Tower==nullptr||Screen==nullptr||ItemHover==nullptr)return;
  if(InventoryLayout==nullptr){InventoryLayout=UDWOInventoryLayout::Create(Tower);InventoryLayout.Initialize(this);}
  if(InventoryDrag==nullptr){InventoryDrag=UDWOInventoryDrag::Create(Tower);InventoryDrag.Initialize(this);}
 }
 UFUNCTION() bool FilterKey(FKey Key)
 {
  if(Hotbar!=nullptr&&Hotbar.FilterKey(Key))return true;
  EnsureInventoryInteraction();
  if(Tower.Studio.Ready&&Tower.Studio.UI.FilterKey(Key))return true;
  if(Tower.Studio.Ready&&Tower.Studio.Map.FilterKey(Key))return true;
  if(Key==EKeys::Escape&&InventoryDrag!=nullptr&&!InventoryDrag.Uid.IsEmpty()){InventoryDrag.Cancel();return true;}
  if(Key==EKeys::LeftMouseButton&&InventoryDrag!=nullptr)InventoryDrag.Press();
  if(BuildMenu!=nullptr&&BuildMenu.FilterKey(Key))return true;
  if(MapDevice!=nullptr&&MapDevice.FilterKey(Key))return true;
  if(Talents!=nullptr&&Talents.FilterKey(Key))return true;
  if(Tower.WorldUI!=nullptr&&Tower.WorldUI.FilterKey(Key))return true;
  if(MapCoreUI!=nullptr&&MapCoreUI.FilterKey(Key))return true;
  return ItemHover!=nullptr&&ItemHover.FilterKey(Key);
 }
 void HandleKeys(APlayerController PC)
 {
  Tower.HudKeyConsumed=false;if(Screen==nullptr)return;
  if(Hotbar!=nullptr&&Hotbar.HandleKeys(PC)){Tower.HudKeyConsumed=true;return;}
  if(Tower.Studio.Ready&&Tower.Studio.Map.HandleKeys(PC)){Tower.HudKeyConsumed=true;return;}
  if(BuildMenu!=nullptr&&BuildMenu.HandleKeys(PC)){Tower.HudKeyConsumed=true;return;}
  if(MapDevice!=nullptr&&MapDevice.HandleKeys(PC)){Tower.HudKeyConsumed=true;return;}
  if(Talents!=nullptr&&Talents.HandleKeys(PC)){Tower.HudKeyConsumed=true;return;}
  if(Tower.WorldUI!=nullptr&&Tower.WorldUI.HandleKeys(PC)){Tower.HudKeyConsumed=true;return;}
  if(PC.WasInputKeyJustPressed(EKeys::Escape)&&MapCoreUI!=nullptr&&MapCoreUI.FilterKey(EKeys::Escape)){Tower.HudKeyConsumed=true;return;}
  if(ItemHover!=nullptr&&ItemHover.HandleKeys(PC)){Tower.HudKeyConsumed=true;return;}
  TArray<FKey> Keys;Keys.Add(EKeys::B);Keys.Add(EKeys::C);Keys.Add(EKeys::M);Keys.Add(EKeys::Escape);
  for(auto Key:Keys)if(PC.WasInputKeyJustPressed(Key))
  {if(Key==EKeys::M){MapDevice.Open();Tower.HudKeyConsumed=true;continue;}if(Key!=EKeys::Escape||UOpusUI::BlocksCombat(Screen)){Screen.HandleInputKey(Key);Tower.HudKeyConsumed=true;}}
  if(PC.WasInputKeyJustPressed(EKeys::Tab)&&UOpusUI::BlocksCombat(Screen)){Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Tower.Rules)+1));Tower.HudKeyConsumed=true;}
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Screen==nullptr||Tower==nullptr||Tower.Rules==nullptr||UOpusNative::Flag("DWOSvgReference"))return;
  EnsureInventoryInteraction();
  bool Modal=(Tower.Studio.Ready&&(Tower.Studio.UI.Panel!=nullptr||Tower.Studio.Map.Visible))||(Tower.Recent.UI!=nullptr&&Tower.Recent.UI.Panel!=nullptr)||Tower.Events.IsOpen()||(MapDevice!=nullptr&&MapDevice.Visible)||(Talents!=nullptr&&Talents.Visible)||(UOpusUI::BlocksCombat(Screen)&&!(Tower.Run.Phase==EDWOTowerPhase::Cleared&&Tower.Run.AutoAdvance));
  if(Modal&&!Tower.Paused){Tower.SetPaused(true);ModalPaused=true;}
  else if(!Modal&&ModalPaused){Tower.SetPaused(false);ModalPaused=false;}
  // Populate one data group per frame. State changes restart the queue so the
  // visible panel is refreshed, while hidden panels never clone their rows.
  if(RefreshedState!=Screen.CurrentState){RefreshedState=Screen.CurrentState;RefreshPart=0;RefreshIn=0;}
  if(RefreshPart<8){RefreshDataPart(RefreshPart);RefreshPart++;}
  RefreshIn-=DeltaSeconds;if(RefreshIn>0)return;RefreshIn=0.1;
  if(RefreshPart>=8)RefreshPart=0;
  auto P=Tower.Rules.GetPlayerPools();
  UOpusUI::SetText(Screen,"health-text",""+int(P.Life)+" / "+int(P.MaxLife));
  UOpusUI::SetText(Screen,"shield-text",""+int(P.Shield)+" / "+int(P.MaxShield));
  UOpusUI::SetText(Screen,"mana-text",""+int(P.Mana)+" / "+int(P.MaxMana));
  UOpusUI::SetProgress(Screen,"health",P.MaxLife>0?P.Life/P.MaxLife:0);
  UOpusUI::SetProgress(Screen,"shield",P.MaxShield>0?P.Shield/P.MaxShield:0);
  UOpusUI::SetProgress(Screen,"mana",P.MaxMana>0?P.Mana/P.MaxMana:0);
  auto S=Tower.Hero.Stun;double Left=S.Remaining();
  UOpusUI::SetText(Screen,"stun-title","晕眩");
  UOpusUI::SetText(Screen,"stun-text",Left>0?"晕眩 "+Math::CeilToInt(Left*10)/10.0+"s":""+Math::CeilToInt(S.Meter.Amount)+" / "+Math::RoundToInt(P.MaxLife));
  UOpusUI::SetProgress(Screen,"stun",S.BuildupFraction());
  UOpusUI::SetText(Screen,"wallet",""+UOpusNative::Stones(Tower.Rules));
  int RemainingEnemies=0;bool BossAlive=false;for(auto Enemy:Tower.Enemies)if(IsValid(Enemy)&&!Enemy.Dead&&!Enemy.Staged&&Enemy.EventId.IsEmpty()){RemainingEnemies++;if(Enemy.Boss)BossAlive=true;}
  FString Objective=(Tower.Run.Phase==EDWOTowerPhase::Ready?"准备就绪 · 回车开始":Tower.Run.Phase==EDWOTowerPhase::Cleared?(Tower.MapItems.State.Active.Uid.IsEmpty()?"已通关 · N 世界地图":"已通关 · 使用下一张地图"):Tower.Run.Phase==EDWOTowerPhase::Failed?"挑战失败 · R 重试":Tower.Preparing?"正在准备整张地牢":"击败首领 · 怪物 "+RemainingEnemies+" 只")+(Tower.Run.TimeLimitEnabled?" · "+int(Tower.Run.Remaining())+"秒":" · 不限时");
  if(!Tower.DropService.Ready)Objective=Tower.DropService.Failed?Tower.DropService.Error+" · 回车重试":"正在同步掉落目录…";
  UOpusUI::SetText(Screen,"objective",Objective);
  auto Report=Tower.Rules.GetFrameReport();UOpusUI::SetText(Screen,"statistics","击杀 "+Tower.Kills+" · 投射物 "+Report.Projectiles+" · "+int(Tower.Run.Elapsed)+"s");
  UOpusUI::SetText(Screen,"live-floor","第"+Tower.Run.Floor+"层");UOpusUI::SetText(Screen,"live-boss-title","第 "+Tower.Run.Floor+" 层守卫");UOpusUI::SetShown(Screen,"live-summon-empty",UOpusNative::SummonCount(Tower.Rules)==0);
  UOpusUI::SetText(Screen,"summon-count",""+UOpusNative::SummonCount(Tower.Rules));
  UOpusUI::SetShown(Screen,"boss",BossAlive);
  UOpusUI::SetText(Screen,"live-result-title",Tower.Run.Phase==EDWOTowerPhase::Cleared?"第 "+Tower.Run.Floor+" 层通关":"止步第 "+Tower.Run.Floor+" 层");UOpusUI::SetText(Screen,"live-next-floor","前往世界地图");UOpusUI::SetText(Screen,"live-return-settings","挑战设置");UOpusUI::SetText(Screen,"live-unclaimed","地面剩余 "+UOpusData::Ground(Tower.Rules).Num()+" 件掉落，可返回拾取。");
  if(Tower.Run.Phase!=LastPhase){if(Tower.Run.Phase==EDWOTowerPhase::Cleared)Screen.ApplySceneState("completion");if(Tower.Run.Phase==EDWOTowerPhase::Failed)Screen.ApplySceneState(Tower.Run.Timeout?"timeout":"death");LastPhase=Tower.Run.Phase;}
 }

 void EquipSelected(FString& Error)
 {
  for(auto Raw:UOpusData::Inventory(Tower.Rules))if(Raw.Id==SelectedId){auto E=Raw;if(DWOInventory::Count(E,SelectedCompartment)<1){Error="物品已移至另一个背包";return;}if(E.Category=="map")Tower.MapItems.Use(E.Id,Error);else if(E.Kind=="equipment"){if(Tower.Rules.EquipPlayerItem(FName(E.Slot),DWOInventory::Worn(E,SelectedCompartment)?NAME_None:FName(E.Id),Error))SelectedCompartment=DWOInventory::Worn(E,SelectedCompartment)?"stock":"equipped";}else if(E.Kind=="skill")Tower.Rules.ChangePlayerSkill(UOpusNative::Selected(Tower.Rules),FName(E.Id),Error);else Tower.Rules.UsePlayerItem(FName(E.Id),Error);return;}
 }
 void IdentifySelected(FString& Error){FString Uid;if(Tower.Rules.IdentifyPlayerEquipment(FName(SelectedId),Uid,Error)&&!Uid.IsEmpty())SelectedId=Uid;}
 void Execute(FString Op,FString Value,FString& Error)
 {
  if(Op=="select"||Op=="craft-select"){SelectedId=Value;if(Op=="select"){for(auto E:Bag)if(E.Id==Value){Screen.ApplySceneState(E.Kind=="equipment"?(E.Identified?"item":"unidentified"):"inventory");break;}}}
  else if(Op=="inspect"){SelectedId=Value;Screen.ApplySceneState("inventory");}
  else if(Op=="assign"){Tower.Rules.ChangePlayerSkill(UOpusNative::Selected(Tower.Rules),FName(Value),Error);Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Tower.Rules)+1));}
  else if(Op=="buy"){FOpusEntry Map;if(Value.StartsWith("item:")&&Tower.MapItems.Definition(Value.Mid(5),Map))Tower.MapItems.Buy(Value,Error);else Tower.Rules.BuyPlayerListing(Value,Error);}
  else if(Op=="merchant")UOpusNative::BuyMerchant(Tower.Rules,Value,Error);
  else if(Op=="pickup"){Tower.Hero.PickLoot(Value);Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Tower.Rules)+1));}
  else if(Op=="preset"){if(UOpusNative::ApplyPreset(Tower.Rules,FName(Value),Error)){Tower.Rules.RespawnPlayer();Screen.ApplySceneState("selected-1");}}
  else if(Op=="currency")CurrencyId=Value;
  else if(Op=="omen")OmenId=Value;
  else if(Op=="affix"){for(int I=0;I<16;I++)if(Value==""+I)AffixIndex=I;}
  else if(Op=="hunter-affix"){if(HunterTargets.Contains(Value))HunterTargets.Remove(Value);else HunterTargets.Add(Value);}
  else if(Op=="hunter-currency"){if(Value.IsEmpty())HunterCurrencies.Empty();else if(HunterCurrencies.Contains(Value))HunterCurrencies.Remove(Value);else HunterCurrencies.Add(Value);}
  else if(Op=="build-page"){auto Builds=BuildChoices();BuildPage=(BuildPage+1)%Math::Max(1,int(Math::CeilToInt(double(Builds.Num())/3)));}
  else if(Op=="build-detail"){auto Builds=BuildChoices();int At=BuildPage*3+String::Conv_StringToInt(Value);if(Builds.IsValidIndex(At))BuildFeedback=Builds[At].Description;}
 }
 FString FamilyName(int I) const {return I==0?"自动施法":I==1?"余烬回响":I==2?"万剑归宗":I==3?"召唤万剑":I==4?"自毒连投":"全部构筑";}
 TArray<FOpusEntry> BuildChoices() const
 {
  TArray<FOpusEntry> Out;for(auto E:UOpusData::Presets())if(BuildFamily==5||(BuildFamily==0&&E.Id.StartsWith("autocast_"))||(BuildFamily==1&&E.Id.StartsWith("embers_echo"))||(BuildFamily==2&&E.Id.StartsWith("myriad_swords"))||(BuildFamily==3&&E.Id.StartsWith("summon_"))||(BuildFamily==4&&E.Id.StartsWith("self_poison")))Out.Add(E);return Out;
 }
 void RefreshBuilds()
 {
  for(int I=0;I<6;I++)UOpusUI::SetText(Screen,"live-family-"+I,FamilyName(I)+"    ›");UOpusUI::SetText(Screen,"bd-family-name",FamilyName(BuildFamily));
  UOpusUI::SetSelection(Screen,"bd-family:",BuildFamily,6);
  auto Builds=BuildChoices();for(int I=0;I<3;I++){int At=BuildPage*3+I;bool Present=Builds.IsValidIndex(At);UOpusUI::SetText(Screen,"live-grade-"+I,Present?Builds[At].Name+" · 应用":"暂无更多配装");UOpusUI::SetText(Screen,"live-build-details-label-"+I,I==2&&Builds.Num()>3?"下一组 ›":"+ 配装详情");UOpusUI::SetAction(Screen,"live-build-details-"+I,I==2&&Builds.Num()>3?"build-page":"build-detail",""+I);}
  UOpusUI::SetText(Screen,"bd-feedback",BuildFeedback);
 }
 bool HunterAllows(FOpusEntry E) const
 {
  if(!ShowLoot)return false;if(E.Origin=="inventory"||!HunterEnabled)return true;if(E.Kind=="item")return E.Category=="currency"&&HunterCurrencies.Contains(E.Id);
  if(E.Kind!="equipment"||!E.Identified||HunterTargets.Num()==0)return false;
  for(auto Id:HunterTargets)if(!E.AffixIds.Contains(Id))return false;return true;
 }
 void SaveHunter(){auto J=UDWOTowerJourney::Get();J.HunterEnabled=HunterEnabled;J.ShowLoot=ShowLoot;J.HunterTargets=HunterTargets;J.HunterCurrencies=HunterCurrencies;}
 void RefreshPanels()
 {
  if(UOpusUI::HasVisibleBinding(Screen,"character-stats")){DWOAttributeScroll::Ensure(this);TArray<FOpusUIRow> Stats;for(auto E:UOpusData::Character(Tower.Rules)){auto R=Row(E,"none");Stats.Add(R);}UOpusUI::SetRows(Screen,"character-stats",Stats);TArray<FOpusUIRow> Effects;for(auto E:UOpusData::CharacterEffects(Tower.Rules)){auto R=Row(E,"none");R.Meta=E.Kind;Effects.Add(R);}UOpusUI::SetRows(Screen,"character-traits-list",Effects);UOpusUI::SetText(Screen,"character-effects",""+Effects.Num()+" 项生效");UOpusUI::SetText(Screen,"live-character-skill",UOpusData::SkillName(Tower.Rules,UOpusNative::Selected(Tower.Rules)));}
  if(UOpusUI::HasVisibleBinding(Screen,"summon-list")){TArray<FOpusUIRow> Summons;for(auto E:UOpusData::Summons(Tower.Rules)){auto R=Row(E,"none");R.Detail=E.Description;Summons.Add(R);}if(Summons.Num()==0){FOpusUIRow Empty;Empty.Label="暂无召唤单位";Empty.Op="none";Summons.Add(Empty);}UOpusUI::SetRows(Screen,"summon-list",Summons);}
  UOpusUI::SetText(Screen,"live-map-packs","怪物群 +"+int(Tower.Map.Packs)+"%");UOpusUI::SetText(Screen,"live-map-life","地图召唤物气血 +"+int(Tower.Map.LifeIncreased)+"%");UOpusUI::SetText(Screen,"live-map-damage","地图召唤物伤害 +"+int(Tower.Map.DamageIncreased)+"%");UOpusUI::SetText(Screen,"live-map-tier","T"+Tower.Map.Tier+(UDWOTowerJourney::Get().God?" · 无敌":""));TArray<FOpusUIRow> MapRows;for(auto E:UOpusData::MapEquipment(Tower.Rules,UDWOTowerJourney::Get().MapCore)){auto R=Row(E,"none");MapRows.Add(R);}UOpusUI::SetRows(Screen,"map-info-equipment",MapRows);
  if(UOpusUI::HasVisibleBinding(Screen,"map-lineage")||UOpusUI::HasVisibleBinding(Screen,"map-info-lineage")){TArray<FOpusUIRow> FamilyRows;for(auto F:Tower.Lineages.Lineages){FOpusUIRow R;R.Label=F.DisplayName;R.Op="none";for(auto M:F.Members)R.Detail+=M.Archetype.ToString()+" · ";FamilyRows.Add(R);}UOpusUI::SetRows(Screen,"map-lineage",FamilyRows);UOpusUI::SetRows(Screen,"map-info-lineage",FamilyRows);}
  if(UOpusUI::HasVisibleBinding(Screen,"hunter-list")){TArray<FOpusUIRow> Hunters;FString Query=UOpusUI::Input(Screen,"field:hunterQuery");int Matches=0;for(auto E:HunterPool){bool Chosen=HunterTargets.Contains(E.Id);if((HunterOnly&&!Chosen)||(!Query.IsEmpty()&&!E.Name.Contains(Query)&&!E.Id.Contains(Query)))continue;if(Matches>=HunterPage*12&&Matches<(HunterPage+1)*12){auto R=Row(E,"hunter-affix");R.Label=(Chosen?"✓ ":"")+E.Name;Hunters.Add(R);}Matches++;}UOpusUI::SetRows(Screen,"hunter-list",Hunters);UOpusUI::SetText(Screen,"hunter-selected","已选 "+HunterTargets.Num()+" · 全部满足 AND");UOpusUI::SetText(Screen,"hunter-pagination",""+(HunterPage+1)+" / "+Math::Max(1,int(Math::CeilToInt(double(Matches)/12))));UOpusUI::SetText(Screen,"hunter-enabled",HunterEnabled?"✓ 启用过滤":"过滤已关闭");}
  auto Journey=UDWOTowerJourney::Get();UOpusUI::SetText(Screen,"live-monster-choice",Journey.MonsterType.IsNone()?"全部敌人 · 点击切换":Journey.MonsterType.ToString());UOpusUI::SetText(Screen,"live-setting-time-limit",(Tower.Run.TimeLimitEnabled?"✓ ":"□ ")+"30 秒限时");UOpusUI::SetText(Screen,"live-setting-auto-cast",(Tower.Hero.AutoCast?"✓ ":"□ ")+"自动施法");UOpusUI::SetText(Screen,"live-setting-god",(Journey.God?"✓ ":"□ ")+"无敌模式");UOpusUI::SetText(Screen,"live-setting-loot",(ShowLoot?"✓ ":"□ ")+"显示地面掉落");
  if(UOpusUI::HasVisibleBinding(Screen,"hunter-currencies")){TArray<FOpusUIRow> CurrencyRows;FOpusUIRow ClearCurrency;ClearCurrency.Label="清空通货条件";ClearCurrency.Op="hunter-currency";for(auto E:UOpusData::Listings(Tower.Rules))if(E.Category=="currency"){auto R=Row(E,"hunter-currency");R.Label=(HunterCurrencies.Contains(E.Id)?"✓ ":"")+E.Name;CurrencyRows.Add(R);}UOpusUI::SetRows(Screen,"hunter-currencies",CurrencyRows);}
  for(int I=0;I<3;I++){FName Id=UOpusNative::Belt(Tower.Rules,I);FString Label=Id.IsNone()?"空槽":Id.ToString();for(auto E:Bag)if(E.Id==Id.ToString())Label=E.Name+" ×"+E.Count;UOpusUI::SetText(Screen,"suite-belt-"+I,Label);}
 }
 FString SlotName(FString Key) const {return Key=="weapon"?"武器":Key=="offhand"?"副手":Key=="helmet"?"头盔":Key=="chest"?"胸甲":Key=="gloves"?"手套":Key=="boots"?"靴子":Key=="belt"?"腰带":Key=="amulet"?"项链":Key=="ring"?"戒指":Key;}
 FOpusUIRow Row(FOpusEntry E,FString Op)
 {
  FOpusUIRow R;R.Label=E.Name;R.Kind=E.Kind;R.Rarity=E.Rarity;R.NewItem=NewItems!=nullptr&&NewItems.IsNew(E.Id);
  R.Detail=E.Description;R.Meta=(E.Tier>0?"T"+E.Tier+" · ":"")+SlotName(E.Slot);R.Op=Op;R.Value=E.Id;R.Icon=E.Icon;R.Count=E.Count;R.Tier=E.Tier;R.Equipped=E.Equipped;R.Slot=E.Slot;R.Badge=""+E.Price+" 灵石 · 购买";R.Selected=SelectedId==E.Id;return R;
 }
 FOpusUIRow BagRow(FOpusEntry E,FString Compartment){auto R=Row(E,"select");R.Count=DWOInventory::Count(E,Compartment);R.Equipped=DWOInventory::Worn(E,Compartment);R.Selected=SelectedId==E.Id&&SelectedCompartment==Compartment;return R;}
 void RefreshData(){for(int Part=0;Part<8;Part++)RefreshDataPart(Part);}
 void RefreshDataPart(int Part)
 {
  FCpuProfilerTraceScoped Scope(n"Opus_HUDDataPart");
  FString State=Screen.CurrentState;
  if(Part==0&&State=="character"){FString Choice=UOpusTalentBridge::Option(Screen,"battle_live_modal_page::battle-character-filter");if(!Choice.IsEmpty()&&Choice!=GearFilter){GearFilter=Choice;InventoryDrag.Cancel();ItemHover.Close();}}
  if(Part==0){Bag=UOpusData::Inventory(Tower.Rules);if(NewItems!=nullptr)NewItems.Observe(Bag);if(InventoryLayout!=nullptr)InventoryLayout.Reconcile();}TArray<FOpusUIRow> Rows,CharacterRows,Gear,Craft,Currencies,Omens;
  int Equipped=0,VisibleBag=0,OrdinaryCount=0;FOpusEntry Selected;bool Found=false;
  FString CharacterQuery=UOpusUI::Input(Screen,"field:characterQuery"),BagQuery=UOpusUI::Input(Screen,"field:bagQuery"),CraftQuery=UOpusUI::Input(Screen,"field:craftQuery");
  TArray<FOpusEntry> VisibleEntries;
  for(int I=0;I<Bag.Num();I++)
  {
   auto E=Bag[I];if(E.Id==SelectedId){Selected=E;Found=DWOInventory::Count(E,SelectedCompartment)>0;}
   int Stock=DWOInventory::Count(E,"stock");
   if(Stock>0){OrdinaryCount++;
    if(BagQuery.IsEmpty()||E.Name.Contains(BagQuery)||E.Description.Contains(BagQuery)){VisibleEntries.Add(E);VisibleBag++;}
    if(CharacterQuery.IsEmpty()||E.Name.Contains(CharacterQuery)||E.Description.Contains(CharacterQuery))CharacterRows.Add(BagRow(E,"stock"));
   }
   if(E.Kind=="equipment"){if(E.Equipped)Gear.Add(BagRow(E,"equipped"));if(CraftQuery.IsEmpty()||E.Name.Contains(CraftQuery)||E.Description.Contains(CraftQuery))Craft.Add(Row(E,"craft-select"));if(E.Equipped)Equipped++;}
   if(E.Category=="currency")Currencies.Add(Row(E,"currency"));if(E.Category=="omen")Omens.Add(Row(E,"omen"));
  }
  int Pages=InventoryLayout==nullptr?1:Math::Max(1,Math::CeilToInt(double(InventoryLayout.Cells.Num())/72));BagPage=Math::Clamp(BagPage,0,Pages-1);
  if(InventoryLayout!=nullptr){Rows=InventoryLayout.PageGrid(BagQuery,BagPage);CharacterRows=InventoryLayout.CharacterGrid(CharacterQuery);}
  if(Part==1){
  TArray<FOpusUIRow> Owned;for(auto E:Bag)Owned.Add(Row(E,"select"));UOpusUI::SetRows(Screen,"shop-owned",Owned);
  UOpusUI::SetRows(Screen,"bag-grid",Rows,8);UOpusUI::SetRows(Screen,"character-grid",CharacterRows,8);UOpusUI::SetText(Screen,"character-capacity",""+OrdinaryCount+" 项");
  TArray<FOpusUIRow> Worn;for(auto Slot:DWOInventory::Slots()){FOpusUIRow R;R.Enabled=false;R.Slot=Slot;for(auto E:Bag)if(E.Kind=="equipment"&&E.Equipped&&E.Slot==Slot){R=BagRow(E,"equipped");break;}Worn.Add(R);}
  UOpusUI::SetRows(Screen,"equipped-bag-grid",Worn,3);UOpusUI::SetText(Screen,"equipped-bag-count",""+Equipped+" / 9");
  UOpusUI::SetRows(Screen,"gear-list",Gear,3);UOpusUI::SetRows(Screen,"craft-bag",Craft,3);
  UOpusUI::SetText(Screen,"bag-count",""+OrdinaryCount+" 项");UOpusUI::SetText(Screen,"bag-page",""+(BagPage+1)+" / "+Pages);
  UOpusUI::SetText(Screen,"gear-count",""+Equipped+" / 9");
  UOpusUI::SetText(Screen,"suite-wallet",""+UOpusNative::Stones(Tower.Rules)+" 灵石");
  UOpusUI::SetText(Screen,"suite-bag-detail",Found?Selected.Name:"背包为空");
  UOpusUI::SetText(Screen,"live-equip-label",Found&&DWOInventory::Worn(Selected,SelectedCompartment)?"卸下":"装备 / 使用");
  UOpusUI::SetText(Screen,"bag-quantity",Found?"×"+DWOInventory::Count(Selected,SelectedCompartment):"");
  }
  if(Part==2){
  UOpusUI::SetText(Screen,"inspector-name",Found?Selected.Name:"请选择装备");UOpusUI::SetImage(Screen,"inspector-icon",Found?Selected.Icon:"");UOpusUI::SetText(Screen,"inspector-meta",Found?"T"+Selected.Tier+" · "+SlotName(Selected.Slot):"");
  TArray<FOpusUIRow> InfoRows;if(Found){TArray<FString> Lines;Selected.Description.ParseIntoArray(Lines,"\n");for(auto Line:Lines){FOpusUIRow Info;Info.Label=Line;Info.Op="none";InfoRows.Add(Info);}}UOpusUI::SetRows(Screen,"inspector-effects",InfoRows);
  UOpusUI::SetImage(Screen,"craft-icon",Found?Selected.Icon:"");UOpusUI::SetText(Screen,"craft-name",Found?Selected.Name:"请选择装备");UOpusUI::SetText(Screen,"craft-meta",Found?"T"+Selected.Tier+" · "+SlotName(Selected.Slot):"");
  UOpusUI::SetText(Screen,"craft-base","");
  TArray<FOpusUIRow> Details;for(auto E:UOpusData::EquipmentAffixes(Tower.Rules,SelectedId)){auto R=Row(E,"affix");R.Label=(E.Id==""+AffixIndex?"● ":"")+E.Name+" · T"+E.Tier;R.Detail=E.Description;Details.Add(R);}UOpusUI::SetRows(Screen,"craft-affixes",Details);UOpusUI::SetText(Screen,"bag-affix-count",""+Details.Num()+" 条随机词条");UOpusUI::SetText(Screen,"inspector-random-count",""+Details.Num());
  TArray<FOpusUIRow> Materials=Currencies;Materials.Append(Omens);UOpusUI::SetRows(Screen,"craft-materials",Materials);UOpusUI::SetRows(Screen,"inspector-materials",Currencies);UOpusUI::SetRows(Screen,"inspector-omens",Omens);
  UOpusUI::SetText(Screen,"craft-operation","通货："+CurrencyId+" · 预兆："+OmenId+" · 词条 "+(AffixIndex+1));
  UOpusUI::SetText(Screen,"craft-feedback",Tower.LastError);
  }
  if(Part==3&&UOpusUI::HasVisibleBinding(Screen,"picker-list")){
  TArray<FOpusUIRow> Choices;for(auto E:UOpusData::Skills(Tower.Rules)){auto R=Row(E,"assign");R.Equipped=E.Equipped&&E.Slot==""+UOpusNative::Selected(Tower.Rules);if(E.Equipped)R.Detail+=R.Equipped?" · 当前槽":" · 从槽 "+(String::Conv_StringToInt(E.Slot)+1)+" 移入";Choices.Add(R);}UOpusUI::SetRows(Screen,"picker-list",Choices);
  UOpusUI::SetText(Screen,"picker-slot","槽位 "+(UOpusNative::Selected(Tower.Rules)+1));UOpusUI::SetShown(Screen,"picker-empty",Choices.Num()==0);
  }
  if(Part==0)for(int I=0;I<4;I++){FString Label=UOpusData::SkillName(Tower.Rules,I);double Recharge=Tower.Hero.SelfSkills.SlotRemaining(I);UOpusUI::SetText(Screen,"live-skill-name-"+I,Recharge>0?Label+" "+Math::CeilToInt(Recharge)+"s":Label);UOpusUI::SetImage(Screen,"live-skill-icon-"+I,UOpusData::SkillIcon(Tower.Rules,I));UOpusUI::SetShown(Screen,"live-skill-icon-"+I,Label!="空槽");UOpusUI::SetShown(Screen,"live-skill-empty-"+I,Label=="空槽");UOpusUI::SetText(Screen,"loadout-"+I,Label);}
  if(Part==4&&(UOpusUI::HasVisibleBinding(Screen,"shop-stock")||UOpusUI::HasVisibleBinding(Screen,"shop-owned"))){
  TArray<FOpusUIRow> Offers;for(auto E:UOpusData::Listings(Tower.Rules))
  {FString ShopQuery=UOpusUI::Input(Screen,"field:shopQuery");if(!ShopQuery.IsEmpty()&&!E.Name.Contains(ShopQuery)&&!E.Description.Contains(ShopQuery))continue;if((ShopTab==0&&E.Kind!="skill")||(ShopTab==1&&E.Kind!="equipment")||(ShopTab==2&&E.Kind!="item")||(OnlyAffordable&&E.Price>UOpusNative::Stones(Tower.Rules)))continue;auto R=Row(E,"buy");R.Value=E.Key;R.Detail=""+E.Price+" 灵石 · "+E.Description;R.Enabled=E.Price<=UOpusNative::Stones(Tower.Rules);Offers.Add(R);}
  UOpusUI::SetRows(Screen,"shop-stock",Offers);
  }
  if(Part==5){
  TArray<FOpusUIRow> Ground;for(auto E:UOpusData::Ground(Tower.Rules)){if(!HunterAllows(E))continue;auto R=Row(E,"pickup");R.Value=E.Key;Ground.Add(R);}UOpusUI::SetRows(Screen,"ground-loot",Ground);
  TArray<FOpusUIRow> Stock;for(auto E:UOpusData::Merchant(Tower.Rules)){auto R=Row(E,"merchant");R.Value=E.Key;R.Detail=""+E.Price+" 灵石";R.Enabled=E.Price<=UOpusNative::Stones(Tower.Rules);Stock.Add(R);}UOpusUI::SetRows(Screen,"completion-offers",Stock);
  }
  if(Part==6){
  TArray<FOpusUIRow> Presets;for(auto E:UOpusData::Presets())Presets.Add(Row(E,"preset"));UOpusUI::SetRows(Screen,"bd-content",Presets);UOpusUI::SetText(Screen,"live-preset-count","可用构筑 "+Presets.Num());
  if(State=="builds"||State=="bd"||State=="bd-detail")RefreshBuilds();
  }
  if(Part==7)RefreshPanels();
  UOpusUI::SetText(Screen,"dps",""+int(UOpusData::Dps(Tower.Rules)));
  for(auto Enemy:Tower.Enemies)if(IsValid(Enemy)&&Enemy.Boss&&!Enemy.Dead){auto Pool=Tower.Rules.GetTargetPools(Enemy.EntityId);UOpusUI::SetProgress(Screen,"boss-health",Pool.MaxLife>0?Pool.Life/Pool.MaxLife:0);break;}
 }
}
