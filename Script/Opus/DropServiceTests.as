// End-to-end HTTP test. It uses the real native combat/death/drop path.
class UDWODropServiceTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Target;
 int Stage=0,FirstRequests=0;
 double Clock=0,LastCast=-10;
 bool Check(bool Good,FString Message){if(!Good){UOpusNative::WriteLog("DROP_TEST_FAIL "+Message);UOpusNative::Quit(false);}return Good;}
 void Initialize(ADWOTowerGameMode Value)
 {
  Tower=Value;
  if(!Check(!Tower.DropService.Offline,"online test cannot bypass HTTP"))return;
  if(UDWOTowerJourney::Get().Visit>1){Stage=6;return;}
  Tower.StartChallenge();
  if(!Check(Tower.Run.Phase==EDWOTowerPhase::Ready&&Tower.Enemies.Num()==0,"map cannot start before the HTTP pool arrives"))return;
  Tower.StartAfterDropPool=false;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Tower==nullptr)return;Clock+=DeltaSeconds;
  if(Clock>100){Check(false,"HTTP/native integration timed out");return;}
  if(UOpusNative::Flag("DWODropExpectFailure"))
  {
   if(!Tower.DropService.Failed)return;
   if(!Check(!Tower.DropService.Ready&&Tower.Enemies.Num()==0&&Tower.Run.Phase==EDWOTowerPhase::Ready,"failed service does not start an offline tradable run"))return;
   UOpusNative::WriteLog("DROP_TEST_PASS ServiceUnavailableFailsClosed requests="+Tower.DropService.Requests);UOpusNative::Quit(true);return;
  }
  if(Tower.DropService.Failed){Check(false,Tower.DropService.Error);return;}
  if(Stage==6)
  {
   if(!Tower.DropService.Ready)return;
   if(!Check(Tower.DropService.Requests>=1&&Tower.Run.Phase==EDWOTowerPhase::Ready,"new map instance requests its own pool"))return;
   UOpusNative::WriteLog("DROP_TEST_PASS MapReloadRequestsAgain");UOpusNative::Quit(true);return;
  }
  if(Stage==0)
  {
   if(!Tower.DropService.Ready)return;
   if(!Check(Tower.DropService.EquipmentCount>0&&Tower.DropService.ItemCount>0&&Tower.DropService.PoolVersion.Len()==64,"backend pool was validated and applied"))return;
   if(UOpusNative::Flag("DWODropSubset")&&!Check(Tower.DropService.EquipmentCount==1&&Tower.DropService.ItemCount==1,"server override constrains the native pool"))return;
   FirstRequests=Tower.DropService.Requests;
   if(!Check(Tower.ApplyMapCore(n"map_core_equipment_t2")&&!Tower.DropService.Ready,"changing the map core invalidates the previous pool"))return;
   UOpusNative::WriteLog("DROP_TEST_PASS InitialPoolAndCoreInvalidation");Stage=1;return;
  }
  if(Stage==1)
  {
   if(!Tower.DropService.Ready)return;
   if(!Check(Tower.DropService.Requests>FirstRequests&&Tower.Map.Tier==2,"new core fetched a fresh matching T2 pool"))return;
   Tower.StartChallenge();Stage=2;return;
  }
  if(Stage==2)
  {
   if(Tower.Preparing||!Tower.Dungeon.Ready||!Tower.Hero.Visual.Ready||Tower.Enemies.Num()==0)return;
   Tower.SpawnEncounter(EDWOTowerCommand::Boss);Stage=3;return;
  }
  if(Stage==3)
  {
   if(Tower.Preparing)return;Target=Tower.Enemies.Last();for(auto E:Tower.Enemies)E.Speed=0;Stage=4;
  }
  if(Stage==4)
  {
   if(!Target.Dead){if(Clock-LastCast>1&&Tower.Hero.CastAt(Target.ActorLocation-FVector(60,0,0),Target.ActorLocation,100000))LastCast=Clock;return;}
   auto Ground=UOpusData::Ground(Tower.Rules);
   if(!Check(Ground.Num()>=2,"native boss death still awards equipment and an item"))return;
   bool Equipment=false,Item=false;
   for(auto E:Ground)
   {
    if(E.Kind=="equipment"){Equipment=true;if(UOpusNative::Flag("DWODropSubset")&&!Check(E.Icon.Contains("frostthorn_staff"),"native equipment selected from the server pool"))return;}
    if(E.Kind=="item"){Item=true;if(UOpusNative::Flag("DWODropSubset")&&!Check(E.Id=="chaos_orb","native item selected from the server pool"))return;}
   }
   if(!Check(Equipment&&Item,"both rule-engine drop streams remain active"))return;
   UOpusNative::WriteLog("DROP_TEST_PASS NativeBossUsesServerPool");Stage=5;Tower.Retry(false);return;
  }
 }
}
