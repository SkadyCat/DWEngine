"""Install the reviewed AS host and typed state transport after the shared build window."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[2];pending={}
def edit(file,pairs):
 p=r/file;s=p.read_text(encoding='utf-8')
 for old,new in pairs:
  assert s.count(old)==1,(file,old[:90],s.count(old))
  s=s.replace(old,new)
 pending[p]=s
edit('Script/Opus/TowerRun.as',[(' UPROPERTY() bool HasCarry=false;',' UPROPERTY() bool Hideout=false;\n UPROPERTY() bool HasCarry=false;')])
edit('Script/Opus/TowerGame.as',[
 ('bool HeroArena=UOpusNative::Flag("DWOHeroArena")&&UOpusNative::Flag("DWOTransient");','bool InHideout=UDWOTowerJourney::Get().Hideout;\n  bool HeroArena=InHideout||(UOpusNative::Flag("DWOHeroArena")&&UOpusNative::Flag("DWOTransient"));'),
 ('SpawnActor(ADWOHeroProbeRoom);Hero.SetActorLocation(FVector(0,0,100));','if(InHideout)SpawnActor(ADWOHideoutRoom);else SpawnActor(ADWOHeroProbeRoom);Hero.SetActorLocation(InHideout?FVector(0,-350,101):FVector(0,0,100));'),
 ('  Recent.Initialize(this);\n',''),
 ('  if(Journey.HasCarry){','  Recent.Initialize(this);if(!Recent.Ready)return;\n  if(Journey.HasCarry){'),
 ('Math::Min(Pools.MaxMana,Journey.Carry.Mana)','Math::Min(UOpusRecentGameplay::UsableMana(Rules),Journey.Carry.Mana)'),
 ('UFUNCTION(BlueprintCallable) void StartChallenge(){if(Rules==nullptr','UFUNCTION(BlueprintCallable) void StartChallenge(){if(UDWOTowerJourney::Get().Hideout){LastError="请返回地牢开始挑战";return;}if(Rules==nullptr'),
 ('Rules!=nullptr&&MapItems.Ready&&!UOpusNative::Flag("DWOTransient")','Rules!=nullptr&&MapItems.Ready&&Recent.Ready&&!UOpusNative::Flag("DWOTransient")'),
 ('Gameplay::OpenLevel(UOpusNative::Flag("DWOHeroArena")&&UOpusNative::Flag("DWOTransient")?','Gameplay::OpenLevel(UDWOTowerJourney::Get().Hideout||(UOpusNative::Flag("DWOHeroArena")&&UOpusNative::Flag("DWOTransient"))?'),
 ('UOpusNative::CompleteRewards(Rules);UOpusNative::SetEnabled(Rules,false);SaveProfile();','UOpusNative::CompleteRewards(Rules);UOpusNative::SetEnabled(Rules,false);Recent.FirstFloorCleared();SaveProfile();'),
 ('  if(UOpusNative::Flag("DWOTauntTest")','  if(UOpusNative::Flag("DWORecentTest")&&UOpusNative::Flag("DWOTransient")){auto RecentTest=UDWORecentGameplayTest::Create(this);RecentTest.Initialize(this);return;}\n  if(UOpusNative::Flag("DWOTauntTest")')])
edit('Script/Opus/MapItems.as',[('FString Snapshot(){return UOpusMapItems::EncodeProfile(Tower.Rules.ExportPlayerState(),State);}','FString Snapshot(){FString Json=UOpusMapItems::EncodeProfile(Tower.Rules.ExportPlayerState(),State);return Tower.Recent.Ready?Tower.Recent.Encode(Json):Json;}')])
edit('Script/Opus/TowerActors.as',[
 ('  if(Tower.Events.HandleKeys(PC))','  if(Tower.Recent.UI!=nullptr&&Tower.Recent.UI.HandleKeys(PC)){TopDown.Stop(true);Rules.SetPlayerCasting(false);return;}\n  if(Tower.Events.HandleKeys(PC))'),
 ('Held=SelfSkills.SelfTarget()||','Held=SelfSkills.FreeTarget()||')])
edit('Script/Opus/SelfSkills.as',[(' bool SelfTarget() const',' bool FreeTarget() const {return SelfTarget()||(Hero!=nullptr&&UOpusRecentGameplay::PointTarget(Hero.Rules));}\n bool SelfTarget() const')])
edit('Script/Opus/SvgHUD.as',[('bool Modal=Tower.Events.IsOpen()||','bool Modal=(Tower.Recent.UI!=nullptr&&Tower.Recent.UI.Panel!=nullptr)||Tower.Events.IsOpen()||')])
file=r/'Script/Opus/HeroVisual.as';s=file.read_text(encoding='utf-8');assert 'Hero.SelfSkills.SelfTarget()' in s;pending[file]=s.replace('Hero.SelfSkills.SelfTarget()','Hero.SelfSkills.FreeTarget()')
file=r/'Source/DungeonWorldOpus/Private/OpusWorldUI.cpp';s=file.read_text(encoding='utf-8');assert 'Directory==TEXT("UICreatorEffects")' in s;pending[file]=s.replace('Directory==TEXT("UICreatorEffects")','Directory==TEXT("UICreatorEffects")||Directory==TEXT("UICreatorRecent")').replace('Directory!=TEXT("UICreatorEffects")','Directory!=TEXT("UICreatorEffects")&&Directory!=TEXT("UICreatorRecent")')
edit('Source/DungeonWorldOpus/DungeonWorldOpus.Build.cs',[('RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorEffects/contract.json", StagedFileType.NonUFS);','RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorEffects/contract.json", StagedFileType.NonUFS);\n  RuntimeDependencies.Add("$(ProjectDir)/Content/UICreatorRecent/contract.json", StagedFileType.NonUFS);')])
edit('Config/DefaultGame.ini',[('+DirectoriesToAlwaysCook=(Path="/Game/UICreatorEvents")','+DirectoriesToAlwaysCook=(Path="/Game/UICreatorEvents")\n+DirectoriesToAlwaysCook=(Path="/Game/UICreatorRecent")')])
for name,dst in [('OpusGameplayState.h','Source/DungeonWorldOpus/Public'),('OpusGameplayState.cpp','Source/DungeonWorldOpus/Private'),('RecentGameplay.as','Script/Opus'),('RecentUI.as','Script/Opus'),('RecentGameplayTests.as','Script/Opus'),('Hideout.as','Script/Opus')]:
 p=r/dst/name;pending[p]=(r/'Tools/Integration/Staged'/name).read_text(encoding='utf-8')
pending[r/'Source/DungeonWorldOpus/Private/Tests/OpusRecentGameplayTests.cpp']=(r/'Tools/Integration/RecentGameplayTests.cpp').read_text(encoding='utf-8')
pending[r/'Plugins/DWRules/Source/DWRules/Private/WebVFX/DWRuleWebVFXAdapter.cpp']=(r/'Tools/Integration/Staged/DWRuleWebVFXAdapter.cpp').read_text(encoding='utf-8')
if '--check' not in sys.argv:
 for p,s in pending.items():p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s,encoding='utf-8')
print('Host continuation '+('checked' if '--check' in sys.argv else 'applied')+' '+str(len(pending))+' files')
