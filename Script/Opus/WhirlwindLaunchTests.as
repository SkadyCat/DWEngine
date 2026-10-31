// Exercise the real authored animation release and AS launch policy, with no target.
class UDWOWhirlwindLaunchTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 double Clock=0;
 int Stage=0,Before=0;
 void Initialize(ADWOTowerGameMode T){Tower=T;T.Hero.ControlProbe=true;SetTickGroup(ETickingGroup::TG_PostUpdateWork);}
 bool Check(bool Good,FString Detail){UOpusNative::WriteLog((Good?"WIND_LAUNCH_PASS ":"WIND_LAUNCH_FAIL ")+Detail);if(!Good)UOpusNative::Quit(false);return Good;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;if(Clock>90){Check(false,"timeout");return;}if(!Tower.CanUseSkills())return;
  auto H=Tower.Hero;auto R=Tower.Rules;
  if(Stage==0)
  {
   FString Error;if(!Check(UOpusNative::ApplyPreset(R,n"whirlwind_grounds",Error),"actual T1 preset "+Error))return;
   R.RespawnPlayer();R.ClearStaticTargets();H.AutoCast=false;H.Aim=FVector(1,0,0);H.AimPoint=H.CombatLocation()+H.Aim*2000;Before=H.Visual.CastSerial;
   Stage=10;Clock=0;return;
  }
  if(Stage==10)
  {
   if(H.CharacterMovement.MovementMode==EMovementMode::MOVE_None)H.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
   if(H.Visual.Anim.UpdateCount<2)return;
   if(Clock<.5||!H.CharacterMovement.IsMovingOnGround())return;
   auto Pools=R.GetPlayerPools();bool Accepted=H.CastAt(H.CombatLocation()+H.Aim*60,H.AimPoint,30,false);
   if(!Check(Accepted,"manual empty-ground action accepted state="+H.Visual.ActionState+" cooldown="+Pools.Cooldown+" actionScale="+Pools.ActionScale))return;Stage=1;return;
  }
  if(Stage==1&&H.Visual.CastSerial>Before)
  {
   int Winds=0;double MinForward=100000,MinOut=1,AngleSum=0;bool Advanced=true;
   for(int I=0;I<H.DeliveryPolicy.ProjectileCount();I++)
   {
    auto P=H.DeliveryPolicy.Projectile(I);if(!P.Valid||P.Hostile||P.Kind!=n"whirlwind")continue;
    if(P.Age<.000001)return;Winds++;MinForward=Math::Min(MinForward,(P.Position-H.CombatLocation()).DotProduct(H.Aim));MinOut=Math::Min(MinOut,P.Velocity.GetSafeNormal2D().DotProduct(H.Aim));AngleSum+=Math::Atan2(P.Velocity.Y,P.Velocity.X)*180/PI;Advanced=Advanced&&P.Age<.15;
   }
   if(Winds==0)return;
   if(!Check(Winds==3&&MinForward>=100&&MinOut>0&&Advanced,"born ahead and pushed out winds="+Winds+" forwardCm="+MinForward+" minOut="+MinOut))return;
   if(!Check(Math::Abs(AngleSum)>1,"seeded random launch differs from a symmetric fan angleSum="+AngleSum))return;
   Check(H.Visual.CastSerial==Before+1,"one native cast per authored release");Stage=2;UOpusNative::WriteLog("WIND_LAUNCH_DONE");UOpusNative::Quit(true);
  }
 }
}
