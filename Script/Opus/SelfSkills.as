// Self-targeting, per-skill recharge and delivery capabilities are ordinary
// composed sheet traits. The rule VM applies and expires the authored status.
class UDWOSelfSkills : UActorComponent
{
 UPROPERTY() ADWOHero Hero;
 TMap<FName,double> Recharges;
 bool Ready=false;
 void Initialize(ADWOHero H)
 {
  Hero=H;Ready=H.DeliveryPolicy.BindPolicy(H.Rules,this,n"CanReceiveDamage",n"ProjectileMotion");
  if(!Ready)H.Tower.Fail("Cannot bind self skill delivery policy");
 }
 bool FreeTarget() const {return SelfTarget()||(Hero!=nullptr&&UOpusRecentGameplay::PointTarget(Hero.Rules));}
 bool SelfTarget() const {return Hero!=nullptr&&(Hero.CharacterEvents.SheetStat(n"skill.self_target")>0||UOpusRecentGameplay::SelfTarget(Hero.Rules));}
 double Remaining() const
 {
  if(Hero==nullptr)return 0;FName Id=Hero.DeliveryPolicy.SelectedSkill();
  double Own=Recharges.Contains(Id)?Math::Max(0.0,Recharges[Id]-Hero.CharacterEvents.SimulationClock()):0;return Math::Max(Own,UOpusRecentGameplay::SkillCooldown(Hero.Rules,UOpusNative::Selected(Hero.Rules)));
 }
 bool CanCast() const {return Remaining()<=0;}
 FVector LaunchOrigin(FVector Origin,FVector Direction) const
 {
  if(Hero==nullptr||Hero.DeliveryPolicy.SelectedSkill()!=n"whirlwind")return Origin;
  FVector Start=Hero.CombatLocation(),End=Origin+Direction.GetSafeNormal2D()*80;double Wall=UOpusDungeon::Sweep(Hero,Start,End,50);
  return Wall>=0?Start+(End-Start)*Math::Max(0.0,Wall-.02):End;
 }
 double SlotRemaining(int Slot) const
 {
  if(Hero==nullptr)return 0;FName Id=Hero.DeliveryPolicy.SkillAt(Slot);
  double Own=Recharges.Contains(Id)?Math::Max(0.0,Recharges[Id]-Hero.CharacterEvents.SimulationClock()):0;return Math::Max(Own,UOpusRecentGameplay::SkillCooldown(Hero.Rules,Slot));
 }
 void Released()
 {
  double Seconds=Hero.CharacterEvents.SheetStat(n"skill.independent_cooldown");if(Seconds<=0)return;
  Recharges.Add(Hero.DeliveryPolicy.SelectedSkill(),Hero.CharacterEvents.SimulationClock()+Seconds);
 }
 UFUNCTION() bool CanReceiveDamage()
 {
  return Hero==nullptr||Hero.Rules.GetPlayerPools().bDead||Hero.DeliveryPolicy.LiveStat(n"defense.damage_immunity")<=0;
 }
 UFUNCTION() void ProjectileMotion(int Index,float DeltaSeconds)
 {
  if(Hero==nullptr||Hero.Rules.GetPlayerPools().bDead)return;
  auto P=Hero.DeliveryPolicy.Projectile(Index);if(!P.Valid)return;
  // Launch handling belongs to the AS host. The rule VM still creates and
  // simulates the actual projectile, including its authored speed and lifetime.
  if(!P.Hostile&&P.Kind==n"whirlwind"&&P.Age<0.000001)
  {
   FRandomStream Launch;Launch.Initialize(P.Id*7919+Hero.Visual.CastSerial*1297);double Angle=Launch.RandRange(-4500,4500)/100.0;
   Hero.DeliveryPolicy.SetVelocity(Index,P.Velocity.RotateAngleAxis(Angle,FVector::UpVector));return;
  }
  if(!P.Hostile||Hero.DeliveryPolicy.LiveStat(n"projectile.hostile_attraction")<=0)return;
  FVector Toward=(Hero.CombatLocation()-P.Position).GetSafeNormal2D();double Speed=P.Velocity.Size2D();
  if(Toward.IsNearlyZero()||Speed<=0)return;
  Hero.DeliveryPolicy.SetVelocity(Index,FVector(Toward.X*Speed,Toward.Y*Speed,P.Velocity.Z));
 }
}
