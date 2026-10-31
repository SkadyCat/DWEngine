class UDWOPlayerEffectsTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0,Casts=0;double Age=0,Wait=0,Before=0,EffectAt=0;bool Done=false;
 FString LayerId;
 bool VisibleIcon(UOpusWorldScreen Screen){auto N=Screen.FindSourceNode("rs-effect-icon");if(N==nullptr)return false;for(auto W:N.GetAllChildren())if(Cast<UImage>(W)!=nullptr&&W.IsVisible())return true;return false;}
 bool Check(bool Good,FString Message){if(Good)return true;Done=true;UOpusNative::WriteLog("EFFECTS_TEST_FAIL "+Message);UOpusNative::Quit(false);return false;}
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 void Next(int S){Stage=S;Age=0;UOpusNative::WriteLog("EFFECTS_TEST_STAGE "+S);}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  // The real encounter allows up to 90 seconds to stream models before combat.
  if(Done||Tower==nullptr)return;Age+=Dt;Wait+=Dt;if(Wait>120){Check(false,"timeout stage="+Stage);return;}
  auto P=Tower.Hud.PlayerEffects;if(P==nullptr)return;
  if(Stage==0)
  {
   if(!Tower.Dungeon.Ready||!Tower.DropService.Ready)return;
   if(!Check(Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-player-effects-anchor")!=nullptr,"saved complete HUD includes effect anchor"))return;
   FString Error;bool Applied=UOpusNative::ApplyPreset(Tower.Rules,n"royal_treasury",Error);if(!Check(Applied,"royal preset "+Error))return;
   Tower.Rules.SetPlayerGod(true);Tower.StartChallenge();Next(1);return;
  }
  if(Stage==1)
  {
   if(Tower.Preparing||Age<.3+Casts*.5||Tower.Rules.GetPlayerPools().Cooldown>0)return;auto H=Tower.Hero;
   if(Tower.Rules.CastBuildPower(UOpusNative::ActivePlan(Tower.Rules),H.OwnerSlot,H.CombatLocation(),FVector(1,0,0),30)>=0)Casts++;
   if(Casts<3)return;
   auto Pools=Tower.Rules.GetPlayerPools();for(int I=0;I<3;I++)Tower.Rules.ApplyStatus(Pools.TargetId,n"poison",1,2);
   EffectAt=UOpusPlayerEffects::Clock(Tower.Rules);Next(2);return;
  }
  if(Stage==2)
  {
   if(UOpusPlayerEffects::Clock(Tower.Rules)-EffectAt<.9)return;
   if(!Check(UOpusPlayerEffects::VerifySnapshot(Tower.Rules),"projection is read-only"))return;
   auto Rows=UOpusPlayerEffects::Snapshot(Tower.Rules);bool Skill=false,Poison=false;for(auto E:Rows){if(E.Id.StartsWith("layers:")){Skill=E.Stacks==3&&E.Cap>3&&E.Icon.Contains("royal_treasury.png")&&E.Timer=="下次减层";LayerId=E.Id;}if(E.Id=="status:poison")Poison=E.Stacks==3&&E.Icon.Contains("status/poison.png");}
   if(!Check(Skill&&Poison&&P.Icons.Num()>=2,"skill icon, live layers and status icon"))return;
   if(!Check(VisibleIcon(P.Icons[LayerId])&&VisibleIcon(P.Icons["status:poison"]),"actual skill and status image resources are visible"))return;
   if(!Check(P.Icons["status:poison"].FillValues["player-effect:progress"]<.8,"countdown advances from the simulation clock"))return;
   auto Icon=P.Icons[LayerId];auto Node=Cast<UOpusActionNode>(Icon.FindSourceNode("rs-effect-hover"));UOpusUI::HoverEvents(Icon).Entered.Broadcast(Node);
   Tower.SetPaused(true);Before=UOpusPlayerEffects::Clock(Tower.Rules);Next(3);return;
  }
  if(Stage==3)
  {
   if(Age<.4)return;
   if(!Check(Math::Abs(UOpusPlayerEffects::Clock(Tower.Rules)-Before)<.001&&P.Tooltip!=nullptr,"pause freezes clock and hover is native"))return;
   for(auto Pair:P.Icons)if(!Check(Pair.Value.FillValues.Contains("player-effect:progress"),"native animated countdown installed"))return;
   if(UOpusNative::Flag("DWOEffectsVisual"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/FullHUD/player-effects.png -nosuffix");
   Next(4);return;
  }
  if(Stage==4){if(Age<.4)return;P.Enter(Cast<UOpusActionNode>(P.Icons["status:poison"].FindSourceNode("rs-effect-hover")));Before=UOpusPlayerEffects::Clock(Tower.Rules);Tower.SetPaused(false);Next(5);return;}
  if(Stage==5)
  {
   if(UOpusPlayerEffects::Clock(Tower.Rules)-Before<2.3)return;bool Expired=true;for(auto E:UOpusPlayerEffects::Snapshot(Tower.Rules))if(E.Id=="status:poison")Expired=false;
   if(!Check(Expired&&!P.Icons.Contains("status:poison"),"expired status removes its native icon"))return;
   if(!Check(P.Tooltip==nullptr,"expired hovered status closes its tooltip"))return;
   Done=true;UOpusNative::WriteLog("EFFECTS_TEST_PASS NativeHUD Icons Countdown Hover Pause Expiry ReadOnly");UOpusNative::Quit(true);
  }
 }
}
