// Transient integration probe: source presets, real combat carriers and the actual render host.
class UDWOModernCombatTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() ADWOEnemy Target;
 UPROPERTY() ADWOTemporaryEnemy Eye;
 FString Original;
 int Stage=0,Casts=0,Frames=0,BeforeKills=0,BeforeLoot=0;
 double Total=0,Age=0,NextCast=0,FollowBefore=0,SeenHighAt=-1;
 bool Done=false,SawVisible=false,SawHostile=false,SawHigh=false;
 void Initialize(ADWOTowerGameMode T){Tower=T;T.Hero.ControlProbe=true;Original=T.Rules.ExportPlayerState();T.Rules.SetPlayerGod(true);T.StartChallenge();}
 bool Check(bool Good,FString What){if(Good)return true;Done=true;UOpusNative::WriteLog("MODERN_TEST_FAIL "+What);UOpusNative::Quit(false);return false;}
 void Next(int S){Stage=S;Age=0;NextCast=0;Casts=0;}
 bool Equip(FName Base,FName Slot)
 {
  auto R=Tower.Rules;FString Error;TArray<FString> Before;for(auto E:UOpusData::Inventory(R))Before.Add(E.Id);
  bool Bought=R.BuyPlayerListing("equipment:"+Base,Error);if(!Check(Bought,"acquire "+Base+" "+Error))return false;
  for(auto E:UOpusData::Inventory(R))if(E.Kind=="equipment"&&!Before.Contains(E.Id))
  {
   FString Uid=E.Id;if(!E.Identified){bool Identified=R.IdentifyPlayerEquipment(FName(E.Id),Uid,Error);if(!Check(Identified,"identify "+Error))return false;}
   bool Equipped=R.EquipPlayerItem(Slot,FName(Uid),Error);return Check(Equipped,"equip instance "+Base+" "+Error);
  }
  return Check(false,"no acquired equipment instance "+Base);
 }
 void Shot(FString Name){System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/UEIntegration/"+Name+".png -nosuffix");}
 bool Rendered()
 {
  TArray<ADWRuleVisualHost> Hosts;GetAllActorsOfClass(Hosts);
  for(auto H:Hosts){TArray<UInstancedStaticMeshComponent> Parts;H.GetComponentsByClass(Parts);for(auto P:Parts)if(P.GetInstanceCount()>0&&P.IsVisible())return true;}return false;
 }
 bool Preset(FName Id){FString Error;bool Ok=UOpusNative::ApplyPreset(Tower.Rules,Id,Error);Tower.Rules.SetPlayerGod(true);Tower.Recent.SyncAuras();return Check(Ok,"source preset "+Id+" "+Error);}
 void Fire()
 {
  if(Age<NextCast)return;NextCast=Age+.1;auto H=Tower.Hero;
  H.AimPoint=Target.ActorLocation;H.Aim=(H.AimPoint-H.CombatLocation()).GetSafeNormal();
  Tower.Rules.UpdateOwner(H.OwnerSlot,H.CombatLocation()+H.Aim*60,H.Aim);UOpusNative::TargetPoint(Tower.Rules,H.OwnerSlot,H.AimPoint);
  if(Tower.Rules.CastBuildPower(UOpusNative::ActivePlan(Tower.Rules),H.OwnerSlot,H.CombatLocation()+H.Aim*60,H.Aim,30)>=0)Casts++;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Total+=Dt;Age+=Math::Clamp(double(Dt),0.0,.05);if(!Check(Total<150,"timeout stage="+Stage))return;
  auto H=Tower.Hero;auto R=Tower.Rules;auto G=Tower.Recent;FString Error;
  if(!H.Visual.Ready||Tower.Preparing||!Tower.Dungeon.Ready)return;
  if(H.CharacterMovement.MovementMode==EMovementMode::MOVE_None)H.CharacterMovement.SetMovementMode(EMovementMode::MOVE_Walking);
  if(Stage==0)
  {
   if(!Tower.CanFight())return;
   for(auto E:Tower.Enemies)if(IsValid(E)){E.SetActorTickEnabled(false);UOpusRecentGameplay::SetTargetInvulnerable(R,E.EntityId,true);if(Target==nullptr)Target=E;}
   if(!Check(Target!=nullptr,"real native encounter"))return;
   Target.SetActorLocation(H.CombatLocation()+FVector(650,0,0));Target.Body.SetStaticMesh(Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Cube.Cube")));Target.Body.SetRelativeScale3D(FVector(.6));
   if(!Preset(n"whirlwind_grounds"))return;Next(1);return;
  }
  if(Stage==1)
  {
   Fire();SawVisible=SawVisible||Rendered();
   if(Age>4&&Frames==0){Shot("whirlwind-combat");Frames++;}
   if(Age<10)return;if(!Check(Casts>=5&&SawVisible&&R.GetFrameReport().Projectiles>0,"whirlwind source build creates live rendered carriers"))return;
   UOpusNative::WriteLog("MODERN_TEST_PASS WhirlwindRendered casts="+Casts);if(!Preset(n"royal_treasury"))return;Next(2);return;
  }
  if(Stage==2)
  {
   if(Casts<10){Fire();return;}if(Age<1)return;
   if(!Check(R.SelectPlayerSkill(1,Error)&&H.SelfSkills.FreeTarget(),"royal weapon grants point-targeted finisher "+Error))return;
   H.AimPoint=Target.ActorLocation;H.Aim=(H.AimPoint-H.CombatLocation()).GetSafeNormal();
   if(!Check(H.Visual.RequestCast(H.CombatLocation()+H.Aim*60,H.AimPoint,30),"finisher uses original cast animation"))return;Next(3);return;
  }
  if(Stage==3)
  {
   auto D=H.DeliveryPolicy;for(int I=0;I<D.ProjectileCount();I++){auto P=D.Projectile(I);if(P.Valid&&!P.Hostile&&P.Position.Z>H.CombatLocation().Z+430&&P.Velocity.Z<0)SawHigh=true;}
   if(SawHigh&&SeenHighAt<0)SeenHighAt=Age;
   if(SawHigh&&Age-SeenHighAt>=.14){if(!Check(Rendered(),"finisher has actual native render batches"))return;Tower.SetPaused(true);Shot("treasury-finisher");Next(4);return;}
   if(Age>5)Check(false,"high downward EA carrier after animation release");return;
  }
  if(Stage==4)
  {
   if(Age<.5)return;Tower.SetPaused(false);UOpusNative::WriteLog("MODERN_TEST_PASS TreasuryFinisherAnimatedHighOriginRendered");
   if(!Check(R.ImportPlayerState(Original,Error),"restore original transient profile "+Error))return;R.SetPlayerGod(true);
   if(!Equip(n"twisted_eye_amulet",n"amulet"))return;R.SelectPlayerSkill(0,Error);
   BeforeKills=Tower.Kills;BeforeLoot=UOpusNative::Loot(R).Num();Next(5);return;
  }
  if(Stage==5)
  {
   Fire();for(auto E:G.Temporary)if(IsValid(E)){Eye=E;break;}
   if(Eye==nullptr){if(Age>20)Check(false,"source amulet never produced a temporary monster");return;}
   if(!Check(Eye.Registered&&Math::Abs(Eye.Radius-8.75)<.01&&Eye.Speed==1000&&Eye.FollowDistance==240,"real quarter-size temporary enemy reads own monster data"))return;
   H.Aim=FVector(0,-1,0);FVector Goal=H.CombatLocation()-H.Aim*240;FollowBefore=(Eye.ActorLocation-Goal).Size2D();Next(6);return;
  }
  if(Stage==6)
  {
   H.Aim=FVector(0,-1,0);auto D=H.DeliveryPolicy;for(int I=0;I<D.ProjectileCount();I++){auto P=D.Projectile(I);SawHostile=SawHostile||(P.Valid&&P.Hostile);}
   if(Age>.8&&Frames==1){if(!Check(IsValid(Eye)&&(Eye.ActorLocation-(H.CombatLocation()-H.Aim*240)).Size2D()<Math::Max(60.0,FollowBefore),"eye follows behind actual player"))return;Shot("twisted-eye-combat");Frames++;}
   if(Age<8)return;
   if(!Check(G.Temporary.Num()==0&&SawHostile&&Tower.Kills==BeforeKills&&UOpusNative::Loot(R).Num()==BeforeLoot,"real enemy bolts; five-second expiry without kill or drop"))return;
   Done=true;UOpusNative::WriteLog("MODERN_TEST_PASS TemporaryEyesFollowOwnProjectileExpiryNoRewards");UOpusNative::WriteLog("MODERN_TEST_PASS Complete");UOpusNative::Quit(true);return;
  }
 }
}
