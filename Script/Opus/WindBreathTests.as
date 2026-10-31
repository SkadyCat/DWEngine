// Actual native damage, equipment, aura UI and AS expanding wave; transient only.
class UDWOWindBreathTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TArray<ADWOEnemy> Targets;
 TArray<double> Before;
 double Clock=0,Age=0,WaveBegan=0;
 int Stage=0,Emitted=0,WindFrames=0;
 bool Done=false,ShotDone=false;
 bool Check(bool Good,FString Why){if(Good)return true;Done=true;UOpusNative::WriteLog("WIND_BREATH_FAIL "+Why);UOpusNative::Quit(false);return false;}
 void Pass(FString Name){UOpusNative::WriteLog("WIND_BREATH_PASS "+Name);}
 void Initialize(ADWOTowerGameMode T){Tower=T;T.Hero.ControlProbe=true;T.Rules.SetPlayerGod(true);T.StartChallenge();}
 bool Equip(FName Base,FName Slot)
 {
  auto R=Tower.Rules;FString Error;TArray<FString> Old;for(auto E:UOpusData::Inventory(R))Old.Add(E.Id);
  if(!Check(R.BuyPlayerListing("equipment:"+Base,Error),"buy "+Base+" "+Error))return false;
  for(auto E:UOpusData::Inventory(R))if(E.Kind=="equipment"&&!Old.Contains(E.Id))
  {
   FString Uid=E.Id;if(!E.Identified&&!Check(R.IdentifyPlayerEquipment(FName(E.Id),Uid,Error),Error))return false;
   return Check(R.EquipPlayerItem(Slot,FName(Uid),Error),Error);
  }
  return Check(false,"no granted item");
 }
 void Shot(FString Name){if(UOpusNative::Flag("DWOWindBreathVisual"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/WindBreath/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Clock+=Dt;Age+=Dt;
  if(Clock>100){Check(false,"timeout stage="+Stage);return;}
  auto R=Tower.Rules;auto H=Tower.Hero;auto A=H.ReactiveAuras;auto G=Tower.Recent;FString Error;
  if(Stage==0)
  {
   for(auto E:Tower.Enemies)if(IsValid(E)){E.SetActorTickEnabled(false);if(Targets.Num()<4&&!Targets.Contains(E))Targets.Add(E);}
   if(Tower.Preparing||!H.Visual.Ready||Targets.Num()<4||Age<3)return;
   if(!Equip(n"wind_breath_ring",n"ring"))return;
   G.SyncAuras();bool Found=false;for(auto C:G.Choices)if(C.Id==n"wind_breath")Found=C.Name=="风之息";
   if(!Check(Found&&G.EquipAura(0,n"wind_breath"),"owned nonweapon aura is available in the actual UICreator panel"))return;
   for(auto Entry:UOpusData::AffixPool(R))if(!Check(Entry.Id!="hit_retaliation_nova","intrinsic retaliation does not roll independently"))return;
   for(auto Entry:UOpusData::Skills(R))if(!Check(Entry.Id!="wind_breath","aura stays outside the four active skill slots"))return;
   int Was=A.Emitted;H.CharacterEvents.TestHit(10);if(!Check(A.Emitted==Was,"disabled aura does not reflect"))return;
   G.Toggle(0);if(!Check(G.Enabled[0]&&H.CharacterEvents.SheetStat(n"retaliation.hit_percent")==80&&UOpusRecentGameplay::ManaReservation(R)==50,"composed 80% and existing 50% reservation"))return;
   if(!Check(G.UI.Open("auras")&&G.UI.Panel!=nullptr,"open real saved UICreator aura panel"))return;
   Pass("AcquisitionAssignmentReservation");Stage=1;Age=0;return;
  }
  if(Stage==1)
  {
   if(Age<3||!UOpusWorldUI::VisualReady(G.UI.Panel))return;
   if(!Check(A.WindSystem!=nullptr,"owned aura preloads its wind resource before the first hit"))return;
   Shot("aura-panel");Stage=11;Age=0;return;
  }
  if(Stage==11)
  {
   if(Age<.15)return;G.UI.Close();
   // Four cardinal targets ensure the effect is radial; last target is outside.
   FVector Center=H.CombatLocation();
   for(int I=0;I<Targets.Num();I++)
   {
    double Angle=I*1.5707963267948966;double Distance=I==3?800:200+I*100;
    Targets[I].SetActorLocation(Center+FVector(Math::Cos(Angle)*Distance,Math::Sin(Angle)*Distance,0));
    auto P=R.GetTargetPools(Targets[I].EntityId);
    R.UpdateActorTarget(Targets[I],Targets[I].EntityId,1,float32(P.Life+P.Shield),float32(P.MaxLife+P.MaxShield),35);
   }
   Stage=2;Age=0;return;
  }
  if(Stage==2)
  {
   if(Age<.15)return;for(auto E:Targets)Before.Add(R.GetTargetPools(E.EntityId).Life+R.GetTargetPools(E.EntityId).Shield);
   Emitted=A.Emitted;H.CharacterEvents.TestHit(100);
   if(!Check(A.Emitted==Emitted+1&&Math::Abs(A.LastOriginal-100)<.001&&Math::Abs(A.LastReflected-80)<.001,"original incoming100 produces reflected80"))return;
   if(!Check(A.Waves.Num()>0&&A.Waves[0].Victims.Num()==0,"wind has a moving front, no instant full-radius damage"))return;
   WaveBegan=H.CharacterEvents.SimulationClock();
   Stage=3;Age=0;return;
  }
  if(Stage==3)
  {
   double PulseAge=H.CharacterEvents.SimulationClock()-WaveBegan;
   if(PulseAge>.12&&WindFrames==0){WindFrames=1;Shot("wind-early");}
   if(PulseAge>.3&&WindFrames==1){WindFrames=2;Shot("radial-wind");}
   if(PulseAge>.46&&WindFrames==2){WindFrames=3;Shot("wind-late");}
   if(PulseAge<1)return;
   for(int I=0;I<Targets.Num();I++)
   {
    auto P=R.GetTargetPools(Targets[I].EntityId);double Lost=Before[I]-P.Life-P.Shield;
    if(!Check(I==3?Math::Abs(Lost)<.01:Math::Abs(Lost-Math::Min(Before[I],80.0))<.05,"cardinal target "+I+" damage="+Lost))return;
   }
   if(!Check(A.Waves.Num()==0,"wind actor and Niagara expire, live="+A.Waves.Num()))return;
   Pass("RadialExpansionOncePerEnemyAndCleanup");
   int Was=A.Emitted;H.CharacterEvents.TestHit(1,true);if(!Check(A.Emitted==Was,"DOT never reflects"))return;
   G.Toggle(0);H.CharacterEvents.TestHit(1);if(!Check(A.Emitted==Was,"turning off stops reaction"))return;
   G.Toggle(0);R.EquipPlayerItem(n"ring",FName(),Error);G.SyncAuras();H.CharacterEvents.TestHit(1);
   if(!Check(A.Emitted==Was&&G.AuraSlots[0].IsNone()&&UOpusRecentGameplay::ManaReservation(R)==0,"unequip revokes reaction and reservation"))return;
   Pass("DotDisabledUnequipped");Done=true;UOpusNative::Quit(true);
  }
 }
}
