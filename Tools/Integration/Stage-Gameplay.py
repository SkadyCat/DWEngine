from pathlib import Path
r=Path(__file__).resolve().parents[2]
s=(r/'Script/Opus/RecentGameplay.as').read_text(encoding='utf-8')
def rep(a,b):
 global s
 assert a in s,a[:90]
 s=s.replace(a,b)
rep(' bool Registered=false;',' bool Registered=false;\n double Speed=0,FollowDistance=0,Radius=35,StopDistance=20;')
rep('  TArray<FOpusSkillData> Added;', '  Speed=Definition.Speed;FollowDistance=Definition.FollowDistance;StopDistance=Definition.StopDistance;Radius=35*Definition.Scale;RuleTarget.RadiusCm=Radius;\n  TArray<FOpusSkillData> Added;')
rep('T.Run.Floor,35,"",Added,Error)', 'T.Run.Floor,float32(Radius),"",Added,Error)')
rep('Body.SetRelativeScale3D(FVector(.7));', 'Body.SetRelativeScale3D(FVector(.7*Definition.Scale));')
rep('Pupil.SetRelativeLocation(FVector(32,0,0));Pupil.SetRelativeScale3D(FVector(.13,.3,.4));', 'Pupil.SetRelativeLocation(FVector(32*Definition.Scale,0,0));Pupil.SetRelativeScale3D(FVector(.13,.3,.4)*Definition.Scale);')
rep('  FVector Target=Tower.Hero.CombatLocation();SetActorRotation((Target-ActorLocation).Rotation());', '''  FVector Target=Tower.Hero.CombatLocation();double Dt=Math::Clamp(DeltaSeconds,0.f,.05f);
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
  SetActorRotation((Target-ActorLocation).Rotation());''')
rep(' bool Ready=false;',' bool Ready=false;\n UPROPERTY() FOpusGameplayState State;\n UPROPERTY() UDWORecentUI UI;')
rep(' {Tower=T;AuraSlots.SetNum(4);Enabled.SetNum(4);Ready=true;SyncAuras();}', ''' {
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
 }''')
(r/'Tools/Integration/Staged/RecentGameplay.as').write_text(s,encoding='utf-8')
print('RecentGameplay staged')
