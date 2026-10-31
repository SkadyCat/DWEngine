struct FDWOPreparedMonster
{
 FOpusMonsterData Data;
 FName Rarity;
 FVector Position;
 FString Appearance;
 bool Boss=false;
 FString EventId;
 TArray<FOpusSkillData> Skills;
}
class ADWOController : APlayerController
{
 default bShowMouseCursor=true;
}
class ADWOTowerGameMode : AGameModeBase
{
 default PrimaryActorTick.bStartWithTickEnabled=true;
 default DefaultPawnClass=ADWOHero;
 default PlayerControllerClass=ADWOController;
 default HUDClass=nullptr;
 UPROPERTY(DefaultComponent) UDWOSvgHUD Hud;
 UPROPERTY(DefaultComponent) UDWOWorldOverlays WorldUI;
 UPROPERTY(DefaultComponent) UOpusDropService DropService;
 UPROPERTY(DefaultComponent) UDWOMapEvents Events;
 UPROPERTY(DefaultComponent) UDWOMapItems MapItems;
 UPROPERTY(DefaultComponent) UDWORecentGameplay Recent;
 UPROPERTY(DefaultComponent) UDWOStudioGameplay Studio;
 bool StartAfterDropPool=false;
 UPROPERTY() UDWOLootTest LootTest;
 UPROPERTY() UDWOMonsterHealthHUD MonsterHealthHUD;
 UPROPERTY() UDWOMonsterHealthTest MonsterHealthTest;
 UPROPERTY() UDWOEntityOverlayTest EntityOverlayTest;
 UPROPERTY() UDWOSvgContractTest SvgTest;
 UPROPERTY() UDWOFrameTest FrameTest;
 UPROPERTY() UDWOHeroTest HeroTest;
 UPROPERTY() bool HudKeyConsumed=false;
 UPROPERTY() FDWOTowerRun Run;
 UPROPERTY() UDWRuleWorldSubsystem Rules;
 UPROPERTY() ADWOHero Hero;
 UPROPERTY() TArray<ADWOEnemy> Enemies;
 UPROPERTY() bool Paused=false;
 UPROPERTY() bool Preparing=false;
 TArray<FDWOPreparedMonster> PendingMonsters;
 TArray<FString> PendingAssets;
 int PreparedIndex=0,PreparationFrames=0;
 double PreparationSeconds=0;
 bool ResetVerified=false;
 UPROPERTY() FString LastError;
 UPROPERTY() UDWODungeon Dungeon;
 UPROPERTY() int Kills=0;
 FRandomStream Random;
 FRandomStream AppearanceRandom;
 UPROPERTY() UDWMonsterLineageSettings Lineages;
 FDWMonsterLineageRule AppearanceRule;
 FOpusMapData Map;
 FString Inherited;
 double SaveAt=5,LogAt=0;
 bool Traveling=false;
 int SmokeSeconds=0;
 double CaptureClock=0;bool Captured=false;
 int NextId=0x10000000;
 bool HoverViewShown=false;
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  SetActorTickEnabled(true);
  if(UOpusNative::Flag("DWOTransient"))UOpusNative::WriteLog("TowerHostTick enabled="+IsActorTickEnabled());
  SetTickGroup(ETickingGroup::TG_PostUpdateWork);
  if(UOpusNative::Flag("DWOFlowTest")){bool Ok=DWOTowerTests::Run();UOpusNative::Quit(Ok);return;}
  // Old explicit Entry launchers still arrive in the new scene.
  bool InHideout=UDWOTowerJourney::Get().Hideout;
  bool HeroArena=InHideout||(UOpusNative::Flag("DWOHeroArena")&&UOpusNative::Flag("DWOTransient"));
  if(!HeroArena&&UOpusDungeon::Generator(this)==nullptr){Traveling=true;Gameplay::OpenLevel(n"/Game/ClaudeDungeon/Maps/L_OpusDungeon",true,"game=/Script/Angelscript.DWOTowerGameMode");return;}
  Rules=UOpusNative::GetRules(this);if(Rules==nullptr){Fail("Missing DWRules world");return;}
  FString Error;if(!Rules.LoadRuleLibrary("RulesTower",Error)){Fail(Error);return;}
  Hero=Cast<ADWOHero>(Gameplay::GetPlayerPawn(0));
  if(Hero==nullptr){Hero=Cast<ADWOHero>(SpawnActor(ADWOHero,FVector(0,0,100)));auto PC=Gameplay::GetPlayerController(0);if(PC!=nullptr)PC.Possess(Hero);}
  UOpusNative::SetGeometry(Rules,FOpusSweep(this,n"SweepRoom"));
  Hero.Rules=Rules;Hero.Tower=this;
  Dungeon=Cast<UDWODungeon>(NewObject(this,UDWODungeon));
  if(!Dungeon.InitializeSettings(this))return;
  if(HeroArena)
  {
   if(InHideout)SpawnActor(ADWOHideoutRoom);else SpawnActor(ADWOHeroProbeRoom);Hero.SetActorLocation(InHideout?FVector(0,-350,101):FVector(0,0,100));Dungeon.Tower=this;Dungeon.PlayerSpawnLocation=Hero.ActorLocation;Dungeon.Ready=true;
   for(int X=-4;X<=4;X++)for(int Y=-4;Y<=4;Y++)Dungeon.Points.Add(FVector(X*300,Y*300,60));
  }
  else Dungeon.Initialize(this);
  if(Hero.TopDown.CameraFollow!=nullptr)Hero.TopDown.CameraFollow.ResetAfterTeleport();
  Hero.EnsureCombatTarget();Rules.UpdateActorTarget(Hero.CombatTarget,1,0,400,400,45);Hero.OwnerSlot=Rules.RegisterOwner(1,0);
  if(!Rules.EnablePlayer(Hero.OwnerSlot,Error)){Fail(Error);return;}
  auto Journey=UDWOTowerJourney::Get();
  if(!Journey.Initialized)
  {
   Journey.God=UOpusNative::Flag("DWOGod");Journey.AutoCast=UOpusNative::Flag("DWOAuto");
   Journey.AutoAdvance=UOpusNative::Flag("DWOAutoAdvance");Journey.TimeLimit=UOpusNative::Flag("DWOTimeLimit");
   Journey.MonsterCount=Math::Clamp(UOpusNative::IntOption("DWODensity",20),20,300);Journey.Floor=Math::Clamp(UOpusNative::IntOption("DWOFloor",1),1,100);
   if(!UOpusNative::Flag("DWOTransient")||UOpusNative::Flag("DWOMapItemReloadTest"))Journey.PlayerJson=UOpusNative::ReadProfile();
   if(!UOpusNative::Flag("DWOTransient")||UOpusNative::Flag("DWOMapCoreProfileTest")){FString SavedCore=UOpusNative::ReadMapCore();if(!SavedCore.IsEmpty())Journey.MapCore=FName(SavedCore);}
   FString RequestedCore=UOpusNative::StringOption("DWOMapCore");if(!RequestedCore.IsEmpty())Journey.MapCore=FName(RequestedCore);
   Journey.Initialized=true;
  }
  if(Journey.PlayerJson.IsEmpty()||!Rules.ImportPlayerState(Journey.PlayerJson,Error))
  {if(!Rules.StartPlayerProgression(Error)){Fail(Error);return;}Rules.RespawnPlayer();}
  if(!MapItems.Initialize(this,Journey.PlayerJson,Error)){Fail(Error);return;}
  if(!MapItems.State.Active.Uid.IsEmpty())Journey.Floor=MapItems.State.Active.Tier;
  Run.Floor=Journey.Floor;Run.MonsterCount=Journey.MonsterCount;Run.TimeLimitEnabled=Journey.TimeLimit;Run.AutoAdvance=Journey.AutoAdvance&&Run.Floor<100&&MapItems.State.Active.Uid.IsEmpty();
  Hero.AutoCast=Journey.AutoCast;Rules.SetPlayerGod(Journey.God);
  FString Preset=UOpusNative::StringOption("DWOPreset");if(!Preset.IsEmpty()&&!UOpusNative::ApplyPreset(Rules,FName(Preset),Error)){Fail(Error);return;}
  // Restore passive sources before clamping carried pools to their final maxima.
  Hud.Tower=this;Hud.Talents=UDWOTalentConstellation::Create(this);Hud.Talents.Initialize(Hud);
  Recent.Initialize(this);if(!Recent.Ready)return;
  if(Journey.HasCarry){Rules.SelectPlayerSkill(Journey.Selected,Error);UOpusNative::CarryPools(Rules,Journey.Carry);auto Pools=Rules.GetPlayerPools();bool Kept=Math::Abs(Pools.Life-Math::Min(Pools.MaxLife,Journey.Carry.Life))<0.001&&Math::Abs(Pools.Mana-Math::Min(UOpusRecentGameplay::UsableMana(Rules),Journey.Carry.Mana))<0.001;UOpusNative::WriteLog(Kept?"TowerCarryPass":"TowerCarryMismatch");if(!Kept&&UOpusNative::Flag("DWOAssertCarry")){Fail("cross-floor resources changed");return;}Journey.HasCarry=false;}
  Random.Initialize(int(int64(Journey.Seed)+int64(Run.Floor)*2654435769));
  AppearanceRandom.Initialize(Journey.Seed+Run.Floor*3571);
  Lineages=Cast<UDWMonsterLineageSettings>(LoadObject(nullptr,"/Game/Monsters/Mass/Lineages/DA_MonsterLineages.DA_MonsterLineages"));
  if(Lineages==nullptr){Fail("Missing accepted lineage configuration");return;}
  double Total=0;for(auto R:Lineages.Rules)Total+=R.Weight;double Pick=AppearanceRandom.GetFraction()*Total;
  for(auto R:Lineages.Rules){AppearanceRule=R;Pick-=R.Weight;if(Pick<0)break;}
  UOpusNative::WriteLog("TowerLineage rule="+AppearanceRule.Id+" families="+Lineages.Lineages.Num());
  if(!ApplyMapCore(Journey.MapCore,false)){UOpusNative::WriteLog("MapCoreFallback "+LastError);if(!ApplyMapCore(n"map_core_equipment",false)){Fail(LastError);return;}}
  Journey.Visit++;UOpusNative::BeginRewards(Rules,int(int64(Journey.Seed)+int64(Run.Floor)*2654435769),Run.Floor,Journey.Visit,Math::Min(10,Map.Tier));
  DropService.Initialize(Rules);DropService.RequestPool(Journey.MapCore,Run.Floor,Math::Min(10,Map.Tier),MapItems.State.Active.BaseId);
  SmokeSeconds=UOpusNative::IntOption("DWOSmoke",0);
  UOpusNative::WriteLog("TowerReady floor="+Run.Floor+" native=1 host=AS map="+Journey.MapCore+" count="+Run.MonsterCount);
  if(!UOpusNative::LuaSmoke()){Fail("Lua bridge smoke failed");return;}
  Hero.Stun.Initialize(Hero);
  Hero.SelfSkills.Initialize(Hero);
  Hero.ReactiveAuras.Initialize(Hero);
  Hud.Initialize(this);
  WorldUI.Initialize(this);
  Events.Initialize(this);
  Studio.Initialize(this);
  EnsureMonsterHealthHUD();
  UOpusNative::SetEnabled(Rules,false);SaveProfile();
  if(UOpusNative::Flag("DWOInventoryDragTest")&&UOpusNative::Flag("DWOTransient")){auto DragTest=UDWOInventoryDragTest::Create(this);DragTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOStudioIterationTest")&&UOpusNative::Flag("DWOTransient")){auto IterationTest=UDWOStudioIterationTest::Create(this);IterationTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOStudioSyncTest")&&UOpusNative::Flag("DWOTransient")){auto StudioTest=UDWOStudioSyncTest::Create(this);StudioTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOMerchantWidgetTest")&&UOpusNative::Flag("DWOTransient")){auto MerchantTest=UDWOMerchantWidgetTest::Create(this);MerchantTest.Initialize(this);return;}
  if((UOpusNative::Flag("DWOItemContextTest")||UOpusNative::Flag("DWOInventorySplitTest"))&&UOpusNative::Flag("DWOTransient")){auto ContextTest=UDWOItemContextTest::Create(this);ContextTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOAggroTest")&&UOpusNative::Flag("DWOTransient")){auto AggroTest=UDWOEnemyAggroTest::Create(this);AggroTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWO3CTest")&&UOpusNative::Flag("DWOTransient")){auto ThreeCTest=UDWO3CIntegrationTest::Create(this);ThreeCTest.Initialize(this);return;}
  if((UOpusNative::Flag("DWOMapEventTest")||UOpusNative::Flag("DWOEventsPreview"))&&UOpusNative::Flag("DWOTransient")){auto EventTest=UDWOMapEventTest::Create(this);EventTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOMapItemTest")&&UOpusNative::Flag("DWOTransient")){auto MapTest=UDWOMapItemTest::Create(this);MapTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOMapDeviceTest")&&UOpusNative::Flag("DWOTransient")){auto DeviceTest=UDWOMapDeviceTest::Create(this);DeviceTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOTalentTest")&&UOpusNative::Flag("DWOTransient")){auto TalentTest=UDWOTalentTest::Create(this);TalentTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWODropServiceTest")&&UOpusNative::Flag("DWOTransient")){auto DropTest=UDWODropServiceTest::Create(this);DropTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOStunTest")&&UOpusNative::Flag("DWOTransient")){auto StunTest=UDWOPlayerStunTest::Create(this);StunTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWORecentTest")&&UOpusNative::Flag("DWOTransient")){auto RecentTest=UDWORecentGameplayTest::Create(this);RecentTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOModernTest")&&UOpusNative::Flag("DWOTransient")){auto ModernTest=UDWOModernCombatTest::Create(this);ModernTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOTauntTest")&&UOpusNative::Flag("DWOTransient")){auto TauntTest=UDWOTauntTest::Create(this);TauntTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOWindBreathTest")&&UOpusNative::Flag("DWOTransient")){auto WindTest=UDWOWindBreathTest::Create(this);WindTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOAuraActivationTest")&&UOpusNative::Flag("DWOTransient")){auto AuraFlowTest=UDWOAuraActivationTest::Create(this);AuraFlowTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWODungeonTest")&&UOpusNative::Flag("DWOTransient")){auto DungeonTest=UDWODungeonTest::Create(this);DungeonTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOMapCoreTest")&&UOpusNative::Flag("DWOTransient")){DWOMapCoreTests::Run(this);return;}
  if(UOpusNative::Flag("DWOMapCoreReloadTest")&&UOpusNative::Flag("DWOTransient")){UOpusNative::Quit(DWOMapCoreTests::Reload(this));return;}
  if(UOpusNative::Flag("DWODeathRetry")&&Journey.Visit>1){bool Ok=!Rules.GetPlayerPools().bDead&&Run.Phase==EDWOTowerPhase::Ready;UOpusNative::WriteLog(Ok?"AS_TEST_PASS DeathRetryReady":"AS_TEST_FAIL DeathRetryReady");UOpusNative::Quit(Ok);return;}
  if(UOpusNative::Flag("DWOContractTest")){SvgTest=UDWOSvgContractTest::Create(this);SvgTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOTopDownTest")&&UOpusNative::Flag("DWOTransient")){auto ControlTest=UDWOTopDownTest::Create(this);ControlTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOHeroTest")&&UOpusNative::Flag("DWOTransient")){HeroTest=UDWOHeroTest::Create(this);HeroTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOAttackEndTest")&&UOpusNative::Flag("DWOTransient")){auto EndTest=UDWOAttackEndTest::Create(this);EndTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOFreeCastTest")&&UOpusNative::Flag("DWOTransient")){auto CastTest=UDWOFreeCastTest::Create(this);CastTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOFrameTest")&&UOpusNative::Flag("DWOTransient")){FrameTest=UDWOFrameTest::Create(this);FrameTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOBossClearTest")&&UOpusNative::Flag("DWOTransient")){auto BossTest=UDWOBossClearTest::Create(this);BossTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOHoverTest")&&UOpusNative::Flag("DWOTransient")){auto HoverTest=UDWOHoverContractTest::Create(this);HoverTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOEquipmentStyleTest")&&UOpusNative::Flag("DWOTransient")){auto EquipmentTest=UDWOEquipmentStyleTest::Create(this);EquipmentTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOInventoryNoticeTest")&&UOpusNative::Flag("DWOTransient")){auto NoticeTest=UDWOInventoryNoticeTest::Create(this);NoticeTest.Initialize(this);return;}
  if((UOpusNative::Flag("DWOLootTest")||UOpusNative::Flag("DWOLootPreview"))&&UOpusNative::Flag("DWOTransient")){LootTest=UDWOLootTest::Create(this);LootTest.Initialize(this);}
  if(UOpusNative::Flag("DWOHealthbarTest")&&UOpusNative::Flag("DWOTransient")){MonsterHealthTest=UDWOMonsterHealthTest::Create(this);MonsterHealthTest.Initialize(this);}
  if(UOpusNative::Flag("DWOUEVFXTest")&&UOpusNative::Flag("DWOTransient")){auto VFXTest=UDWOUEVFXTest::Create(this);VFXTest.Initialize(this);return;}
  if(UOpusNative::Flag("DWOEntityUITest")&&UOpusNative::Flag("DWOTransient")){EntityOverlayTest=UDWOEntityOverlayTest::Create(this);EntityOverlayTest.Initialize(this);}
  if(UOpusNative::Flag("DWOStart")||Journey.StartOnArrival||((UOpusNative::Flag("DWODeathRetry")||UOpusNative::Flag("DWOResetTest"))&&Journey.Visit==1)){Journey.StartOnArrival=false;StartChallenge();}
 }
 bool ApplyMapCore(FName Core,bool Persist=true)
 {
  FOpusMapData Data;FString Error;if(!UOpusMapItems::Evaluate(Rules,Core,MapItems.State.Active,Data,Error)){LastError=Error;return false;}
  auto Journey=UDWOTowerJourney::Get();Map=Data;Journey.MapCore=Core;SetMonsterCount(Journey.MonsterCount);
  // A map is the summoner; pass its aggregate through an ordinary inherited rule source.
  double Life=Math::Max(0.01,1.0+Map.LifeIncreased/100)*Map.LifeMore;
  double Damage=Math::Max(0.0,1.0+Map.DamageIncreased/100)*Map.DamageMore;
  Inherited="{\"id\":\"__map_summon\",\"params\":{},\"modifiers\":[{\"target\":\"owner\",\"stat\":\"max_life\",\"op\":\"add\",\"value\":"+Map.LifeBase+"},{\"target\":\"owner\",\"stat\":\"max_life\",\"op\":\"multiply\",\"value\":"+Life+"},{\"target\":\"owner\",\"stat\":\"damage_multiplier\",\"op\":\"multiply\",\"value\":"+Damage+"}]}";
  UOpusNative::SetMapRewardTier(Rules,Map.Tier);LastError="";
  if(DropService.Initialized&&Persist)DropService.RequestPool(Core,Run.Floor,Math::Min(10,Map.Tier),MapItems.State.Active.BaseId);
  if(Persist&&(!UOpusNative::Flag("DWOTransient")||UOpusNative::Flag("DWOMapCoreProfileTest")))if(!UOpusNative::WriteMapCore(Core.ToString())){LastError="地图核心配置保存失败";return false;}
  UOpusNative::WriteLog("MapCoreEquipped id="+Core+" tier="+Map.Tier+" packs="+Map.Packs+" count="+Run.MonsterCount);return true;
 }
 void Fail(FString Error){Preparing=false;if(UOpusNative::IntOption("DWOSmoke",0)>0)UOpusNative::Quit(false);LastError=Error;Run.Phase=EDWOTowerPhase::Failed;UOpusNative::WriteLog("TowerError "+Error);if(Rules!=nullptr)UOpusNative::SetEnabled(Rules,false);}
 void EnsureMonsterHealthHUD()
 {
  if(MonsterHealthHUD!=nullptr||UOpusNative::Flag("DWONoHUD")||UOpusNative::Flag("DWOSvgReference"))return;
  auto PC=Gameplay::GetPlayerController(0);if(PC==nullptr)return;
  MonsterHealthHUD=Cast<UDWOMonsterHealthHUD>(WidgetBlueprint::CreateWidget(UDWOMonsterHealthHUD,PC));
  if(MonsterHealthHUD!=nullptr){MonsterHealthHUD.Tower=this;MonsterHealthHUD.AddToViewport(1);UOpusNative::WriteLog("MONSTER_HEALTH_HUD_READY host=AS style=RuleStudio3D");}
 }
 bool HasRemainingDungeonEnemies() const {for(auto Enemy:Enemies)if(IsValid(Enemy)&&!Enemy.Dead&&!Enemy.Staged&&Enemy.EventId.IsEmpty())return true;return false;}
 UFUNCTION(BlueprintCallable) bool CanFight() const {return DropService.Ready&&Dungeon!=nullptr&&Dungeon.Ready&&!Paused&&!Preparing&&Hero!=nullptr&&Hero.Visual.Ready&&(Run.Phase==EDWOTowerPhase::Waves||Run.Phase==EDWOTowerPhase::Boss||(Run.Phase==EDWOTowerPhase::Cleared&&(Events.CombatActive()||HasRemainingDungeonEnemies())));}
 // Using a skill does not start a wave or require an active encounter.
 UFUNCTION(BlueprintCallable) bool CanUseSkills() const {return DropService.Ready&&Dungeon!=nullptr&&Dungeon.Ready&&!Paused&&!Preparing&&!Traveling&&Hero!=nullptr&&Hero.Visual.Ready&&Run.Phase!=EDWOTowerPhase::Failed;}
 UFUNCTION(BlueprintCallable) void SetPaused(bool Value){Paused=Value;UOpusNative::SetEnabled(Rules,CanUseSkills());if(Hero!=nullptr)Rules.SetPlayerCasting(false);}
 UFUNCTION(BlueprintCallable) void StartChallenge(){if(UDWOTowerJourney::Get().Hideout){LastError="请返回地牢开始挑战";return;}if(Rules==nullptr||Run.Phase!=EDWOTowerPhase::Ready||Dungeon==nullptr)return;if(!DropService.Ready){StartAfterDropPool=true;if(DropService.Failed)DropService.Retry();LastError="正在请求地图掉落道具池";return;}if(!Dungeon.Ready){Dungeon.StartWhenReady=true;return;}StartAfterDropPool=false;LastError="";Paused=false;if(Studio.Ready)Studio.Started();SpawnEncounter(Run.Start());}
 void SpawnEncounter(EDWOTowerCommand Command)
 {
  if(Command!=EDWOTowerCommand::Wave&&Command!=EDWOTowerCommand::Boss)return;
  if(Preparing){Fail("Encounter preparation already active");return;}
  FCpuProfilerTraceScoped Scope(n"Opus_PlanEncounter");
  Preparing=true;PreparedIndex=0;PreparationFrames=0;PreparationSeconds=0;PendingMonsters.Empty();PendingAssets.Empty();
  UOpusNative::SetEnabled(Rules,false);Rules.SetPlayerCasting(false);
  // Explicit boss-only probes share this path; normal starts publish the full map.
  bool BossOnly=Command==EDWOTowerCommand::Boss;int Count=BossOnly?1:Run.EncounterCount();
  Dungeon.Reserved.Empty();
  TArray<FOpusMonsterData> Types;double Weight=0;
  for(auto D:UOpusNative::Monsters(Rules))if(D.Enabled&&D.Weight>0&&D.MinFloor<=Run.Floor&&(UDWOTowerJourney::Get().MonsterType.IsNone()||D.Id==UDWOTowerJourney::Get().MonsterType)){Types.Add(D);Weight+=D.Weight;}
  if(Types.Num()==0){Fail("No eligible monster definitions");return;}
  for(int I=0;I<Count;I++)
  {
   bool IsBoss=BossOnly||I==Count-1;
   double Pick=Random.GetFraction()*Weight;FOpusMonsterData D=Types.Last();for(auto Candidate:Types){Pick-=Candidate.Weight;if(Pick<0){D=Candidate;break;}}
   FName Rarity=D.Rarity;if(IsBoss)Rarity=n"monster_rarity_boss";else if(Rarity.IsNone())Rarity=Random.GetFraction()<0.08?n"monster_rarity_rare":n"monster_rarity_common";
   TArray<FOpusSkillData> Skills;
   if(IsBoss){D.Health*=9;D.Speed=Math::Min(230.0,D.Speed);FOpusSkillData Nova;Nova.Id=n"monster_nova";Nova.Range=450;Nova.Cooldown=5;Nova.Power=30;Skills.Add(Nova);}
   FVector Pos;if(!Dungeon.SpawnPoint(I,IsBoss,Pos)){Fail("No connected dungeon spawn point");return;}
   FDWOPreparedMonster Plan;Plan.Data=D;Plan.Rarity=Rarity;Plan.Position=Pos;Plan.Boss=IsBoss;Plan.Skills=Skills;Plan.Appearance=PickAppearance(IsBoss);PendingMonsters.Add(Plan);
   auto Model=DWOModels::Streamable(Plan.Appearance);if(Model.Mesh.IsEmpty()){Fail("Unknown model "+Plan.Appearance);return;}
   DWO3CAssets::Prepare(Plan.Appearance,PendingAssets);
  }
  UOpusAssets::Request(this,PendingAssets);
  UOpusNative::WriteLog("TowerPreparing floor="+Run.Floor+" wave="+Run.Wave+" count="+Count+" assets="+PendingAssets.Num());
 }
 bool SpawnEventGuards(FString EventId,int Count,int Seed,FVector Origin)
 {
  if(Preparing||Run.Phase!=EDWOTowerPhase::Cleared||Count<1||Count>30)return false;
  FRandomStream Stream;Stream.Initialize(Seed);TArray<FOpusMonsterData> Types;double Weight=0;
  for(auto D:UOpusNative::Monsters(Rules))if(D.Enabled&&D.Weight>0&&D.MinFloor<=Run.Floor){Types.Add(D);Weight+=D.Weight;}
  TArray<FVector> Points;for(auto P:Dungeon.Points)if(Dungeon.OutsidePlayerSpawnExclusion(P)&&(P-Origin).Size2D()>=180&&(P-Origin).Size2D()<=1800)Points.Add(P);
  if(Types.Num()==0||Points.Num()<Count){LastError="宝库附近没有足够的可达守卫位置";return false;}
  PendingMonsters.Empty();PendingAssets.Empty();
  for(int I=0;I<Count;I++)
  {
   double Pick=Stream.GetFraction()*Weight;auto D=Types.Last();for(auto T:Types){Pick-=T.Weight;if(Pick<0){D=T;break;}}
   int At=Stream.RandRange(0,Points.Num()-1);FDWOPreparedMonster Plan;Plan.Data=D;Plan.Rarity=n"monster_rarity_rare";Plan.EventId=EventId;Plan.Position=Points[At];Points.RemoveAt(At);Plan.Appearance=PickAppearanceFrom(false,Stream);PendingMonsters.Add(Plan);
   auto Model=DWOModels::Streamable(Plan.Appearance);DWO3CAssets::Prepare(Plan.Appearance,PendingAssets);
  }
  SetPaused(false);Preparing=true;PreparedIndex=0;PreparationFrames=0;PreparationSeconds=0;UOpusNative::SetEnabled(Rules,false);Rules.SetPlayerCasting(false);UOpusAssets::Request(this,PendingAssets);UOpusNative::WriteLog("EVENT_GUARDS token="+EventId+" count="+Count);return true;
 }
 void PrepareEncounter(float DeltaSeconds)
 {
  PreparationFrames++;PreparationSeconds+=DeltaSeconds;
  if(!Hero.Visual.Ready)return;
  FString AssetError=UOpusAssets::Error(this,PendingAssets);if(!AssetError.IsEmpty()){Fail(AssetError);Preparing=false;return;}
  if(PreparationSeconds>90){Fail("Encounter preparation timed out");Preparing=false;return;}
  if(!UOpusAssets::Ready(this,PendingAssets))return;
  // Exactly one actor, its native registration and authored health widget per frame.
  // All native clocks stay disabled until the whole encounter can be published.
  if(PreparedIndex<PendingMonsters.Num())
  {
   FCpuProfilerTraceScoped Scope(n"Opus_PrepareOneMonster");
   auto Plan=PendingMonsters[PreparedIndex++];auto D=Plan.Data;
   auto Enemy=Cast<ADWOEnemy>(SpawnActor(ADWOEnemy,Plan.Position+FVector(0,0,Plan.Boss?30:25)));
   Enemy.SetActorHiddenInGame(true);Enemy.Staged=true;Enemy.Rules=Rules;Enemy.Tower=this;Enemy.EntityId=++NextId;Enemy.Boss=Plan.Boss;Enemy.EventId=Plan.EventId;Enemy.Speed=D.Speed;Enemy.StopDistance=D.StopDistance;
   Enemy.RuleTarget.RadiusCm=Plan.Boss?90:45;if(Plan.Boss)Enemy.Body.SetRelativeScale3D(FVector(1.4,1.4,2.4));
   FString Error;if(!UOpusNative::RegisterEnemy(Rules,Enemy,Enemy.EntityId,D,Plan.Rarity,Run.Floor,Enemy.RuleTarget.RadiusCm,Inherited,Plan.Skills,Error)){Enemy.DestroyActor();Fail(Error);Preparing=false;return;}
   float Power=Plan.Boss?30:UOpusNative::IsElite(Rules,Enemy.EntityId)?20:10;
   if(!UOpusNative::HitPower(Rules,Enemy.EntityId,Power,Error)){Fail(Error);Preparing=false;return;}
   Enemy.ConfigureAppearance(Plan.Appearance);Enemies.Add(Enemy);
   Enemy.Model.GlobalAnimRateScale=0;Enemy.StopTravel();Enemy.CharacterMovement.SetMovementMode(EMovementMode::MOVE_None);
   if(MonsterHealthHUD!=nullptr)MonsterHealthHUD.Prepare(Enemy);
   return;
  }
  FCpuProfilerTraceScoped Scope(n"Opus_PublishEncounter");
  for(auto Enemy:Enemies)if(IsValid(Enemy)&&Enemy.Staged){Enemy.Staged=false;Enemy.SetActorHiddenInGame(false);Enemy.Model.GlobalAnimRateScale=Paused?0:1;Enemy.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);}
  Preparing=false;UOpusNative::SetEnabled(Rules,CanUseSkills());
  int Bosses=0;for(auto Plan:PendingMonsters)if(Plan.Boss)Bosses++;
  UOpusNative::WriteLog("TowerEncounter floor="+Run.Floor+" wave="+Run.Wave+" boss="+Bosses+" count="+PendingMonsters.Num()+" frames="+PreparationFrames+" preparation="+PreparationSeconds);
  PendingMonsters.Empty();PendingAssets.Empty();
 }
 UFUNCTION() float32 SweepRoom(FVector Start,FVector End,float32 Radius)
 {
  return float32(DWO3CGeometry::Sweep(Start,End,Radius));
 }
 FString PickAppearance(bool Boss){return PickAppearanceFrom(Boss,AppearanceRandom);}
 FString PickAppearanceFrom(bool Boss,FRandomStream& Stream)
 {
  double Weight=0;for(auto F:AppearanceRule.Lineages)Weight+=F.Weight;double Pick=Stream.GetFraction()*Weight;FName Family;
  for(auto F:AppearanceRule.Lineages){Family=F.Lineage;Pick-=F.Weight;if(Pick<0)break;}
  if(Boss){if(Family==n"goblin")return "faction_boss_red_crown_king";if(Family==n"cthulhu")return "faction_boss_abyss_sovereign";if(Family==n"stone_golem")return "faction_boss_crystal_core_titan";int B=Stream.RandRange(0,2);return B==0?"faction_boss_red_crown_king":B==1?"faction_boss_abyss_sovereign":"faction_boss_crystal_core_titan";}
  for(auto F:Lineages.Lineages)if(F.Id==Family){double Sum=0;for(auto M:F.Members)Sum+=M.Weight;double At=Stream.GetFraction()*Sum;for(auto M:F.Members){At-=M.Weight;if(At<0)return M.Archetype.ToString().ToLower();}if(F.Members.Num()>0)return F.Members.Last().Archetype.ToString().ToLower();}
  return "swordsman";
 }
 void SetMonsterCount(int Count){int Base=Math::Clamp(Count,20,300);UDWOTowerJourney::Get().MonsterCount=Base;Run.MonsterCount=Math::Clamp(int(Math::RoundToInt((Base-1)*Math::Max(0.0,1.0+Map.Packs/100))),0,299)+1;}
 UFUNCTION(BlueprintCallable) void ChallengeFloor(int Floor){if(Floor!=Run.Floor){LastError="请在世界地图中装入对应层级的地图物品";return;}UDWOTowerJourney::Get().StartOnArrival=true;Travel(Run.Phase==EDWOTowerPhase::Cleared);}
 UFUNCTION(BlueprintCallable) void Retry(bool StartImmediately=false){UDWOTowerJourney::Get().StartOnArrival=StartImmediately;Travel(false);}
 UFUNCTION(BlueprintCallable) void SaveProfile(){if(Rules!=nullptr&&MapItems.Ready&&Recent.Ready&&!UOpusNative::Flag("DWOTransient"))UOpusNative::WriteProfile(MapItems.Snapshot());}
 void Travel(bool Carry)
 {
  if(Traveling)return;Events.ReleaseLocks();Traveling=true;auto Journey=UDWOTowerJourney::Get();Journey.PlayerJson=MapItems.Snapshot();Journey.Carry=Rules.GetPlayerPools();Journey.HasCarry=Carry;Journey.Floor=Run.Floor;Journey.Selected=UOpusNative::Selected(Rules);Journey.AutoCast=Hero.AutoCast;SaveProfile();
  Gameplay::OpenLevel(UDWOTowerJourney::Get().Hideout||(UOpusNative::Flag("DWOHeroArena")&&UOpusNative::Flag("DWOTransient"))?n"/Engine/Maps/Entry":n"/Game/ClaudeDungeon/Maps/L_OpusDungeon",true,"game=/Script/Angelscript.DWOTowerGameMode");
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){if(!Traveling)SaveProfile();}
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Traveling||Dungeon==nullptr)return;
  UOpusAssets::Pump(this);
  if(DropService.Failed){LastError=DropService.Error;StartAfterDropPool=false;}
  if(StartAfterDropPool&&DropService.Ready)StartChallenge();
  if(!Dungeon.Ready){Dungeon.Update(DeltaSeconds);return;}
  if(!HoverViewShown&&CaptureClock>=1&&!UOpusNative::StringOption("DWOHoverView").IsEmpty()&&UOpusNative::Flag("DWOTransient")){HoverViewShown=true;if(!DWOHoverTests::View(this)){Fail("Hover view failed");return;}}
  EnsureMonsterHealthHUD();
  double Dt=Math::Clamp(double(DeltaSeconds),0.0,0.05);
  CaptureClock+=Dt;FString Capture=UOpusNative::StringOption("DWOCapture");if(UOpusNative::Flag("DWOTransient")&&!Captured&&!Capture.IsEmpty()&&CaptureClock>=5){Captured=true;System::ExecuteConsoleCommand("Shot ShowUI filename="+Capture+" -nosuffix");}

  if(SmokeSeconds>0){auto Journey=UDWOTowerJourney::Get();Journey.SmokeClock+=Dt;if(Journey.SmokeClock>=SmokeSeconds){UOpusNative::WriteLog("TowerSmokeComplete floor="+Run.Floor+" phase="+int(Run.Phase)+" cleared="+Journey.Cleared);UOpusNative::Quit(true);return;}}
  if(Run.Phase==EDWOTowerPhase::Failed&&UOpusNative::Flag("DWODeathRetry")){auto Button=Hud.Screen.FindSourceNode("battle_live_modal_page::battle-death-restart");if(Button==nullptr){Fail("saved SVG retry button missing");return;}Hud.Screen.HandleInvoke(Button);return;}
  if(UOpusNative::Flag("DWOResetTest")&&UDWOTowerJourney::Get().Visit==1&&Run.Elapsed>=0.5){auto Button=Hud.Screen.FindSourceNode("hud-screen-instance::rs-rail-instance::rs-function-restart::rs-function-button");if(Button==nullptr){Fail("saved SVG reset button missing");return;}Hud.Screen.HandleInvoke(Button);return;}
  if(Rules==nullptr||Hero==nullptr||Traveling)return;
  if(!DropService.Ready){UOpusNative::SetEnabled(Rules,false);return;}
  if(Preparing){PrepareEncounter(DeltaSeconds);return;}
  if(UOpusNative::Flag("DWOResetTest")&&UDWOTowerJourney::Get().Visit>1&&!ResetVerified){ResetVerified=true;bool Ok=Run.Phase==EDWOTowerPhase::Waves&&Run.Wave==1&&Run.Elapsed==0&&Enemies.Num()==Run.EncounterCount()&&!Rules.GetPlayerPools().bDead;UOpusNative::WriteLog(Ok?"SVG_TEST_PASS ResetStartsCurrentFloor":"SVG_TEST_FAIL ResetStartsCurrentFloor");UOpusNative::Quit(Ok);return;}
  if(Paused)return;
  UOpusNative::SetEnabled(Rules,CanUseSkills());
  UOpusNative::StepIfNeeded(Rules,float(Dt));int Alive=0,BossesAlive=0;
  Recent.AfterStep();
  for(auto Enemy:Enemies)if(IsValid(Enemy)&&!Enemy.Dead){auto Pools=Rules.GetTargetPools(Enemy.EntityId);if(Pools.bActive&&Pools.bDead)Enemy.Die();else if(Enemy.EventId.IsEmpty()){Alive++;if(Enemy.Boss)BossesAlive++;}}
  bool Auto=Run.AutoAdvance;if(Events.HoldsTravel())Run.AutoAdvance=false;
  auto Before=Run.Phase;auto Command=Run.Step(Dt,BossesAlive,Rules.GetPlayerPools().bDead);Run.AutoAdvance=Auto;SpawnEncounter(Command);
  if(Run.Phase==EDWOTowerPhase::Cleared&&Before!=Run.Phase){UDWOTowerJourney::Get().Cleared++;UOpusNative::CompleteRewards(Rules);UOpusNative::SetEnabled(Rules,CanUseSkills());Recent.FirstFloorCleared();SaveProfile();UOpusNative::WriteLog("TowerCleared floor="+Run.Floor+" elapsed="+Run.Elapsed+" kills="+Kills);}
  if(Command==EDWOTowerCommand::Failed){UOpusNative::SetEnabled(Rules,false);SaveProfile();UOpusNative::WriteLog("TowerFailed floor="+Run.Floor+" reason="+(Run.Timeout?"timeout":"dead"));}
  if(Command==EDWOTowerCommand::Advance){Run.AutoAdvance=false;Hud.MapDevice.Open();return;}
  if(CanFight()&&Run.Elapsed>=SaveAt){SaveAt+=5;SaveProfile();}
  if(SmokeSeconds>0){auto Journey=UDWOTowerJourney::Get();if(Journey.SmokeClock>=LogAt){LogAt=Journey.SmokeClock+2;auto Pools=Rules.GetPlayerPools();UOpusNative::WriteLog("TowerSmoke t="+Journey.SmokeClock+" floor="+Run.Floor+" wave="+Run.Wave+" phase="+int(Run.Phase)+" alive="+Alive+" life="+Pools.Life+" cleared="+Journey.Cleared+" casts="+Rules.GetFrameReport().MonsterCasts+" drops="+Rules.GetFrameReport().CapacityDrops);}}
 }
}
