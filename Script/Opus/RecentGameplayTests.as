class UDWORecentGameplayTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 bool Done=false;double Clock=0,Age=0;int Stage=0;
 void Initialize(ADWOTowerGameMode T){Tower=T;if(UDWOTowerJourney::Get().Hideout)Stage=20;}
 bool Check(bool Good,FString What){if(Good)return true;Done=true;UOpusNative::WriteLog("RECENT_TEST_FAIL "+What);UOpusNative::Quit(false);return false;}
 bool Equip(FName Base,FName Slot)
 {
  auto R=Tower.Rules;FString Error;TArray<FString> Before;for(auto E:UOpusData::Inventory(R))Before.Add(E.Id);
  bool Bought=R.BuyPlayerListing("equipment:"+Base,Error);if(!Check(Bought,"acquire "+Base+" "+Error))return false;
  for(auto E:UOpusData::Inventory(R))if(E.Kind=="equipment"&&!Before.Contains(E.Id))
  {
   FString Uid=E.Id;if(!E.Identified){bool Identified=R.IdentifyPlayerEquipment(FName(E.Id),Uid,Error);if(!Check(Identified,"identify "+Error))return false;}
   bool Equipped=R.EquipPlayerItem(Slot,FName(Uid),Error);return Check(Equipped,"equip instance "+Base+" "+Error);
  }
  return Check(false,"no acquired equipment instance "+Base);
 }
 void Shot(FString Name){System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/UEIntegration/"+Name+".png -nosuffix");}
 bool RewardAndStorage()
 {
  auto G=Tower.Recent;auto R=Tower.Rules;FString Error;
  auto Claim=G.UI.Panel.FindSourceNode("quest-claim-1");if(!Check(Claim!=nullptr,"authored claim action"))return false;G.UI.Panel.OnSourceAction.Broadcast(Claim);
  if(!Check(G.State.RewardClaimed&&G.State.ClaimedChoice==1&&!G.Claim(0),"exactly one persisted choice through authored UI"))return false;
  FOpusGameplayState Decoded;FString Snapshot=Tower.MapItems.Snapshot();if(!Check(UOpusGameplayStore::Decode(Snapshot,Decoded,Error)&&Decoded.RewardClaimed&&Decoded.RewardCandidates==G.State.RewardCandidates&&Decoded.AuraSlots==G.AuraSlots,"single complete snapshot roundtrip"))return false;
  R.BuyPlayerListing("item:chaos_orb",Error);int Count=0;for(auto E:UOpusData::Inventory(R))if(E.Id=="chaos_orb")Count=E.Count;
  FOpusStoredStack Stack;if(!Check(Count>0&&UOpusGameplayStore::Store(R,"item:chaos_orb",1,Stack,Error),"real inventory store "+Error))return false;
  int After=0;for(auto E:UOpusData::Inventory(R))if(E.Id=="chaos_orb")After=E.Count;if(!Check(After==Count-1&&Stack.Count==1&&UOpusGameplayStore::Entry(R,Stack.Payload).Count==1,"storage conserves count"))return false;
  if(!Check(UOpusGameplayStore::Restore(R,Stack,Error),"real inventory withdrawal "+Error))return false;
  After=0;for(auto E:UOpusData::Inventory(R))if(E.Id=="chaos_orb")After=E.Count;if(!Check(After==Count,"withdrawal restores count"))return false;
  auto Q=UOpusGameplayStore::Entry(R,G.State.RewardCandidates[1]);FOpusStoredStack Gear;
  if(!Check(UOpusGameplayStore::Store(R,Q.Key,1,Gear,Error)&&UOpusGameplayStore::Entry(R,Gear.Payload).Id==Q.Id&&UOpusGameplayStore::Restore(R,Gear,Error),"equipment UID and rolled lines roundtrip "+Error))return false;
  return true;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done)return;Clock+=Dt;if(Clock>120){Check(false,"timeout");return;}if(Tower==nullptr||!Tower.Dungeon.Ready||Tower.Hud.Screen==nullptr||Clock<2)return;
  auto G=Tower.Recent;auto R=Tower.Rules;FString Error;Age+=Dt;
  if(Stage==20)
  {
   if(!Tower.Hero.Visual.Ready)return;
   if(!Check(G.State.RewardClaimed&&G.State.Stash.Num()==1&&G.State.Stash[0].Cell==7&&G.State.Goods.Num()==1&&G.State.Goods[0].Cell==3,"level reload restores quest and complete positioned storage"))return;
   TArray<ADWOHideoutRoom> Rooms;GetAllActorsOfClass(Rooms);if(!Check(Rooms.Num()==1&&Rooms[0].OriginalScene!=nullptr&&Rooms[0].Decoration!=nullptr,"original hideout geometry in separate world"))return;
   G.UI.Open("hideout");Stage=21;Age=0;return;
  }
  if(Stage>0)
  {
   if(Age<.6)return;
   if(!UOpusWorldUI::VisualReady(G.UI.Panel)){if(Age>8)Check(false,"native UI surface failed to arrange stage="+Stage);return;}
   if(Stage==1){Shot("quest-reward");Stage=11;Age=0;return;}
   if(Stage==11){if(!RewardAndStorage())return;G.UI.Close();G.UI.Open("auras");Stage=2;Age=0;return;}
   if(Stage==2){Shot("auras");Stage=12;Age=0;return;}
   if(Stage==12){G.UI.Close();G.UI.Open("hideout");Stage=3;Age=0;return;}
   if(Stage==3){Shot("storage");Stage=13;Age=0;return;}
   if(Stage==21){Shot("hideout-storage");Stage=22;Age=0;return;}
   if(Stage==22){
    bool Restored=UOpusGameplayStore::Restore(R,G.State.Stash[0],Error)&&UOpusGameplayStore::Restore(R,G.State.Goods[0],Error);if(!Check(Restored,"stored complete payloads remain withdrawable after world reload "+Error))return;
    G.UI.Close();Done=true;UOpusNative::WriteLog("RECENT_TEST_PASS AuraReservation Unequip FirstFloorReward NoReroll SingleChoice Snapshot StorageConservation EquipmentUid UICreator DragHandlers HideoutTravel PositionedStash LocalGoods");UOpusNative::Quit(true);return;
   }
   if(!Check(G.UI.Rows.Num()==12&&G.UI.Drags.Num()==96,"authored inventory cells and real drag handlers"))return;
   auto Journey=UDWOTowerJourney::Get();Journey.Hideout=true;
   int Count=G.UI.Bag.Num();if(!Check(Count>=2,"two real storage candidates"))return;
   G.UI.StoreKind="stash";G.UI.Move("bag",0,7);if(!Check(G.State.Stash.Num()==1&&G.State.Stash[0].Cell==7,"actual warehouse UI transfer"))return;
   G.UI.StoreKind="shop";G.UI.Move("bag",0,3);if(!Check(G.State.Goods.Num()==1&&G.State.Goods[0].Cell==3,"actual local listing UI transfer"))return;
   G.UI.Close();Done=true;Tower.Travel(true);return;
  }
  Done=true;
  auto Probe=G.UI.Create("aura-panel",150,false);if(!Check(Probe!=nullptr&&Probe.SourceDocument!=nullptr,"saved aura template"))return;
  if(!Equip(n"aura_armour_ring",n"ring"))return;
  if(!Equip(n"aura_fire_amulet",n"amulet"))return;
  G.SyncAuras();if(!Check(G.EquipAura(0,n"aura_armour")&&G.EquipAura(1,n"aura_fire"),"aura assignment"))return;
  double Mana=R.GetPlayerPools().Mana;
  G.Toggle(0);if(!Check(G.Enabled[0]&&UOpusRecentGameplay::ManaReservation(R)==50&&R.GetPlayerPools().Mana<=UOpusRecentGameplay::UsableMana(R)+.001,"50% reservation"))return;
  G.Toggle(1);if(!Check(G.Enabled[1]&&UOpusRecentGameplay::ManaReservation(R)==100&&R.GetPlayerPools().Mana==0,"additive full reservation"))return;
  G.Toggle(0);G.Toggle(1);if(!Check(R.GetPlayerPools().Mana==0&&UOpusRecentGameplay::ManaReservation(R)==0,"unsealing never refills mana"))return;
  G.Toggle(0);R.EquipPlayerItem(n"ring",FName(),Error);G.SyncAuras();if(!Check(G.AuraSlots[0].IsNone()&&!G.Enabled[0]&&UOpusRecentGameplay::ManaReservation(R)==0,"unequip revokes aura immediately"))return;
  auto Before=G.State;Tower.Run.Floor=1;Tower.Run.Phase=EDWOTowerPhase::Cleared;G.FirstFloorCleared();
  if(!Check(G.State.FirstFloorComplete&&G.State.RewardCandidates.Num()==3,"first floor full candidates"))return;
  TArray<FString> Ids;for(auto J:G.State.RewardCandidates){auto E=UOpusGameplayStore::Entry(R,J);if(!Check(E.Tier==1&&E.Slot=="weapon"&&!Ids.Contains(E.Icon),"distinct ordinary T1 weapon rolls"))return;Ids.Add(E.Icon);}
  FString Frozen=G.State.RewardCandidates[0];G.FirstFloorCleared();if(!Check(Frozen==G.State.RewardCandidates[0],"closing/repeating cannot reroll reward"))return;
  Tower.Hud.LastPhase=EDWOTowerPhase::Cleared;G.UI.Open("quests");if(!Check(G.UI.Panel!=nullptr,"open actual reward UI"))return;Done=false;Stage=1;Age=0;
 }
}
