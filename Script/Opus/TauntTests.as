// Real transient battle: native cast notifies, native status lifetimes, and
// enemy/allied carriers pass through the normal serial motion and hit policies.
class UDWOTauntTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Enemy;
 int Stage=0,BeforeCasts=0,ProbePlan=-1,HostileOwner=-1,FriendlyOwner=-1,OldId=0,AllyId=0,NewId=0;
 double Clock=0,Total=0,LastClock=0,ReleasedAt=0,PauseRemaining=0;
 bool Done=false;
 bool Check(bool Good,FString Why){if(Good)return true;Done=true;UOpusNative::WriteLog("TAUNT_TEST_FAIL "+Why);UOpusNative::Quit(false);return false;}
 void Pass(FString Name){UOpusNative::WriteLog("TAUNT_TEST_PASS "+Name);}
 void Next(int Value){Stage=Value;Clock=0;}
 void Shot(FString Name){if(UOpusNative::Flag("DWOTauntVisual"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/Taunt/"+Name+".png -nosuffix");}
 void Initialize(ADWOTowerGameMode T){Tower=T;T.Hero.ControlProbe=true;T.Rules.SetPlayerGod(true);T.StartChallenge();}
 void Probe(bool Hostile,FVector Offset)
 {
  auto H=Tower.Hero;int Owner=Hostile?HostileOwner:FriendlyOwner;FVector Origin=H.CombatLocation()+Offset,Direction=FVector(0,1,0);
  Tower.Rules.UpdateOwner(Owner,Origin,Direction);Tower.Rules.CastBuildPower(ProbePlan,Owner,Origin,Direction,10);
 }
 FOpusProjectileState Find(int Id)
 {
  auto D=Tower.Hero.DeliveryPolicy;for(int I=0;I<D.ProjectileCount();I++){auto P=D.Projectile(I);if(P.Valid&&P.Id==Id)return P;}return FOpusProjectileState();
 }
 int Latest(bool Hostile)
 {
  int Id=0;auto D=Tower.Hero.DeliveryPolicy;for(int I=0;I<D.ProjectileCount();I++){auto P=D.Projectile(I);if(P.Valid&&P.Hostile==Hostile)Id=Math::Max(Id,P.Id);}return Id;
 }
 bool Pulled(int Id)
 {
  auto P=Find(Id);FVector Toward=(Tower.Hero.CombatLocation()-P.Position).GetSafeNormal2D();
  FVector Direction=P.Velocity.GetSafeNormal2D();
  return Check(P.Valid&&P.Hostile&&Math::Abs(P.Velocity.Size2D()-1300)<1&&Direction.X*Toward.X+Direction.Y*Toward.Y>.999,"enemy carrier homes without changing speed id="+Id);
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;auto H=Tower.Hero;auto V=H.Visual;auto S=H.SelfSkills;
  double Now=H.CharacterEvents.SimulationClock();Clock+=Stage==7?double(Dt):Math::Max(0.0,Now-LastClock);LastClock=Now;Total+=Dt;
  if(Total>110){Check(false,"timeout stage="+Stage);return;}
  if(Stage==0)
  {
   for(auto E:Tower.Enemies)if(IsValid(E)){E.SetActorTickEnabled(false);if(Enemy==nullptr)Enemy=E;}
   if(!Tower.Preparing&&V.Ready&&H.CharacterMovement.MovementMode==EMovementMode::MOVE_None)H.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
   if(Tower.Preparing||!V.Ready||Enemy==nullptr||!H.CharacterMovement.IsMovingOnGround()){Clock=0;return;}
   if(Clock<4)return;FString Error;bool Assigned=Tower.Rules.ChangePlayerSkill(1,n"taunt",Error);
   if(!Check(Assigned,"fixed character equipment grants selectable taunt: "+Error))return;
   Tower.Rules.SelectPlayerSkill(1,Error);
   if(!Check(S.Ready&&S.SelfTarget()&&H.CharacterEvents.SheetStat(n"skill.independent_cooldown")==10,"composed self targeting and ten-second independent recharge"))return;
   for(auto A:UOpusData::AffixPool(Tower.Rules))if(!Check(A.Id!="damage_immunity"&&A.Id!="hostile_projectile_attraction"&&A.Id!="self_targeted_cast"&&A.Id!="independent_skill_cooldown","intrinsic capabilities stay outside random gear rolls"))return;
   TArray<FName> Traits;Traits.Add(n"monster_bolt");ProbePlan=Tower.Rules.CompileTraits(n"monster_bolt",Traits,Error);
   if(!Check(ProbePlan>=0,"native projectile probe compiles: "+Error))return;
   HostileOwner=Tower.Rules.RegisterOwner(90001,1);FriendlyOwner=Tower.Rules.RegisterOwner(90002,0);
   Probe(true,FVector(1500,500,0));Probe(false,FVector(1500,-500,0));Pass("CompositionAndFixedEquipmentGrant");Next(1);return;
  }
  if(Stage==1)
  {
   if(Clock<.1)return;OldId=Latest(true);AllyId=Latest(false);
   if(!Check(OldId>0&&AllyId>0&&Find(OldId).Velocity.X==0,"enemy and allied projectiles exist before taunt"))return;
   Tower.Rules.SetPlayerGod(false);H.AimPoint=H.CombatLocation()+FVector(-2500,-2300,0);H.Aim=FVector(-1,0,0);BeforeCasts=V.CastSerial;
   if(!Check(H.ResolveCastTarget(H.AimPoint)==nullptr&&V.RequestCast(H.CombatLocation()+H.Aim*60,H.AimPoint,0),"self skill starts its original cast animation without an enemy target"))return;
   Next(2);return;
  }
  if(Stage==2)
  {
   if(V.CastSerial==BeforeCasts||Tower.Rules.GetStatus(1,n"taunt")<=0)return;
   ReleasedAt=Now;if(!Check(S.Remaining()>9.8&&Tower.Rules.GetStatusRemaining(1,n"taunt")>1.8,"real attack release applies two-second status and starts own cooldown"))return;
   auto P=Tower.Rules.GetPlayerPools();double Meter=H.Stun.Meter.Amount;
   H.CharacterEvents.TestHit(10000);H.CharacterEvents.TestHit(10000,true);auto Q=Tower.Rules.GetPlayerPools();
   if(!Check(Q.Life==P.Life&&Q.Shield==P.Shield&&!Q.bDead&&H.Stun.Meter.Amount==Meter,"active immunity prevents hit and DOT damage, death and stun buildup"))return;
   Probe(true,FVector(-1300,500,0));Pass("SelfCastAndHitDotImmunity");Next(3);return;
  }
  if(Stage==3)
  {
   if(Clock<.12)return;NewId=Latest(true);if(!Pulled(OldId)||!Pulled(NewId))return;
   auto Ally=Find(AllyId);if(!Check(Ally.Valid&&!Ally.Hostile&&Math::Abs(Ally.Velocity.X)<.01&&Math::Abs(Ally.Velocity.Y-1300)<.1,"allied projectile direction is unchanged"))return;
   TArray<FString> Live;Tower.MonsterHealthHUD.RefreshPlayer(0,Live);
   if(!Check(Tower.MonsterHealthHUD.StatusBadges.Contains("1:taunt"),"saved SVG taunt badge is bound to the live native status"))return;
   Shot("taunt-active");Next(4);return;
  }
  if(Stage==4)
  {
   if(Now-ReleasedAt<1.85)return;
   if(!Check(Tower.Rules.GetStatus(1,n"taunt")>0,"taunt remains active before the two-second boundary"))return;
   Next(5);return;
  }
  if(Stage==5)
  {
   if(Tower.Rules.GetStatus(1,n"taunt")>0)return;
   if(!Check(Now-ReleasedAt>1.85&&Now-ReleasedAt<2.15,"native status expires at two seconds"))return;
   auto P=Tower.Rules.GetPlayerPools();H.CharacterEvents.TestHit(10);if(!Check(Tower.Rules.GetPlayerPools().Life<P.Life,"damage resumes after the status expires"))return;
   Probe(true,FVector(1500,600,0));Next(6);return;
  }
  if(Stage==6)
  {
   if(Clock<.1)return;auto Unpulled=Find(Latest(true));
   if(!Check(Unpulled.Valid&&Math::Abs(Unpulled.Velocity.X)<.01,"new projectile is no longer pulled after expiry"))return;
   Pass("ExistingNewHostileProjectilesAndExpiry");FString Error;Tower.Rules.SelectPlayerSkill(0,Error);
   Enemy.SetActorLocation(H.CombatLocation()+FVector(450,0,0));H.Aim=FVector(1,0,0);H.AimPoint=Enemy.ActorLocation;
   if(!Check(S.CanCast()&&V.RequestCast(H.CombatLocation()+H.Aim*60,H.AimPoint,30),"other active skill can cast during taunt cooldown"))return;
   V.MovementPressed();Tower.Rules.SelectPlayerSkill(1,Error);
   if(!Check(S.Remaining()>7&&S.Remaining()<8.1&&!S.CanCast(),"switching back retains the original taunt cooldown"))return;
   Tower.Rules.ChangePlayerSkill(1,NAME_None,Error);Tower.Rules.ChangePlayerSkill(1,n"taunt",Error);
   if(!Check(!S.CanCast(),"removing and reassigning the same skill cannot reset its cooldown"))return;
   PauseRemaining=S.Remaining();Tower.SetPaused(true);Next(7);return;
  }
  if(Stage==7)
  {
   if(Clock<.2)return;if(!Check(Math::Abs(S.Remaining()-PauseRemaining)<.001,"pause freezes independent cooldown"))return;
   Tower.SetPaused(false);Shot("taunt-cooldown");Next(8);return;
  }
  if(Stage==8)
  {
   if(!S.CanCast())return;H.AimPoint=H.CombatLocation()+FVector(-2500,-2300,0);BeforeCasts=V.CastSerial;
   if(!Check(Now-ReleasedAt>=9.8&&Now-ReleasedAt<10.2&&V.RequestCast(H.CombatLocation()+H.Aim*60,H.AimPoint,0),"taunt becomes available again at ten seconds"))return;
   Next(9);return;
  }
  if(Stage==9)
  {
   if(V.CastSerial==BeforeCasts||Tower.Rules.GetStatus(1,n"taunt")<=0)return;
   if(!Check(S.Remaining()>9.7,"successful recast starts a fresh cooldown"))return;
   Pass("IndependentCooldownSwitchPauseAndRecast");Done=true;UOpusNative::Quit(true);
  }
 }
}
