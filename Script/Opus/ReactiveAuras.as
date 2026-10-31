// Generic radial retaliation: rule sheet owns tuning; AS owns wave lifetime/hit sets.
class ADWOReflectedWindWave : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UNiagaraComponent Wind;
 default Wind.bAutoActivate=false;
 UPROPERTY() ADWOHero Hero;
 TArray<int> Victims;
 double Amount=0,Radius=0,Speed=0,Age=0,LastClock=0,Front=0;
 bool Finished=false,Ended=false;
 void Initialize(ADWOHero H,double Damage,double RadiusCm,double SpeedCm)
 {
  Hero=H;Amount=Damage;Radius=RadiusCm;Speed=SpeedCm;LastClock=H.CharacterEvents.SimulationClock();
  Wind.SetAsset(H.ReactiveAuras.WindSystem);
  Wind.SetVariableBool(n"User.AudioOn",false);Wind.SetVariableBool(n"User.LightOn",false);Wind.SetVariableBool(n"User.Optimized",true);
  Wind.SetVariableFloat(n"User._ColorHue",0);Wind.SetVariableFloat(n"User._Size",1);Wind.SetVariableFloat(n"User._Tall",.12);
  Wind.SetVariableFloat(n"User.Duration",.7);Wind.SetVariableFloat(n"User.Saturation",1);Wind.SetVariableFloat(n"User.Brightness",.65);
  Wind.SetVariableLinearColor(n"User.Color01",FLinearColor(.7,.92,1,.8));
  Wind.SetVariableLinearColor(n"User.Color2",FLinearColor(.42,.78,.9,.65));
  Wind.SetVariableLinearColor(n"User.ColorBottom",FLinearColor(.03,.06,.07,.1));
  Wind.SetRelativeScale3D(FVector(.03,.03,.05));Wind.Activate(true);
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Hero==nullptr||Hero.Tower==nullptr||Hero.Tower.Traveling||Hero.Rules.GetPlayerPools().bDead){Ended=true;DestroyActor();return;}
  double Clock=Hero.CharacterEvents.SimulationClock(),Dt=Math::Max(0.0,Clock-LastClock);LastClock=Clock;
  if(Dt<=0||Hero.Tower.Paused||Hero.Tower.Preparing)return;
  Age+=Dt;
  if(!Finished)
  {
   Front=Math::Min(Radius,Age*Speed);
   for(int Id:Hero.ReactiveCombat.ReflectedPulse(ActorLocation,Front,Amount,Victims))Victims.Add(Id);
   // The source whirlwind has a one-metre half-scale footprint. Flatten into a
   // low wind sheet and grow with the authoritative hit front.
   double Scale=Math::Max(.03,Front/200.);Wind.SetRelativeScale3D(FVector(Scale,Scale,.09));
   if(Front>=Radius){Finished=true;Wind.Deactivate();}
  }
  if(Age>=Radius/Math::Max(1.0,Speed)+.25){Ended=true;DestroyActor();}
 }
}
class UDWOReactiveAuras : UActorComponent
{
 UPROPERTY() ADWOHero Hero;
 UPROPERTY() UNiagaraSystem WindSystem;
 UPROPERTY() UNiagaraComponent Warmup;
 double WarmupAge=0;
 UPROPERTY() TArray<ADWOReflectedWindWave> Waves;
 bool Initialized=false;
 int Emitted=0;
 double LastOriginal=0,LastReflected=0;
 void Initialize(ADWOHero H)
 {
  if(Initialized)return;Hero=H;
  if(!H.ReactiveCombat.BindPlayer(H.Rules)){H.Tower.Fail("Cannot bind reactive aura event transport");return;}
  H.ReactiveCombat.OnLandedAttack.AddUFunction(this,n"OnLandedAttack");Initialized=true;
  Preload();
 }
 void Preload()
 {
  if(WindSystem!=nullptr||Hero==nullptr||Hero.Tower==nullptr||!Hero.Tower.Recent.Ready)return;
  for(auto Choice:Hero.Tower.Recent.Choices)if(Choice.Id==n"wind_breath")
  {
   WindSystem=Cast<UNiagaraSystem>(LoadObject(nullptr,"/Game/WebVFX/Systems/NS_OpusWhirlwind.NS_OpusWhirlwind"));
   if(WindSystem!=nullptr)
   {
    Warmup=UNiagaraComponent::Create(Hero);Warmup.SetVisibility(false);Warmup.SetAsset(WindSystem);
    Warmup.SetVariableBool(n"User.AudioOn",false);Warmup.SetVariableBool(n"User.LightOn",false);Warmup.Activate(true);
   }
   return;
  }
 }
 UFUNCTION() void OnLandedAttack(double OriginalDamage,bool Dot,bool Dead)
 {
  if(Dot||Dead||OriginalDamage<=0||Hero==nullptr)return;
  double Percent=Hero.CharacterEvents.SheetStat(n"retaliation.hit_percent");
  double Radius=Hero.CharacterEvents.SheetStat(n"retaliation.radius")*100;
  double Speed=Hero.CharacterEvents.SheetStat(n"retaliation.speed")*100;
  if(Percent<=0||Radius<=0||Speed<=0)return;
  Preload();if(WindSystem==nullptr){Hero.Tower.LastError="风之息缺少风效资源";return;}
  auto Wave=SpawnActor(ADWOReflectedWindWave,Hero.CombatLocation());
  if(Wave==nullptr)return;
  LastOriginal=OriginalDamage;LastReflected=OriginalDamage*Percent/100;
  Wave.Initialize(Hero,LastReflected,Radius,Speed);Waves.Add(Wave);Emitted++;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Preload();
  if(Warmup!=nullptr){WarmupAge+=Dt;if(WarmupAge>1)Warmup.Deactivate();}
  for(int I=Waves.Num()-1;I>=0;I--)if(!IsValid(Waves[I])||Waves[I].Ended)Waves.RemoveAt(I);
 }
}
