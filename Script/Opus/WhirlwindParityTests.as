// Transient diagnostic: one manual native cast with the real shipping T1 preset.
// High-health stationary targets keep hits available throughout its eight-second life.
class UDWOWhirlwindParityTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0,Peak=0,Initial=0,Sample=0,MaxCenters=0,MaxHeadings=0,MaxGrounds=0,MaxTinted=0;
 double Age=0,Total=0;
 bool Done=false,Captured=false,GroundOnlyCaptured=false;
 void Initialize(ADWOTowerGameMode T){Tower=T;T.Hero.ControlProbe=true;}
 UFUNCTION() float32 NoWall(FVector From,FVector To,float32 Radius){return -1;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Total+=Dt;
  if(Total>90){Done=true;UOpusNative::WriteLog("WHIRLWIND_PARITY_FAIL timeout");UOpusNative::Quit(false);return;}
  if(!Tower.CanUseSkills())return;
  auto R=Tower.Rules;auto H=Tower.Hero;
  if(Stage==0)
  {
   FString Error;FName Preset=UOpusNative::Flag("DWOWhirlwindGround")?n"whirlwind_grounds":n"whirlwind_swarm";if(!UOpusNative::ApplyPreset(R,Preset,Error)){Done=true;UOpusNative::WriteLog("WHIRLWIND_PARITY_FAIL "+Error);UOpusNative::Quit(false);return;}
   R.RespawnPlayer();H.AutoCast=false;R.SetPlayerCasting(false);R.ClearStaticTargets();
   if(!UOpusNative::Flag("DWOWhirlwindDungeon"))UOpusNative::SetGeometry(R,FOpusSweep(this,n"NoWall"));
   FVector Origin=H.CombatLocation();int Targets=UOpusNative::IntOption("DWOWhirlwindTargets",20);
   for(int I=0;I<Targets;I++){double Angle=I*2.399963,Radius=3+8*Math::Sqrt((I+.5)/Targets);FVector At=Origin+FVector(Math::Cos(Angle)*Radius*100,Math::Sin(Angle)*Radius*100,0);if(UOpusNative::Flag("DWOWhirlwindDungeon")){FVector Floor,Normal;if(!UOpusDungeon::Surface(this,At,Floor,Normal))continue;At=Floor+FVector(0,0,60);if(UOpusDungeon::Sweep(this,At,At,45)>=0)continue;}R.AddStaticTarget(At,45,1000000000,1);}
   H.Aim=FVector(1,0,0);H.AimPoint=Origin+H.Aim*1000;FVector Launch=H.SelfSkills.LaunchOrigin(Origin+H.Aim*60,H.Aim);R.UpdateOwner(H.OwnerSlot,Launch,H.Aim);UOpusNative::TargetPoint(R,H.OwnerSlot,H.AimPoint);
   int Root=R.CastBuildPower(UOpusNative::ActivePlan(R),H.OwnerSlot,Launch,H.Aim,30);
   UOpusNative::WriteLog("WHIRLWIND_PARITY_BEGIN targets="+Targets+" origin="+Origin+" root="+Root);
   Stage=1;Age=0;return;
  }
  Age+=Math::Clamp(double(Dt),0.0,.05);int Count=R.GetFrameReport().Projectiles;Peak=Math::Max(Peak,Count);
  auto GroundViews=UOpusRecentGameplay::Grounds(R);int Tinted=0;for(int I=0;I<H.DeliveryPolicy.ProjectileCount();I++){auto P=H.DeliveryPolicy.Projectile(I);if(P.Valid&&!P.Hostile&&P.GroundTint>=0)Tinted++;}MaxGrounds=Math::Max(MaxGrounds,GroundViews.Num());MaxTinted=Math::Max(MaxTinted,Tinted);
  if(!Captured&&Age>1&&GroundViews.Num()>=2&&Tinted>0&&UOpusNative::Flag("DWOWhirlwindGround")&&UOpusNative::Flag("DWOWhirlwindVisual")){Captured=true;System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/WhirlwindParity/DungeonGround/ground-contact.png -nosuffix");}
  if(!GroundOnlyCaptured&&Count==0&&MaxTinted>0&&GroundViews.Num()>0&&UOpusNative::Flag("DWOWhirlwindGround")&&UOpusNative::Flag("DWOWhirlwindVisual")){GroundOnlyCaptured=true;System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/WhirlwindParity/DungeonGround/ground-after-winds.png -nosuffix");}
  if(!Captured&&Age>=6&&UOpusNative::Flag("DWOWhirlwindVisual")){Captured=true;FString Phase=UOpusNative::Flag("DWOWhirlwindBefore")?"before":"after";FString Sub=UOpusNative::Flag("DWOWhirlwindGround")?"DungeonGround/":"";System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/WhirlwindParity/"+Sub+"native-"+Phase+".png -nosuffix");}
  if(Initial==0&&Count>0)Initial=Count;
  if(Age>=Sample){TArray<int> Headings;TArray<FVector> Centers;auto D=H.DeliveryPolicy;for(int I=0;I<D.ProjectileCount();I++){auto P=D.Projectile(I);if(!P.Valid||P.Hostile)continue;int Heading=Math::RoundToInt(Math::Atan2(P.Velocity.Y,P.Velocity.X)*180/PI/5);if(!Headings.Contains(Heading))Headings.Add(Heading);bool Separate=true;for(auto Center:Centers)if((Center-P.Position).Size2D()<100)Separate=false;if(Separate)Centers.Add(P.Position);}MaxCenters=Math::Max(MaxCenters,Centers.Num());MaxHeadings=Math::Max(MaxHeadings,Headings.Num());UOpusNative::WriteLog("WHIRLWIND_PARITY_SAMPLE seconds="+Sample+" live="+Count+" mirrors="+UOpusRecentGameplay::Mirrors(R)+" headingBins="+Headings.Num()+" separatedCenters="+Centers.Num());Sample++;}
  if(Age>=10){Done=true;bool GroundOk=!UOpusNative::Flag("DWOWhirlwindGround")||(MaxGrounds>0&&MaxTinted>0);bool Ok=!UOpusNative::Flag("DWOWhirlwindAssert")||(Initial==3&&Peak>3&&MaxCenters>5&&MaxHeadings>5&&Count==0&&GroundOk);UOpusNative::WriteLog((Ok?"WHIRLWIND_PARITY_DONE ":"WHIRLWIND_PARITY_FAIL ")+"initial="+Initial+" peak="+Peak+" mirrors="+UOpusRecentGameplay::Mirrors(R)+" remaining="+Count+" maxCenters="+MaxCenters+" maxHeadings="+MaxHeadings+" maxGrounds="+MaxGrounds+" maxTinted="+MaxTinted);UOpusNative::Quit(Ok);}
 }
}
