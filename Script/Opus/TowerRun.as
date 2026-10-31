enum EDWOTowerPhase { Ready, Waves, Boss, Cleared, Failed }
enum EDWOTowerCommand { None, Wave, Boss, Advance, Failed }
// Migrated from FDWBTowerRun: encounter sequencing only, with no combat formulas.
struct FDWOTowerRun
{
 UPROPERTY() EDWOTowerPhase Phase=EDWOTowerPhase::Ready;
 UPROPERTY() int Floor=1;
 UPROPERTY() int Wave=0;
 UPROPERTY() int MonsterCount=20;
 UPROPERTY() double Elapsed=0;
 UPROPERTY() double NextIn=2.5;
 UPROPERTY() double TimeLimitEndsAt=30;
 UPROPERTY() bool Timeout=false;
 UPROPERTY() bool TimeLimitEnabled=false;
 UPROPERTY() bool AutoAdvance=false;
 // One complete dungeon encounter, including its existing single boss.
 int EncounterCount() const {return Math::Clamp(MonsterCount,20,300);}
 double Remaining() const {return Math::Max(0.0,TimeLimitEndsAt-Elapsed);}
 void SetTimeLimit(bool Enabled){if(TimeLimitEnabled==Enabled)return;TimeLimitEnabled=Enabled;if(Enabled)TimeLimitEndsAt=Elapsed+30;}
 EDWOTowerCommand Start()
 {
  if(Phase!=EDWOTowerPhase::Ready)return EDWOTowerCommand::None;
  Phase=EDWOTowerPhase::Waves;Wave=1;return EDWOTowerCommand::Wave;
 }
 EDWOTowerCommand Step(double InDt,int BossesAlive,bool Dead)
 {
  if(Phase==EDWOTowerPhase::Failed)return EDWOTowerCommand::None;
  if(Dead){Phase=EDWOTowerPhase::Failed;return EDWOTowerCommand::Failed;}
  if(Phase==EDWOTowerPhase::Ready)return EDWOTowerCommand::None;
  double Dt=Math::Clamp(InDt,0.0,0.05);
  if(Phase==EDWOTowerPhase::Cleared){if(!AutoAdvance)return EDWOTowerCommand::None;NextIn=Math::Max(0.0,NextIn-Dt);return NextIn<=0.00000001?EDWOTowerCommand::Advance:EDWOTowerCommand::None;}
  Elapsed=TimeLimitEnabled?Math::Min(TimeLimitEndsAt,Elapsed+Dt):Elapsed+Dt;
  if((Phase==EDWOTowerPhase::Waves||Phase==EDWOTowerPhase::Boss)&&BossesAlive==0){Phase=EDWOTowerPhase::Cleared;return EDWOTowerCommand::None;}
  if(TimeLimitEnabled&&Remaining()<0.00000001){Timeout=true;Phase=EDWOTowerPhase::Failed;return EDWOTowerCommand::Failed;}
  return EDWOTowerCommand::None;
 }
}
class UDWOTowerJourney : UScriptGameInstanceSubsystem
{
 UPROPERTY() FString TalentState;
 UPROPERTY() FString PlayerJson;
 UPROPERTY() FDWRulePlayerPools Carry;
 UPROPERTY() int Floor=1;
 UPROPERTY() int Selected=0;
 UPROPERTY() int MonsterCount=20;
 UPROPERTY() int Seed=71;
 UPROPERTY() FName MonsterType;
 UPROPERTY() FName MapCore=n"map_core_equipment";
 UPROPERTY() int Visit=0;
 UPROPERTY() int MapItemTestStage=0;
 UPROPERTY() int Cleared=0;
 UPROPERTY() bool Hideout=false;
 UPROPERTY() bool HasCarry=false;
 UPROPERTY() bool StartOnArrival=false;
 UPROPERTY() bool Initialized=false;
 UPROPERTY() bool AutoCast=false;
 UPROPERTY() bool God=false;
 UPROPERTY() bool TimeLimit=false;
 UPROPERTY() bool AutoAdvance=false;
 UPROPERTY() double SmokeClock=0;
 UPROPERTY() bool HunterEnabled=false;
 UPROPERTY() bool ShowLoot=true;
 UPROPERTY() TArray<FString> HunterTargets;
 UPROPERTY() TArray<FString> HunterCurrencies;
}
