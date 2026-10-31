// Three character-equipment rules. Native DWRules still owns damage and status execution.
struct FDWOStunBuildup
{
 double Amount=0,Quiet=0,DecayRate=0;
 void Reset(){Amount=0;Quiet=0;DecayRate=0;}
 void Hit(double Damage,double Weight){Amount+=Damage*Weight;Quiet=0;DecayRate=0;}
 void Tick(double Dt,double Delay,double Seconds)
 {
  double Before=Math::Max(0.0,Quiet-Delay);Quiet+=Math::Max(0.0,Dt);
  double After=Math::Max(0.0,Quiet-Delay);
  if(Amount<=0||After<=0)return;
  if(Seconds<=0){Amount=0;return;}
  if(DecayRate<=0)DecayRate=Amount/Seconds;
  Amount=Math::Max(0.0,Amount-DecayRate*(After-Before));
 }
}
class UDWOPlayerStun : UActorComponent
{
 UPROPERTY() ADWOHero Hero;
 FDWOStunBuildup Meter;
 bool Initialized=false;
 double Weight=0,ThresholdPercent=0,Duration=0,Delay=0,DecaySeconds=0,RestoreSeconds=0;
 bool Broken=false;
 double Restored=1,BreakAge=0,HitAge=1,PreviousControl=0,LastClock=0;
 int TriggerCount=0,HitCount=0;
 void ReadRules()
 {
  auto E=Hero.CharacterEvents;
  Weight=E.SheetStat(n"stun.buildup");ThresholdPercent=E.SheetStat(n"stun.threshold_life_percent");
  Duration=E.SheetStat(n"stun.duration");Delay=E.SheetStat(n"stun.decay_delay");DecaySeconds=E.SheetStat(n"stun.decay_seconds");
  RestoreSeconds=E.SheetStat(n"stun.restore_seconds");
  E.UseExternalStun(Weight>0&&ThresholdPercent>0&&Duration>0);
 }
 void Initialize(ADWOHero H)
 {
  if(Initialized)return;Hero=H;
  if(!H.CharacterEvents.BindPlayer(H.Rules)){H.Tower.Fail("Cannot bind character equipment hit rules");return;}
  H.CharacterEvents.OnResolvedHit.AddUFunction(this,n"OnResolvedHit");Initialized=true;ReadRules();Reset();
 }
 double Threshold() const {return Hero==nullptr?0:Hero.Rules.GetPlayerPools().MaxLife*ThresholdPercent/100;}
 double Remaining() const {return Hero==nullptr||Hero.Rules.GetPlayerPools().bDead?0:Hero.Rules.GetStatusRemaining(1,n"stun");}
 bool Recovering() const {return Broken&&Remaining()<=0;}
 double Integrity() const {double Limit=Threshold();return Broken?Restored:Limit>0?1-Math::Clamp(Meter.Amount/Limit,0.0,1.0):1;}
 double Fraction() const {return 1-Integrity();}
 double BuildupFraction() const {double Life=Hero==nullptr?0:Hero.Rules.GetPlayerPools().MaxLife;return Life>0?Math::Clamp(Meter.Amount/Life,0.0,1.0):0;}
 void Reset(){Meter.Reset();Broken=false;Restored=1;BreakAge=0;HitAge=1;PreviousControl=0;LastClock=Hero==nullptr?0:Hero.CharacterEvents.SimulationClock();}
 UFUNCTION() void OnResolvedHit(double Amount,bool Dot,bool Dead)
 {
  if(!Initialized||Amount<=0)return;
  if(Dead){Reset();Hero.Visual.CancelPendingCast();return;}
  if(Dot||Broken)return;
  ReadRules();if(Weight<=0||ThresholdPercent<=0||Duration<=0)return;
  HitCount++;Meter.Hit(Amount,Weight);HitAge=0;LastClock=Hero.CharacterEvents.SimulationClock();
  if(Meter.Amount>=Threshold())
  {
   if(Hero.Rules.ApplyStatus(1,n"stun",1,float(Duration)))
   {
    Meter.Reset();Broken=true;Restored=0;BreakAge=0;PreviousControl=Duration;
    TriggerCount++;Hero.Rules.SetPlayerCasting(false);Hero.TopDown.Stop();Hero.Visual.Stunned(Duration);
   }
  }
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(!Initialized||Hero==nullptr)return;
  double Clock=Hero.CharacterEvents.SimulationClock(),Dt=Math::Max(0.0,Clock-LastClock);LastClock=Clock;
  auto P=Hero.Rules.GetPlayerPools();if(P.bDead){Reset();return;}
  if(Hero.Tower.Paused||Hero.Tower.Preparing)return;
  HitAge+=Dt;
  ReadRules();if(Weight<=0||ThresholdPercent<=0||Duration<=0){Reset();return;}
  if(Broken)
  {
   BreakAge+=Dt;
   double Control=Remaining(),RecoveryDt=Math::Max(0.0,Dt-PreviousControl);PreviousControl=Math::Max(0.0,Control);
   if(Control>0)return;
   Restored=RestoreSeconds>0?Math::Min(1.0,Restored+RecoveryDt/RestoreSeconds):1;
   if(Restored>=1)Reset();return;
  }
  Meter.Tick(Dt,Delay,DecaySeconds);
 }
}
