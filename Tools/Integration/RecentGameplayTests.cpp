// Installed into the project test module after the shared HUD verification window.
#include "DWRuleLibraryLoader.h"
#include "DWRuleSession.h"
#include "DWRuleModernCombat.h"
#include "DWRuleProjectileAffixes.h"
#include "DWRuleCombatTriggers.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#if WITH_DEV_AUTOMATION_TESTS
using namespace DWRules;
namespace
{
struct FRecentFixture
{
 FDWRuleHost Host;
 TUniquePtr<FDWRuleSession> Session;
 FString Error;
 int32 Owner=INDEX_NONE;
 bool Initialize()
 {
  FDWRuleLibrary Library;if(!FDWRuleLibraryLoader::LoadDirectory(FPaths::ProjectContentDir()/TEXT("Rules"),Library,Error)||!Host.SetLibrary(MoveTemp(Library),Error))return false;
  Owner=Host.World.AddOwner(1,0);Host.World.UpdateOwner(Owner,FVector3f(0,0,.6f),FVector3f(1,0,0));
  Host.World.Targets().Add(FVector3f(0,0,.6f),.45f,400,0,1);
  Session=MakeUnique<FDWRuleSession>(Host,Owner,1,71);return true;
 }
 int32 Plan(FName Name){return Host.CompileNamedBuild(Name,{},Error);}
 bool Select(FName Name){const int32 P=Plan(Name);return P>=0&&Session->SetDirectPlan(P,Name,Error);}
 int32 Cast(){return Session->Cast(Session->Combat.Hero.Plan,FVector3f(0,0,.6f),FVector3f(1,0,0),30,73,Error);}
 void Step(int32 Frames=1)
 {
  for(int32 I=0;I<Frames;++I){const double Dt=1./60;Session->TickCharacter(Dt,false);Session->BeforeWorld(Dt);Host.Step(float(Dt));Session->AfterWorld();Host.World.Targets().Health[0]=float(Session->Character.Life);}
 }
 void Enemy(uint32 Id,FVector3f At,float Radius=.45f)
 {
  const int32 O=Host.World.AddOwner(Id,1);Host.World.UpdateOwner(O,At,FVector3f(-1,0,0));
  Host.World.Targets().Add(At,Radius,1000000,1,Id);FDWCombatEnemy E;E.Id=Id;E.Owner=O;E.Position=FVector3d(At);E.Health=E.MaxHealth=1000000;E.Radius=Radius;Session->Combat.Enemies.Add(E);
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecentEchoTest,"DungeonWorldOpus.Recent.Echo",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecentEchoTest::RunTest(const FString&)
{
 FRecentFixture F;if(!TestTrue(*F.Error,F.Initialize())){AddError(F.Error);return false;}
 FDWRuleLoadout Loadout;Loadout.Owned={{TEXT("frost_lance"),1},{TEXT("echo"),1}};Loadout.Skills[0]=TEXT("frost_lance");Loadout.Skills[1]=TEXT("echo");FDWCollectionPart Echo;Echo.Collection.Id=TEXT("echo_equipment_fixture");FDWTraitEntry EchoGrant;EchoGrant.Id=TEXT("grant_echo");Echo.Collection.Traits.Add(EchoGrant);Echo.bEquipment=true;Loadout.PassiveParts.Add(Echo);
 if(!TestTrue(TEXT("compose real slots"),F.Session->SetLoadout(Loadout,F.Error))){AddError(F.Error);return false;}
 F.Session->Select(1,F.Error);TestTrue(TEXT("cast self buff"),F.Cast()>=0);F.Step();
 TestTrue(TEXT("real echo status"),F.Host.World.GetStatusRemaining(1,StatusKey(TEXT("echo")))>4.9f);
 TestTrue(TEXT("independent cooldown starts"),F.Session->Combat.Cooldown>4.9);
 F.Session->Select(0,F.Error);TestTrue(TEXT("switching permits the attack"),F.Session->Combat.Cooldown<=0);
 const auto Before=F.Session->Character.Flow.ManaSpent;TestTrue(TEXT("manual attack"),F.Cast()>=0);const double Payment=F.Session->Character.Flow.ManaSpent-Before;F.Step();
 TestEqual(TEXT("full skill repeated once"),F.Host.World.Projectiles().Num,2);
 TestTrue(TEXT("status consumed"),F.Host.World.GetStatusRemaining(1,StatusKey(TEXT("echo")))<=0);
 TestTrue(TEXT("only one additional payment"),Payment>0&&Payment<3);
 TestTrue(TEXT("another ordinary cast"),F.Cast()>=0);F.Step();TestEqual(TEXT("no recursive/retained repeat"),F.Host.World.Projectiles().Num,3);
 F.Session->Select(1,F.Error);TestTrue(TEXT("switching back retains echo cooldown"),F.Session->Combat.Cooldown>4.8);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecentTreasuryTest,"DungeonWorldOpus.Recent.Treasury",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecentTreasuryTest::RunTest(const FString&)
{
 FRecentFixture F;if(!F.Initialize()||!F.Select(TEXT("royal_treasury"))){AddError(F.Error);return false;}
 TSet<uint32> Seen;TArray<double> Born;bool Origins=true;
 auto Step=[&](){F.Step();const auto& C=F.Host.World.Projectiles();for(int32 I=0;I<C.Num;++I)if(!Seen.Contains(C.Id[I])){Seen.Add(C.Id[I]);Born.Add(F.Host.World.GetClock());const FVector3f Center(-.6f,0,2.f);const auto* St=F.Session->Combat.Modern->Shots.Find(C.Id[I]);const auto D=(St?FVector3f(St->Origin):C.Position[I])-Center;Origins&=D.Z>=-.001f&&FMath::Abs(D.Size()-1.8f)<.01f;}};
 for(int32 Cast=0;Cast<10;++Cast){while(F.Session->Combat.Cooldown>0)Step();TestTrue(TEXT("manual stock increment"),F.Cast()>=0);Step();}
 const double Last=F.Host.World.GetClock();TArray<FDWMaintainedView> Views;F.Host.World.MaintainedSnapshot(F.Owner,Views);
 if(!TestEqual(TEXT("one maintained emitter"),Views.Num(),1))return false;
 TestEqual(TEXT("stock cap"),Views[0].Stacks,10);
 while(F.Host.World.GetClock()-Last<4.9)Step();F.Host.World.MaintainedSnapshot(F.Owner,Views);TestEqual(TEXT("no early decay"),Views[0].Stacks,10);
 while(F.Host.World.GetClock()-Last<5.1)Step();F.Host.World.MaintainedSnapshot(F.Owner,Views);TestEqual(TEXT("first decay after idle five seconds"),Views[0].Stacks,9);
 while(F.Host.World.GetClock()-Last<7.1)Step();F.Host.World.MaintainedSnapshot(F.Owner,Views);TestEqual(TEXT("subsequent two second decay"),Views[0].Stacks,8);
 TestTrue(TEXT("real random emissions"),Born.Num()>30);TestTrue(TEXT("actual hemisphere origins"),Origins);
 bool Unequal=false;for(int32 I=2;I<Born.Num();++I)Unequal|=FMath::Abs((Born[I]-Born[I-1])-(Born[I-1]-Born[I-2]))>.02;
 TestTrue(TEXT("not a uniform volley timer"),Unequal);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecentMirrorTest,"DungeonWorldOpus.Recent.Mirror",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecentMirrorTest::RunTest(const FString&)
{
 FRecentFixture F;if(!F.Initialize()||!F.Select(TEXT("whirlwind"))){AddError(F.Error);return false;}
 FDWModernGround G;G.Id=900;G.Type=TEXT("mirror");G.Position=FVector3d(1.8,0,0);G.Radius=.1;G.Left=4;F.Session->Combat.Modern->Grounds.Add(G);
 TestTrue(TEXT("cast real roaming projectile"),F.Cast()>=0);F.Step();F.Step();
 TestEqual(TEXT("one crossing creates one clone"),F.Session->Combat.Modern->Mirrors,1);
 const auto& C=F.Host.World.Projectiles();if(!TestEqual(TEXT("two actual carriers"),C.Num,2))return false;
 TestTrue(TEXT("remaining lifetime inherited"),FMath::IsNearlyEqual(C.Life[0]-C.Age[0],C.Life[1]-C.Age[1],.001f));
 F.Step(2);TestEqual(TEXT("residing/newborn overlap does not recurse"),F.Session->Combat.Modern->Mirrors,1);
 FRecentFixture High;if(!High.Initialize()||!High.Select(TEXT("whirlwind"))){AddError(High.Error);return false;}
 High.Host.World.UpdateOwner(High.Owner,FVector3f(0,0,4),FVector3f(1,0,0));High.Session->Combat.Modern->Grounds.Add(G);High.Cast();High.Step(3);
 TestEqual(TEXT("high carrier misses ground"),High.Session->Combat.Modern->Mirrors,0);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecentCaptureTest,"DungeonWorldOpus.Recent.Capture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecentCaptureTest::RunTest(const FString&)
{
 FRecentFixture F;if(!F.Initialize()){AddError(F.Error);return false;}
 FDWRuleLoadout L;L.Owned.Add(TEXT("frost_lance"),1);L.Skills[0]=TEXT("frost_lance");FDWCollectionPart Part;Part.Collection.Id=TEXT("test_capture");
 FDWTraitEntry Vector;Vector.Id=TEXT("vector_control");Part.Collection.Traits.Add(Vector);FDWTraitEntry Capture;Capture.Id=TEXT("projectile_capture");Capture.Params.Add(TEXT("chance"),100);Part.Collection.Traits.Add(Capture);
 FDWTraitEntry Damage;Damage.Id=TEXT("proj_damage");Damage.Params.Add(TEXT("n"),250);Part.Collection.Traits.Add(Damage);L.PassiveParts.Add(Part);
 if(!F.Session->SetLoadout(L,F.Error)){AddError(F.Error);return false;}
 TestTrue(TEXT("capture trigger is actually equipped"),F.Session->Combat.Has(TEXT("projectile_capture")));if(F.Session->Combat.Has(TEXT("projectile_capture")))TestEqual(TEXT("controlled capture chance"),F.Session->Combat.T(TEXT("projectile_capture"))[0]->Param(TEXT("chance")),100.);
 F.Enemy(9,FVector3f(5,0,.6f));const int32 EnemyOwner=F.Session->Combat.Enemies[0].Owner,EnemyPlan=F.Plan(TEXT("monster_twisted_eye_bolt"));
 F.Host.World.Cast(EnemyPlan,EnemyOwner,FVector3f(5,0,.6f),FVector3f(-1,0,0),82,10);
 bool Curved=false,ContactOrigin=false;double Life=F.Session->Character.Life;
 for(int32 N=0;N<100;++N){F.Step();const auto& C=F.Host.World.Projectiles();for(int32 I=0;I<C.Num;++I)if(F.Host.World.GetRoot(C.Root[I]).Owner==F.Owner){Curved|=FMath::Abs(C.Position[I].Y)>.05f;const auto* S=F.Session->Combat.Modern->Shots.Find(C.Id[I]);if(S)ContactOrigin|=FMath::Abs(S->Origin.X-.6)<.04;}}
 TestEqual(TEXT("exactly one interception"),F.Session->Combat.Modern->Captured,1);TestEqual(TEXT("original projectile delivered no hero damage"),F.Session->Character.Life,Life);
 TestTrue(TEXT("starts at actual collision"),ContactOrigin);TestTrue(TEXT("actual curved trajectory"),Curved);
 TestTrue(TEXT("returned carrier inherits player projectile damage"),1000000-F.Session->Combat.Enemies[0].Health>25);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecentReservationTest,"DungeonWorldOpus.Recent.Reservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecentReservationTest::RunTest(const FString&)
{
 FDWRuleCharacter C;const double Maximum=C.MaxMana,Recovered=C.Totals.ManaRecovered,Spent=C.Flow.ManaSpent;
 C.ReserveMana(50);TestEqual(TEXT("maximum remains the stat basis"),C.MaxMana,Maximum);TestEqual(TEXT("available pool capped"),C.Mana,Maximum*.5);
 C.Recover(ECharacterPool::Mana,100);TestEqual(TEXT("recovery cannot fill reserved mana"),C.Mana,Maximum*.5);
 TestEqual(TEXT("reservation did not emit recovery"),C.Totals.ManaRecovered,Recovered);TestEqual(TEXT("reservation did not spend mana"),C.Flow.ManaSpent,Spent);
 C.ReserveMana(0);TestEqual(TEXT("closing does not refill"),C.Mana,Maximum*.5);C.Recover(ECharacterPool::Mana,10);TestEqual(TEXT("normal recovery resumes"),C.Mana,Maximum*.5+10);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecentEaTest,"DungeonWorldOpus.Recent.TreasuryFinisher",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecentEaTest::RunTest(const FString&)
{
 auto Run=[&](int32 Layers,double& Damage)
 {
  FRecentFixture F;if(!F.Initialize()){AddError(F.Error);return false;}
  FDWRuleLoadout Loadout;Loadout.Owned={{TEXT("royal_treasury"),1},{TEXT("treasury_enuma_elish"),1}};Loadout.Skills[0]=TEXT("royal_treasury");Loadout.Skills[1]=TEXT("treasury_enuma_elish");Loadout.Equipment.Add(TEXT("weapon"),TEXT("royal_key"));
  FDWCollectionPart Part;Part.Collection.Id=TEXT("flat_fixture");FDWTraitEntry Flat;Flat.Id=TEXT("flat_physical");Flat.Params.Add(TEXT("n"),20);Part.Collection.Traits.Add(Flat);Loadout.PassiveParts.Add(Part);
  if(!F.Session->SetLoadout(Loadout,F.Error)){AddError(F.Error);return false;}
  for(int32 N=0;N<Layers;++N){while(F.Session->Combat.Cooldown>0)F.Step();if(F.Cast()<0){AddError(F.Error);return false;}F.Step();}
  F.Session->Select(1,F.Error);F.Enemy(9,FVector3f(6,4,.6f),.5f);F.Host.World.SetOwnerTargetPoint(F.Owner,FVector3f(6,4,.6f));
  if(F.Cast()<0){AddError(F.Error);return false;}F.Step();
  TArray<FDWMaintainedView> Views;F.Host.World.MaintainedSnapshot(F.Owner,Views);TestEqual(TEXT("finisher consumes all actual layers"),Views.Num(),0);
  bool HighOrigin=false,Downward=false;const auto& C=F.Host.World.Projectiles();
  for(int32 I=0;I<C.Num;++I)if(F.Host.World.GetPlan(F.Host.World.GetRoot(C.Root[I]).Plan).KindNames[C.Kind[I]]==TEXT("treasury_ea"))
  {HighOrigin=C.Position[I].Z>5.7;Downward=C.Velocity[I].Z<0;}
  TestTrue(TEXT("real high origin and downward flight"),HighOrigin&&Downward);
  F.Step(55);Damage=1000000-F.Session->Combat.Enemies[0].Health;TestTrue(TEXT("real contact damage"),Damage>32);
  const int32 Before=C.Num;while(F.Session->Combat.Cooldown>0)F.Step();F.Cast();F.Step();TestTrue(TEXT("zero layers cannot launch another finisher"),F.Host.World.Projectiles().Num<=Before);
  return true;
 };
 double One=0,Three=0;if(!Run(1,One)||!Run(3,Three))return false;
 TestTrue(TEXT("layer multiplier includes equipment flat damage"),FMath::Abs(Three/One-2.5/1.5)<.01);return true;
}
#endif
