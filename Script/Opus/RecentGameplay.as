// Recent abilities use the native rule carriers. AS owns input and temporary actors.
class ADWOTemporaryEnemy : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Body;
 UPROPERTY(DefaultComponent,Attach=Root) UStaticMeshComponent Pupil;
 UPROPERTY(DefaultComponent) UDWRuleTargetComponent RuleTarget;
 default Body.SetCollisionProfileName(n"NoCollision");
 default Pupil.SetCollisionProfileName(n"NoCollision");
 default RuleTarget.Team=1;
 UPROPERTY() ADWOTowerGameMode Tower;
 int EntityId=0;
 double Expires=0;
 bool Registered=false;
 double Speed=0,FollowDistance=0,Radius=35,StopDistance=20;
 void Initialize(ADWOTowerGameMode T,int Id,FOpusMonsterEmission Request)
 {
  Tower=T;EntityId=Id;FString Error;FOpusMonsterData Definition;bool Found=false;
  for(auto M:UOpusNative::Monsters(T.Rules))if(M.Id==Request.Kind){Definition=M;Found=true;break;}
  Speed=Definition.Speed;FollowDistance=Definition.FollowDistance;StopDistance=Definition.StopDistance;Radius=35*Definition.Scale;RuleTarget.RadiusCm=Radius;
  TArray<FOpusSkillData> Added;
  if(!Found||!UOpusNative::RegisterEnemy(T.Rules,this,Id,Definition,FName(),T.Run.Floor,float32(Radius),"",Added,Error))
  {UOpusRecentGameplay::ReleaseTemporaryMonster(T.Rules,Id);DestroyActor();T.LastError=Error;return;}
  Registered=true;UOpusRecentGameplay::SetTargetInvulnerable(T.Rules,Id,Request.Invulnerable);
  Expires=T.Hero.CharacterEvents.SimulationClock()+Request.Duration;
  auto Mesh=Cast<UStaticMesh>(LoadObject(nullptr,"/Engine/BasicShapes/Sphere.Sphere"));Body.SetStaticMesh(Mesh);Body.SetRelativeScale3D(FVector(.7*Definition.Scale));
  Pupil.SetStaticMesh(Mesh);Pupil.SetRelativeLocation(FVector(32*Definition.Scale,0,0));Pupil.SetRelativeScale3D(FVector(.13,.3,.4)*Definition.Scale);
  UOpusNative::Tint(Body,FLinearColor(.55,.12,.85));UOpusNative::Tint(Pupil,FLinearColor(.025,.015,.04));
 }
 void Release()
 {if(Registered&&Tower!=nullptr){Registered=false;UOpusRecentGameplay::ReleaseTemporaryMonster(Tower.Rules,EntityId);}DestroyActor();}
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason)
 {if(Registered&&Tower!=nullptr&&Tower.Rules!=nullptr){Registered=false;UOpusRecentGameplay::ReleaseTemporaryMonster(Tower.Rules,EntityId);}}
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(!Registered||Tower==nullptr||Tower.Hero==nullptr)return;
  if(Tower.Traveling||Tower.Rules.GetPlayerPools().bDead||Tower.Hero.CharacterEvents.SimulationClock()>=Expires){Release();return;}
  if(!Tower.CanFight())return;
  FVector Target=Tower.Hero.CombatLocation();double Dt=Math::Clamp(DeltaSeconds,0.f,.05f);
  if(Speed>0&&FollowDistance>0)
  {
   FVector Heading=Tower.Hero.GetVelocity();if(Heading.Size2D()<1)Heading=Tower.Hero.Aim;
   FVector Behind=Target-Heading.GetSafeNormal2D()*FollowDistance;FVector Floor;
   if(UOpusDungeon::Project(Tower,Behind,Floor))Behind=Floor+FVector(0,0,60);
   FVector Delta=Behind-ActorLocation,Push;double Pace=Math::Min(1.0,Tower.Rules.GetTargetPools(EntityId).MoveScale);
   for(auto Other:Tower.Recent.Temporary)if(IsValid(Other)&&Other!=this){FVector Away=ActorLocation-Other.ActorLocation;double D=Away.Size2D(),Apart=Radius+Other.Radius+5;if(D>1&&D<Apart)Push+=Away.GetSafeNormal2D()*(1-D/Apart);}
   double Distance=Delta.Size2D(),Move=Math::Min(Speed*Pace*Dt,Math::Max(0.0,Distance-StopDistance));
   FVector Next=ActorLocation+Delta.GetSafeNormal2D()*Move+Push*120*Pace*Dt;float At=DWO3CGeometry::Sweep(ActorLocation,Next,float32(Radius));if(At>=0)Next=ActorLocation+(Next-ActorLocation)*Math::Max(0.f,At-.01f);SetActorLocation(Next);
  }
  SetActorRotation((Target-ActorLocation).Rotation());
  bool Clear=DWO3CGeometry::Sweep(ActorLocation,Target,1)<0;
  Tower.Rules.TickMonsterSkill(EntityId,ActorLocation,Target,Math::Clamp(DeltaSeconds,0.f,.05f),Clear);
 }
}
class UDWORecentGameplay : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TArray<ADWOTemporaryEnemy> Temporary;
 TArray<FOpusAuraChoice> Choices;
 TArray<FName> AuraSlots;
 TArray<bool> Enabled;
 FString Applied;
 bool Ready=false;
 UPROPERTY() FOpusGameplayState State;
 UPROPERTY() UDWORecentUI UI;
 void Initialize(ADWOTowerGameMode T)
 {
  Tower=T;FString Error;if(!UOpusGameplayStore::Decode(UDWOTowerJourney::Get().PlayerJson,State,Error)){T.Fail(Error);return;}
  AuraSlots=State.AuraSlots;Enabled=State.AuraEnabled;AuraSlots.SetNum(4);Enabled.SetNum(4);Ready=true;SyncAuras();
  UI=UDWORecentUI::Create(T);UI.Initialize(this);
 }
 FString Encode(FString Profile){State.AuraSlots=AuraSlots;State.AuraEnabled=Enabled;return UOpusGameplayStore::Encode(Profile,State);}
 bool Persist(FString& Error){if(UOpusNative::Flag("DWOTransient"))return true;if(UOpusNative::WriteProfile(Tower.MapItems.Snapshot()))return true;Error="玩法存档保存失败，操作已取消";return false;}
 void FirstFloorCleared()
 {
  if(State.FirstFloorComplete||Tower.Run.Floor!=1||Tower.Run.Phase!=EDWOTowerPhase::Cleared)return;auto Before=State;FString Error;
  if(!UOpusGameplayStore::RollEquipment(Tower.Rules,n"weapon",1,3,UDWOTowerJourney::Get().Seed+17923,State.RewardCandidates,Error)){Tower.LastError=Error;return;}
  State.FirstFloorComplete=true;if(!Persist(Error)){State=Before;Tower.LastError=Error;}
 }
 bool Claim(int Index)
 {
  if(!State.FirstFloorComplete||State.RewardClaimed||!State.RewardCandidates.IsValidIndex(Index))return false;
  FString Before=Tower.Rules.ExportPlayerState();auto Pools=Tower.Rules.GetPlayerPools();auto Saved=State;FString Error;
  if(!UOpusGameplayStore::Grant(Tower.Rules,State.RewardCandidates[Index],Error)){Tower.LastError=Error;return false;}
  State.RewardClaimed=true;State.ClaimedChoice=Index;if(!Persist(Error)){FString Ignored;Tower.Rules.ImportPlayerState(Before,Ignored);UOpusNative::CarryPools(Tower.Rules,Pools);State=Saved;Tower.LastError=Error;return false;}
  Tower.Hud.RefreshIn=0;return true;
 }
 void SyncAuras()
 {
  if(!Ready||Tower.Rules==nullptr)return;
  Choices=UOpusRecentGameplay::AuraChoices(Tower.Rules);TArray<FName> Active;FString Key;
  for(int I=0;I<4;I++)
  {
   bool Found=false;for(auto C:Choices)if(C.Id==AuraSlots[I])Found=true;
   if(!Found){AuraSlots[I]=FName();Enabled[I]=false;}
   if(Enabled[I]&&!AuraSlots[I].IsNone())Active.Add(AuraSlots[I]);Key+=AuraSlots[I].ToString()+"="+(Enabled[I]?"1":"0")+";";
  }
  if(Key==Applied)return;FString Error;
  if(!UOpusRecentGameplay::ApplyAuras(Tower.Rules,Active,Error)){Tower.LastError=Error;return;}
  Applied=Key;
 }
 bool EquipAura(int Slot,FName Id)
 {
  if(Slot<0||Slot>=4)return false;bool Found=Id.IsNone();for(auto C:Choices)if(C.Id==Id)Found=true;if(!Found)return false;
  for(int I=0;I<4;I++)if(AuraSlots[I]==Id&&!Id.IsNone()){AuraSlots[I]=FName();Enabled[I]=false;}
  AuraSlots[Slot]=Id;Enabled[Slot]=false;SyncAuras();Tower.SaveProfile();return true;
 }
 bool Toggle(int Slot)
 {
  SyncAuras();if(Slot<0||Slot>=4||AuraSlots[Slot].IsNone())return false;
  bool Previous=Enabled[Slot];Enabled[Slot]=!Previous;FString Before=Applied;SyncAuras();
  if(Before==Applied){Enabled[Slot]=Previous;return true;}Tower.SaveProfile();return true;
 }
 void AfterStep()
 {
  if(!Ready)return;
  for(auto Request:UOpusRecentGameplay::DrainMonsterEmissions(Tower.Rules))for(int I=0;I<Request.Count;I++)
  {
   double Angle=Request.Angle+6.283185307179586*I/Math::Max(1,Request.Count);
   FVector Pos=Request.Origin+FVector(Math::Cos(Angle)*Request.Radius,Math::Sin(Angle)*Request.Radius,0),Projected;
   if(UOpusDungeon::Project(Tower,Pos,Projected))Pos=Projected+FVector(0,0,60);
   auto Actor=Cast<ADWOTemporaryEnemy>(SpawnActor(ADWOTemporaryEnemy,Pos));
   if(Actor!=nullptr){Actor.Initialize(Tower,Tower.NextId++,Request);Temporary.Add(Actor);}
   else UOpusRecentGameplay::ReleaseTemporaryMonster(Tower.Rules,0);
  }
  for(int I=Temporary.Num()-1;I>=0;I--)if(!IsValid(Temporary[I]))Temporary.RemoveAt(I);
 }
}
