// Transient-only integration fixture. All clicks invoke the exported UICreator nodes.
class UDWOMapEventTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UDWOMapEventState Altar;
 UPROPERTY() UDWOMapEventState Fusion;
 UPROPERTY() UDWOMapEventState Reward;
 UPROPERTY() UDWOMapEventState Vault;
 double Clock=0,LastCast=-100,CapturedAt=0,PreviewOpenedAt=0;
 int Stage=0;
 FString Left,Right,AltarUid;
 bool Check(bool Good,FString Name){UOpusNative::WriteLog((Good?"EVENT_TEST_PASS ":"EVENT_TEST_FAIL ")+Name);if(!Good)UOpusNative::Quit(false);return Good;}
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 bool Click(FString Id){auto P=Tower.Events.Panel;if(P==nullptr)return Check(false,"missing event panel");auto N=P.FindSourceNode(Id);if(N==nullptr)N=P.FindSourceNode("craft-"+Id);if(N==nullptr)return Check(false,"missing exported node "+Id);P.HandleInvoke(N);return true;}
 FString Seed(FString Id,int SeedValue)
 {
  auto O=Cast<UOpusEventOffer>(NewObject(this,UOpusEventOffer));TArray<FName> Allowed;Allowed.Add(FName(Id));FString Error;TArray<int> Choice;Choice.Add(0);
  if(!O.Generate(Tower.Rules,Allowed,SeedValue,3,n"legendary",1,1,Error)||!O.Claim(Choice,Error)){Check(false,Error);return "";}return O.Entries()[0].Id;
 }
 int Index(UDWOMapEventState State){return Tower.Events.States.FindIndex(State);}
 bool Open(UDWOMapEventState State){Tower.Hero.SetActorLocation(State.Position+FVector(0,0,101));return Tower.Events.Open(Index(State));}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Tower==nullptr)return;Clock+=Dt;if(Clock>150){Check(false,"integration timeout stage="+Stage);return;}
  if(!Tower.Events.Planned||Tower.Events.States.Num()!=4||!Tower.Hero.Visual.Ready||!Tower.DropService.Ready)return;
  if(Stage==0)
  {
   for(auto S:Tower.Events.States){if(S.Definition.Kind=="altar")Altar=S;if(S.Definition.Kind=="fusion")Fusion=S;if(S.Definition.Kind=="choice")Reward=S;if(S.Definition.Kind=="vault")Vault=S;}
   if(!Check(Altar!=nullptr&&Fusion!=nullptr&&Reward!=nullptr&&Vault!=nullptr,"FourConnectedEvents"))return;
   Left=Seed("embers_echo_1_weapon",101);Right=Seed("myriad_swords_t1_weapon",102);AltarUid=Seed("embers_echo_3_weapon",103);
   if(UOpusNative::Flag("DWOEventCraftTest")){Altar.Currency=n"quad_affix_orb";if(!Check(Open(Altar),"CraftPanelOpens"))return;Stage=90;PreviewOpenedAt=Clock;return;}
   if(UOpusNative::Flag("DWOEventsPreview"))
   {
    Tower.Run.Phase=EDWOTowerPhase::Cleared;FString View=UOpusNative::StringOption("DWOEventView");
    if(View=="reward")Open(Reward);
    else if(View=="altar"){Altar.Currency=n"quad_affix_orb";Open(Altar);Tower.Events.Deposit(Altar,AltarUid);Tower.Events.Lock(Altar);Tower.Events.Refresh();}
    else{Open(Fusion);Tower.Events.DepositSide=0;Tower.Events.Deposit(Fusion,Left);Tower.Events.DepositSide=1;Tower.Events.Deposit(Fusion,Right);Tower.Events.Refresh();}
    if(View=="fusion-result")
    {
     bool Found=false;int L=UOpusMapEvents::FusionLineCount(Tower.Rules,Left)-1;int R=UOpusMapEvents::FusionLineCount(Tower.Rules,Right)-1;
     for(int SeedValue=1;SeedValue<1000;SeedValue++){FRandomStream Probe;Probe.Initialize(SeedValue);auto LI=Tower.Events.Draw(Probe,L+1);auto RI=Tower.Events.Draw(Probe,R+1);if(LI.Contains(L)&&RI.Contains(R)){Fusion.Random.Initialize(SeedValue);Found=true;break;}}
     if(!Check(Found,"PreviewSeedContainsTwoLegendaryLines"))return;Click("event-primary");Click("event-primary");if(!Check(Fusion.Offer!=nullptr&&Fusion.Offer.Entries()[0].FixedAffixCount==2,"PreviewTwoDifferentLegendaryLines"))return;
    }
    PreviewOpenedAt=Clock;Stage=99;return;
   }
   Altar.Currency=n"quad_affix_orb";if(!Check(Open(Altar)&&Tower.Events.Deposit(Altar,AltarUid),"AltarDeposit"))return;Click("event-primary");
   if(!Check(Altar.Locked&&Tower.Rules.IsPlayerItemLocked(AltarUid),"AltarLocksBackpackItem"))return;
   FString Error;if(!Check(!Tower.Rules.EquipPlayerItem(n"weapon",FName(AltarUid),Error)&&!Tower.Rules.DiscardPlayerEquipment(AltarUid,Error),"LockedGearCannotMove"))return;
   Tower.Events.Close();if(!Check(Open(Altar)&&Altar.Left==AltarUid&&Altar.Locked,"CloseReopenRetainsBinding"))return;
   for(int I=0;I<12;I++)Click("event-primary");if(!Check(Altar.Crafts==12,"UnlimitedCraftViaUICreator"))return;
   Click("event-secondary");if(!Check(Altar.Spent&&!Tower.Rules.IsPlayerItemLocked(AltarUid),"CollectKeepsEquipmentAndEndsAltar"))return;
   Stage=1;
  }
  if(Stage==1)
  {
   if(!Check(Open(Fusion),"FusionUIOpens"))return;Tower.Events.DepositSide=0;Tower.Events.Deposit(Fusion,Left);Tower.Events.DepositSide=1;
   if(!Check(!Tower.Events.Deposit(Fusion,Left)&&Tower.Events.Deposit(Fusion,Right),"FusionDistinctInputs"))return;
   Tower.Events.Refresh();Click("event-primary");if(!Check(Fusion.Locked&&Tower.Rules.IsPlayerItemLocked(Left)&&Tower.Rules.IsPlayerItemLocked(Right),"FusionLocksBoth"))return;
   Click("event-primary");bool Consumed=true;for(auto E:UOpusMapEvents::Equipment(Tower.Rules))if(E.Id==Left||E.Id==Right)Consumed=false;
   if(!Check(Fusion.Spent&&Fusion.Offer!=nullptr&&Fusion.Offer.Claimed&&Consumed,"FusionConsumesTwoGrantsOne"))return;
   auto NewItem=Fusion.Offer.Entries()[0];if(!Check(NewItem.Tier==3&&NewItem.Synthesized,"FusionPreservesLeftTier"))return;Click("event-secondary");
   Tower.Run.Phase=EDWOTowerPhase::Cleared;FName PreviousCore=UDWOTowerJourney::Get().MapCore;Tower.ApplyMapCore(n"map_core_equipment_t10",false);if(!Check(Open(Reward)&&Reward.Offer!=nullptr&&Reward.Offer.Entries().Num()==3,"ThreeLegendaryChoices"))return;
   if(!Check(Reward.Offer.Entries()[0].Tier==Tower.Events.TierSnapshot,"RewardTierFrozenAtMapEntry"))return;Tower.ApplyMapCore(PreviousCore,false);
   FString Candidate=Reward.Offer.Entries()[0].Id;Tower.Events.Close();Open(Reward);if(!Check(Reward.Offer.Entries()[0].Id==Candidate,"RewardCandidatesFrozen"))return;
   Click("event-select-1");Click("event-primary");if(!Check(Reward.Spent&&Reward.Offer.Claimed,"UICreatorChoosesOne"))return;
   if(!Check(Open(Vault),"VaultUIOpens"))return;Click("event-primary");if(!Check(Vault.GuardsStarted&&Tower.Preparing,"VaultSpawnsNativeGuards"))return;Stage=2;return;
  }
  if(Stage==2)
  {
   if(Tower.Preparing)return;int Count=0;for(auto E:Tower.Enemies)if(E.EventId==Vault.Token){Count++;E.Speed=0;}
   if(!Check(Count==Vault.Definition.Guards&&Tower.CanFight(),"VaultGuardsUseExistingCombat count="+Count+" paused="+Tower.Paused+" active="+Tower.Events.CombatActive()+" hud="+Tower.Hud.Screen.CurrentState))return;Stage=3;
  }
  if(Stage==3)
  {
   for(auto E:Tower.Enemies)if(IsValid(E)&&E.EventId==Vault.Token&&!E.Dead){if(Clock-LastCast>0.35){Tower.Hero.CastAt(E.ActorLocation-FVector(100,0,0),E.ActorLocation,100000);LastCast=Clock;}return;}
   if(!Vault.GuardsCleared)return;if(!Check(Tower.Run.Phase==EDWOTowerPhase::Cleared&&Open(Vault)&&Vault.Offer!=nullptr,"GuardDeathsUnlockVault"))return;
   Click("event-select-0");Click("event-primary");if(!Check(Vault.Spent&&Vault.Offer.Claimed,"VaultRewardClaimedOnce"))return;
   auto All=UOpusMapEvents::Equipment(Tower.Rules);FString Error;FString Before=Tower.Rules.ExportPlayerState();
   if(!Check(Tower.Rules.ImportPlayerState(Before,Error)&&UOpusMapEvents::Equipment(Tower.Rules).Num()==All.Num(),"EventItemsSaveReload"))return;
   UOpusNative::Quit(true);Stage=100;
  }
  // Hidden SetRows prototypes intentionally have no arranged Slate geometry.
  // CreateTemplate already waits for its saved textures; inspect the live panel geometry here.
  if(Stage==90&&Clock-PreviewOpenedAt>2&&UOpusHoverUI::AnchorRect(Tower.Events.Panel,Gameplay::GetPlayerController(0)).Z>0)
  {
   auto UI=Tower.Events.CraftUI;auto P=Tower.Events.Panel;auto PC=Gameplay::GetPlayerController(0);
   auto Bag=UOpusHoverUI::AnchorRect(P.FindSourceNode("craft-event-backpack"),PC);auto Slot=UOpusHoverUI::AnchorRect(P.FindSourceNode("craft-input-0"),PC);auto Currency=UOpusHoverUI::AnchorRect(P.FindSourceNode("craft-input-1"),PC);
   if(!Check(Bag.Z<Slot.X&&Bag.Z>Bag.X,"SeparateLeftBackpackRightAltar"))return;
   UDWOEventItemDrag Handle;for(auto D:UI.Drags)if(D.Uid==AltarUid&&D.From<0)Handle=D;
   if(!Check(Handle!=nullptr,"RealInventoryButtonDragDelegate"))return;Handle.Button.OnPressed.Broadcast();if(!Check(UI.DragUid==AltarUid,"ButtonPressBeginsDrag"))return;UI.Dragged=true;UI.Finish(FVector2D((Slot.X+Slot.Z)/2,(Slot.Y+Slot.W)/2));
   if(!Check(Altar.Left==AltarUid,"DropIntoEquipmentSocket"))return;
   UI.BeginDrag(AltarUid,0);UI.Dragged=true;UI.Finish(FVector2D((Bag.X+Bag.Z)/2,(Bag.Y+Bag.W)/2));if(!Check(Altar.Left.IsEmpty(),"DragBackToBackpack"))return;
   UI.BeginDrag(AltarUid);UI.Dragged=true;UI.Finish(FVector2D((Currency.X+Currency.Z)/2,(Currency.Y+Currency.W)/2));if(!Check(Altar.Left.IsEmpty(),"CurrencySocketRejectsEquipment"))return;
   UI.BeginDrag(AltarUid);UI.Dragged=true;UI.Finish(FVector2D((Slot.X+Slot.Z)/2,(Slot.Y+Slot.W)/2));Click("event-primary");
   UI.BeginDrag(AltarUid,0);if(!Check(Altar.Locked&&UI.DragUid.IsEmpty(),"LockedEquipmentCannotDrag"))return;Stage=91;PreviewOpenedAt=Clock;return;
  }
  if(Stage==91&&Clock-PreviewOpenedAt>1){FString Capture=UOpusNative::StringOption("DWOEventCapture");if(!Capture.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Capture+" -nosuffix");CapturedAt=Clock;Stage=98;return;}
  if(Stage==99&&Clock-PreviewOpenedAt>2&&UOpusHoverUI::AnchorRect(Tower.Events.Panel,Gameplay::GetPlayerController(0)).Z>0){FString Capture=UOpusNative::StringOption("DWOEventCapture");if(!Capture.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Capture+" -nosuffix");CapturedAt=Clock;Stage=98;return;}
  if(Stage==98&&Clock-CapturedAt>2)UOpusNative::Quit(true);
 }
}
