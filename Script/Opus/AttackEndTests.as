// Real authored notifies in an isolated character world; no profile import/save.
class UDWOAttackEndTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0,Revision=0,Casts=0,Requests=0,Pass=0,WarmupFrames=0;
 double Total=0,Clock=0;
 bool Done=false;
 void Initialize(ADWOTowerGameMode T)
 {
  Tower=T;T.Hero.ControlProbe=true;T.Hero.AutoCast=false;T.Rules.SetPlayerGod(true);
  SetTickGroup(ETickingGroup::TG_PostUpdateWork);
 }
 bool Check(bool Ok,FString Message)
 {
  if(Ok)return true;Done=true;UOpusNative::WriteLog("ATTACK_END_FAIL stage="+Stage+" "+Message);UOpusNative::Quit(false);return false;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Total+=Dt;Clock+=Dt;
  if(!Check(Total<90,"timeout"))return;
  auto H=Tower.Hero;auto V=H.Visual;auto R=Tower.Rules;
  if(!V.Ready||!Tower.CanUseSkills())return;
  if(H.CharacterMovement.MovementMode==EMovementMode::MOVE_None)H.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
  if(!H.CharacterMovement.IsMovingOnGround())return;
  if(Stage==0)
  {
   FString Error;int Slot=-1;for(int I=0;I<4;I++)if(H.DeliveryPolicy.SkillAt(I)==n"frost_lance")Slot=I;
   if(!Check(Slot>=0&&R.SelectPlayerSkill(Slot,Error),"starter ice skill selected "+Error))return;
   if(++WarmupFrames==1)
   {
    // Warm the real presentation catalogue before measuring animation frames.
    FVector Origin=H.CombatLocation();R.CastBuildPower(UOpusNative::ActivePlan(R),H.OwnerSlot,Origin,FVector(1,0,0),30);
    V.StartAction(n"cast",4,2);return;
   }
   if(WarmupFrames<120||V.RotationLocked())return;
   H.Aim=FVector(1,0,0);H.AimPoint=H.CombatLocation()+FVector(650,0,0);
   H.AdvanceCastInput(true,.05,true);
   if(!Check(V.PendingCast,"real input starts unpaid wind-up"))return;
   Revision=V.ActionRevision;Casts=V.CastSerial;Requests=V.CastRequests;
   V.HandleActionEnd(V.Clips[n"cast"]);
   if(!Check(V.ActionEndRevision!=Revision&&!V.CanRestartCast(),"end-before-release cannot unlock a wind-up"))return;
   Stage=1;Clock=0;return;
  }
  if(Stage==1)
  {
   if(!Check(Clock<3,"authored End must fire"))return;
   if(V.ActionEndRevision!=Revision)
   {
    if(!Check(V.CastRequests==Requests&&!V.RequestCast(H.CombatLocation(),H.AimPoint,30),"no new attack before authored End"))return;
    return;
   }
   double Progress=V.Anim.GetCleanActionProgress();
   UOpusNative::WriteLog("ATTACK_END_OBSERVED pass="+Pass+" elapsed="+Clock+" progress="+Progress+" state="+V.ActionState+" locked="+V.RotationLocked()+" cooldown="+R.GetPlayerPools().Cooldown);
   if(!Check(V.CastSerial==Casts+1&&!V.PendingCast,"one real release precedes End"))return;
   if(!Check(Progress<.7&&V.ActionState==0&&!V.RotationLocked()&&!V.MovementLocked(),"End ends action and releases locks before the animation tail"))return;
   if(!Check(V.CanRestartCast()&&R.GetPlayerPools().Cooldown<=0,"no rule cooldown in this fixture"))return;
   // A completed action must not wait for the unrelated input polling interval.
   H.AdvanceCastInput(true,0,true);
   if(!Check(V.PendingCast&&V.CastRequests==Requests+1&&V.ActionRevision==Revision+1&&V.Anim.GetCleanActionProgress()<.05,"next attack starts on the first input after End, including blend-out"))return;
   Revision=V.ActionRevision;Casts=V.CastSerial;Requests=V.CastRequests;
   V.HandleActionEnd(V.Clips[n"cast"]);
   if(!Check(V.PendingCast&&V.ActionEndRevision!=Revision,"old/duplicate End cannot finish the next wind-up"))return;
   if(++Pass<3){Clock=0;return;}
   V.MovementPressed();V.HandleAttackRelease(V.Clips[n"cast"]);
   if(!Check(!V.PendingCast&&V.CastSerial==Casts,"cancelled next wind-up cannot release"))return;
   UOpusNative::WriteLog("ATTACK_END_PASS ImmediateChainAndLocks cycles="+Pass);Stage=2;Clock=0;return;
  }
  if(Stage==2)
  {
   if(Clock<.3)return;
   // The real whirlwind trait must have no invented cooldown.
   FString Error;if(!Check(UOpusNative::ApplyPreset(R,n"whirlwind_return",Error),Error))return;
   H.AdvanceCastInput(true,.05,true);
   if(!Check(V.PendingCast,"real whirlwind skill starts"))return;
   Revision=V.ActionRevision;Stage=3;Clock=0;return;
  }
  if(Stage==3)
  {
   if(!Check(Clock<3,"whirlwind End fires"))return;
   if(V.ActionEndRevision!=Revision)return;
   int Before=V.CastRequests;H.AdvanceCastInput(true,0,true);
   if(!Check(R.GetPlayerPools().Cooldown==0&&V.CastRequests==Before+1&&V.PendingCast&&Clock<1,"whirlwind starts next wind-up at End without a two-second wait"))return;
   V.MovementPressed();UOpusNative::WriteLog("ATTACK_END_PASS WhirlwindNoCooldown elapsed="+Clock);
   Stage=4;Clock=0;return;
  }
  if(Stage==4)
  {
   if(Clock<.3||V.RotationLocked())return;
   FString Error;if(!Check(UOpusNative::ApplyPreset(R,n"whirlwind_swarm",Error)&&R.SelectPlayerSkill(1,Error),Error))return;
   if(!Check(H.DeliveryPolicy.SelectedSkill()==n"echo","existing echo skill supplies a real rule cooldown"))return;
   H.AdvanceCastInput(true,.05,true);if(!Check(V.PendingCast,"echo starts normally"))return;
   Revision=V.ActionRevision;Stage=5;Clock=0;return;
  }
  if(Stage==5)
  {
   if(!Check(Clock<3,"echo End fires"))return;if(V.ActionEndRevision!=Revision)return;
   int Before=V.CastRequests;H.AdvanceCastInput(true,.05,true);
   if(!Check(R.GetPlayerPools().Cooldown>0&&V.CastRequests==Before&&!V.PendingCast&&!V.RotationLocked(),"End unlocks animation but cannot bypass an existing rule cooldown"))return;
   UOpusNative::WriteLog("ATTACK_END_PASS NativeCooldownPreserved remaining="+R.GetPlayerPools().Cooldown);Stage=6;Clock=0;return;
  }
  if(Stage==6)
  {
   if(R.GetPlayerPools().Cooldown>0)return;
   int Before=V.CastRequests;H.AdvanceCastInput(true,Dt,true);
   if(!Check(V.PendingCast&&V.CastRequests==Before+1,"cooldown expiry accepts the next normal action"))return;
   V.MovementPressed();Done=true;UOpusNative::WriteLog("ATTACK_END_PASS Complete");UOpusNative::Quit(true);
  }
 }
}
