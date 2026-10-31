namespace DWOTowerTests
{
 bool Check(bool Ok,FString Message){if(!Ok)UOpusNative::WriteLog("AS_TEST_FAIL "+Message);return Ok;}
 bool Run()
 {
  bool Ok=true;
  TArray<int> Counts;Counts.Add(20);Counts.Add(21);Counts.Add(22);Counts.Add(300);
  for(int Count:Counts)
  {
   FDWOTowerRun R;R.MonsterCount=Count;
   Ok=Check(R.Start()==EDWOTowerCommand::Wave&&R.EncounterCount()==Count,"one start preserves the entire dungeon density")&&Ok;
   Ok=Check(R.Start()==EDWOTowerCommand::None,"repeated start cannot spawn a second batch")&&Ok;
   Ok=Check(R.Step(0.01,1,false)==EDWOTowerCommand::None&&R.Phase==EDWOTowerPhase::Waves&&R.Wave==1,"living boss prevents completion regardless of ordinary monster count")&&Ok;
   R.Elapsed=29.99;R.TimeLimitEnabled=true;
   Ok=Check(R.Step(0.05,0,false)==EDWOTowerCommand::None&&R.Phase==EDWOTowerPhase::Cleared&&R.Wave==1,"boss death clears with ordinary monsters remaining and beats final-frame timeout")&&Ok;
   Ok=Check(R.Step(0.05,0,false)==EDWOTowerCommand::None,"cleared floor does not respawn enemies")&&Ok;
   R.AutoAdvance=true;for(int I=0;I<49;I++)R.Step(0.05,0,false);
   Ok=Check(R.Step(0.05,0,false)==EDWOTowerCommand::Advance,"advance delay")&&Ok;
  }
  FDWOTowerRun Timeout;Timeout.TimeLimitEnabled=true;Timeout.Start();Timeout.Elapsed=29.99;Ok=Check(Timeout.Step(0.05,1,false)==EDWOTowerCommand::Failed&&Timeout.Timeout,"timeout failure")&&Ok;
  FDWOTowerRun Dead;Dead.Start();Ok=Check(Dead.Step(0.01,0,true)==EDWOTowerCommand::Failed,"death failure")&&Ok;
  FDWOTowerRun Ready;Ok=Check(Ready.Step(0.05,0,false)==EDWOTowerCommand::None&&Ready.Phase==EDWOTowerPhase::Ready&&Ready.Wave==0&&Ready.Elapsed==0,"ready skill simulation does not start a wave")&&Ok;Ok=Check(Ready.Step(0.01,0,true)==EDWOTowerCommand::Failed,"self damage in ready still reaches death failure")&&Ok;
  FDWOTowerRun Unlimited;Unlimited.Start();Unlimited.Elapsed=60;Unlimited.Step(1,1,false);Ok=Check(Math::Abs(Unlimited.Elapsed-60.05)<0.000001,"dt cap and default unlimited")&&Ok;
  FDWOTowerRun Toggle;Toggle.Start();Toggle.Elapsed=60;Toggle.SetTimeLimit(true);Ok=Check(Math::Abs(Toggle.Remaining()-30)<0.000001,"enable timer starts a fresh deadline")&&Ok;Toggle.SetTimeLimit(false);Toggle.Elapsed=100;Toggle.SetTimeLimit(true);Ok=Check(Math::Abs(Toggle.Remaining()-30)<0.000001,"reenable timer resets deadline")&&Ok;
  UOpusNative::WriteLog(Ok?"AS_TEST_PASS TowerFlow":"AS_TEST_FAIL TowerFlow");return Ok;
 }
}
