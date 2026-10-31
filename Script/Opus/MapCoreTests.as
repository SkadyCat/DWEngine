namespace DWOMapCoreTests
{
 bool Check(bool Condition,FString Message){if(!Condition)UOpusNative::WriteLog("MAP_CORE_TEST_FAIL "+Message);return Condition;}
 bool Reload(ADWOTowerGameMode Tower)
 {
  bool Ok=Check(UDWOTowerJourney::Get().MapCore==n"map_core_equipment_t10"&&Tower.Map.Tier==10&&Tower.Map.Packs==900&&Tower.Run.MonsterCount==191&&Tower.Run.Phase==EDWOTowerPhase::Ready,"saved core restored in a separate game process");
  if(Ok)UOpusNative::WriteLog("MAP_CORE_TEST_PASS DiskReload");return Ok;
 }
 void Run(ADWOTowerGameMode Tower)
 {
  if(UDWOTowerJourney::Get().Visit>1)
  {
   bool Ok=Reload(Tower)&&Check(Tower.Enemies.Num()==0,"retry remains ready with the configured core");
   if(Ok)UOpusNative::WriteLog("MAP_CORE_TEST_PASS RetryPreservesCore");UOpusNative::Quit(Ok);return;
  }
  auto Test=UDWOMapCoreTest::Create(Tower);Test.Initialize(Tower);
 }
}

class UDWOMapCoreTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 bool Done=false;
 bool Check(bool Condition,FString Message){if(DWOMapCoreTests::Check(Condition,Message))return true;Done=true;UOpusNative::Quit(false);return false;}
 void Initialize(ADWOTowerGameMode Value)
 {
  Tower=Value;auto Cores=UOpusData::MapCores(Tower.Rules);
  if(!Check(Cores.Num()==10,"all ten authored native map cores are available"))return;
  for(int I=0;I<Cores.Num();I++){FOpusMapData Data;FString Error;if(!Check(UOpusNative::MapCoreData(Tower.Rules,FName(Cores[I].Id),Data,Error)&&Data.Packs==I*100&&Data.Tier==I+1&&Data.LifeIncreased==0&&Data.DamageIncreased==0,"native aggregate tier "+(I+1)))return;}
  auto CoreButton=Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-map-instance::rs-map-core-equipped");
  auto T10=Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-map-core-picker-instance::rs-map-core-choice-9");
  if(!Check(CoreButton!=nullptr&&T10!=nullptr,"saved UICreator nodes are present"))return;
  FString Before=Tower.Rules.ExportPlayerState();Tower.Hud.Screen.HandleInvoke(CoreButton);
  if(!Check(Tower.Hud.Screen.CurrentState=="map-core-picker","actual equipped card opens the source picker state"))return;
  Tower.Hud.Screen.HandleInputKey(EKeys::Escape);
  if(!Check(Tower.Hud.Screen.CurrentState.StartsWith("selected-"),"Escape closes the map picker through the shared input filter"))return;
  Tower.Hud.Screen.ApplySceneState("builds");Tower.Hud.RefreshData();
  auto Family=Tower.Hud.Screen.FindSourceNode("battle_live_modal_page::battle-builds-family-0");
  if(!Check(Family!=nullptr&&Math::Abs(Family.Normal.PixelSize.X-T10.Normal.PixelSize.X)>1,"BD and map lists retain their own authored selection skins"))return;
  Tower.Hud.Screen.ApplySceneState("selected-1");Tower.Hud.Screen.HandleInvoke(CoreButton);
  Tower.Hud.Screen.HandleInvoke(T10);
  if(!Check(Tower.Hud.Screen.CurrentState.StartsWith("selected-")&&Tower.Map.Packs==900&&Tower.Map.Tier==10&&Tower.Run.MonsterCount==191,"actual source choice equips T10 and closes"))return;
  if(!Check(Tower.Rules.ExportPlayerState()==Before,"core configuration leaves the player inventory unchanged"))return;
  if(!Check(UOpusNative::ReadMapCore()=="map_core_equipment_t10","selected core persisted to the isolated verification profile"))return;
  if(!Check(!Tower.ApplyMapCore(n"missing")&&Tower.Map.Tier==10&&UDWOTowerJourney::Get().MapCore==n"map_core_equipment_t10","invalid IDs leave equipment unchanged"))return;
  Tower.LastError="";Tower.SetMonsterCount(300);if(!Check(Tower.Run.MonsterCount==300,"existing native encounter cap remains 300"))return;Tower.SetMonsterCount(20);
  UOpusNative::WriteLog("MAP_CORE_TEST_PASS NativeCatalogAndUICreatorActions");Tower.StartChallenge();
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Done||Tower==nullptr||Tower.Preparing||Tower.Enemies.Num()==0)return;
  if(!Check(Tower.Enemies.Num()==191&&Tower.Run.Wave==1,"T10 publishes all 191 monsters together"))return;
  int Alive=Tower.Enemies.Num();
  auto CoreButton=Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-map-instance::rs-map-core-equipped");
  auto T2=Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-map-core-picker-instance::rs-map-core-choice-1");
  Tower.Hud.Screen.HandleInvoke(CoreButton);Tower.Hud.Screen.HandleInvoke(T2);
  if(!Check(Tower.Map.Tier==2&&Tower.Run.MonsterCount==39&&Tower.Enemies.Num()==Alive,"live replacement preserves the published dungeon and configures the next start"))return;
  if(!Check(Tower.Run.EncounterCount()==39,"next dungeon start reads the new scale"))return;
  auto T10=Tower.Hud.Screen.FindSourceNode("hud-screen-instance::rs-map-core-picker-instance::rs-map-core-choice-9");
  Tower.Hud.Screen.HandleInvoke(CoreButton);Tower.Hud.Screen.HandleInvoke(T10);
  UOpusNative::WriteLog("MAP_CORE_TEST_PASS LiveWaveAndScaling");Done=true;Tower.Retry();
 }
}
