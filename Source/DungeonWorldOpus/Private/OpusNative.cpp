#include "OpusNative.h"
#include "OpusDungeon.h"
#include "DWRulePawnEquipment.h"
#include "DWRulePresetReader.h"
#include "DWRuleProgramReader.h"
#include "DWRuleTargetComponent.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnLua.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
DEFINE_LOG_CATEGORY_STATIC(LogOpus,Log,All);
UDWRuleWorldSubsystem* UOpusNative::GetRules(UObject* C){return UDWRuleWorldSubsystem::ForContext(C);}
TArray<FOpusMonsterData> UOpusNative::Monsters(UDWRuleWorldSubsystem* R)
{
 TArray<FOpusMonsterData> Out;if(!R)return Out;
 TArray<FName> Keys;R->Library().Monsters.GetKeys(Keys);Keys.Sort(FNameLexicalLess());
 for(FName K:Keys){const auto& D=R->Library().Monsters[K];FOpusMonsterData M;M.Id=K;M.Enabled=D.bEnabled;M.Weight=D.Weight;M.Health=D.Health;M.Speed=D.SpeedCm;M.StopDistance=D.StopDistanceCm;M.Scale=D.VisualScale;M.FollowDistance=D.FollowDistanceCm;M.MinFloor=D.MinFloor;M.Rarity=D.Equipment.Ids.FindRef(TEXT("rarity"));Out.Add(M);}return Out;
}
bool UOpusNative::MapData(UDWRuleWorldSubsystem* R,FOpusMapData& Out,FString& Error)
{return MapCoreData(R,TEXT("map_core_equipment"),Out,Error);}
bool UOpusNative::MapCoreData(UDWRuleWorldSubsystem* R,FName Core,FOpusMapData& Out,FString& Error)
{
 Out=FOpusMapData();Error.Reset();if(!R){Error=TEXT("missing rule world");return false;}const auto* Map=R->Library().Equipment.Find(Core);if(!Map||Map->Slot!=TEXT("map")){Error=TEXT("unknown map core: ")+Core.ToString();return false;}
 Out.Tier=Map->Tier;DWRules::FDWCollectionPart P;P.Collection=*Map;P.bEquipment=true;P.Label=TEXT("map");
 DWRules::FDWBuildSource S;TArray<DWRules::FDWContribution> Rows;
 if(!R->Library().Aggregate(NAME_None,{P},TNumericLimits<double>::Max(),S,Rows,Error))return false;
 TArray<DWRules::FDWHeldTrait> Held;for(const auto& Row:Rows)if(!Row.bMerged&&!Row.bExpired)Held.Add(Row.Held);
 DWRules::FDWRuleCharacter C;if(!C.SetSheet(Held,Error)&&!Error.IsEmpty())return false;
 Out.LifeBase=C.S(TEXT("minion.life_base"));Out.LifeIncreased=C.S(TEXT("minion.life_increased"));Out.LifeMore=C.S(TEXT("minion.life_more"));Out.DamageIncreased=C.S(TEXT("minion.damage"));Out.DamageMore=C.S(TEXT("minion.more"));Out.Packs=C.S(TEXT("map.monster_packs"));return true;
}
void UOpusNative::SetMapRewardTier(UDWRuleWorldSubsystem* R,int32 Tier)
{
 // The subsystem owns this mutable floor state; its accessor exposes a const read view.
 if(R)if(auto* Rewards=const_cast<DWRules::FDWRuleFloorRewards*>(R->PlayerFloorRewards()))Rewards->MaxTier=FMath::Clamp(Tier,1,10);
}
FString UOpusNative::ReadMapCore()
{
 FString Value;FFileHelper::LoadFileToString(Value,*(FPaths::ProjectSavedDir()/(Flag(TEXT("DWOMapCoreProfileTest"))?TEXT("Verification/MapCore/selection.txt"):TEXT("RuleTower/MapCore.txt"))));return Value.TrimStartAndEnd();
}
bool UOpusNative::WriteMapCore(const FString& Core)
{
 const FString Path=FPaths::ProjectSavedDir()/(Flag(TEXT("DWOMapCoreProfileTest"))?TEXT("Verification/MapCore/selection.txt"):TEXT("RuleTower/MapCore.txt"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);return FFileHelper::SaveStringToFile(Core,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
bool UOpusNative::RegisterEnemy(UDWRuleWorldSubsystem* R,AActor* A,int32 Id,FOpusMonsterData Data,FName Rarity,int32 Floor,float Radius,const FString& Inherited,const TArray<FOpusSkillData>& Added,FString& Error)
{
 if(!R||!A){Error=TEXT("missing rule world/actor");return false;}const auto* Base=R->Library().Monsters.Find(Data.Id);if(!Base){Error=TEXT("unknown monster");return false;}
 auto D=*Base;D.Health=Data.Health;D.SpeedCm=Data.Speed;D.Equipment.Ids.Add(TEXT("rarity"),Rarity);
 if(!DWRules::FDWPawnEquipmentRules::SetTower(R->Library(),D.Equipment,Floor,Error))return false;
 if(!Inherited.IsEmpty()){DWRules::FDWTraitSource Source;FString Name;if(!FDWRuleProgramReader::ReadTraitText(Inherited,Source,Name,Error))return false;D.SummonerSources.Add(MoveTemp(Source));}
 for(int32 I=Added.Num()-1;I>=0;--I){DWRules::FDWMonsterSkill Skill;Skill.Id=Added[I].Id;Skill.RangeCm=Added[I].Range;Skill.Cooldown=Added[I].Cooldown;Skill.Power=Added[I].Power;D.Skills.Insert(Skill,0);}
 R->UpdateActorTarget(A,Id,1,float(Data.Health),float(Data.Health),Radius);
 if(R->RegisterMonsterDefinition(Id,D,A->GetActorLocation(),Error,Radius)<0)return false;
 const auto* M=R->CombatEnemy(R->ResolveTargetId(Id));if(!M)return false;
 if(auto* T=A->FindComponentByClass<UDWRuleTargetComponent>()){T->Health=M->Health;T->MaxHealth=M->MaxHealth;T->Shield=M->Shield;T->MaxShield=M->MaxShield;}return true;
}
bool UOpusNative::HitPower(UDWRuleWorldSubsystem* R,int32 Id,float P,FString& E){return R&&R->SetMonsterHitPower(Id,P,E);}
bool UOpusNative::IsElite(UDWRuleWorldSubsystem* R,int32 Id){const auto* M=R?R->CombatEnemy(R->ResolveTargetId(Id)):nullptr;return M&&M->bElite;}
int32 UOpusNative::ActivePlan(UDWRuleWorldSubsystem* R){return R&&R->PlayerSession()?R->PlayerSession()->Combat.Hero.Plan:INDEX_NONE;}
int32 UOpusNative::Selected(UDWRuleWorldSubsystem* R){return R&&R->PlayerSession()?R->PlayerSession()->Selected:0;}
void UOpusNative::CarryPools(UDWRuleWorldSubsystem* R,FDWRulePlayerPools P){if(R&&R->PlayerSession()){auto& C=R->PlayerSession()->Character;C.Life=FMath::Min(C.MaxLife,double(P.Life));C.Shield=FMath::Min(C.ShieldCap,double(P.Shield));C.Mana=FMath::Min(C.ManaCapacity(),double(P.Mana));}}
void UOpusNative::SetEnabled(UDWRuleWorldSubsystem* R,bool E){if(R)R->SetSimulationEnabled(E);}
void UOpusNative::BeginRewards(UDWRuleWorldSubsystem* R,int32 Seed,int32 Floor,int32 Visit,int32 Tier){if(R)R->BeginPlayerFloorRewards(Seed,Floor,Visit,Tier);}
void UOpusNative::CompleteRewards(UDWRuleWorldSubsystem* R){if(R)R->CompletePlayerFloorRewards();}
TArray<FOpusLootData> UOpusNative::Loot(UDWRuleWorldSubsystem* R){TArray<FOpusLootData> Out;const auto* F=R?R->PlayerFloorRewards():nullptr;if(F)for(const auto& L:F->Ground){FOpusLootData D;D.Key=L.Key;D.Position=FVector(L.Position.X,L.Position.Y,60);D.Claimed=L.bClaimed;Out.Add(D);}return Out;}
bool UOpusNative::Pickup(UDWRuleWorldSubsystem* R,const FString& K,FVector P,FString& E){return R&&R->PickupPlayerLoot(K,P,E);}
double UOpusNative::PickupRadius(){return DWRules::FDWRuleFloorRewards::PickupRadiusCm;}
bool UOpusNative::BuyMerchant(UDWRuleWorldSubsystem* R,const FString& K,FString& E){return R&&R->BuyPlayerMerchant(K,E);}
bool UOpusNative::ApplyPreset(UDWRuleWorldSubsystem* R,FName Id,FString& E){TArray<FDWRulePreset> Rows;if(!R||!FDWRulePresetReader::Catalogue(Rows,E))return false;for(const auto& Row:Rows)if(Row.Id==Id)return R->ApplyPlayerPreset(Row.Json,E);E=TEXT("unknown preset");return false;}
FName UOpusNative::Belt(UDWRuleWorldSubsystem* R,int32 S){return R&&R->PlayerInventory().Belt.IsValidIndex(S)?R->PlayerInventory().Belt[S]:NAME_None;}
bool UOpusNative::Flag(const FString& N){return FParse::Param(FCommandLine::Get(),*N);}
int32 UOpusNative::IntOption(const FString& N,int32 F){FParse::Value(FCommandLine::Get(),*(N+TEXT("=")),F);return F;}
FString UOpusNative::StringOption(const FString& N){FString V;FParse::Value(FCommandLine::Get(),*(N+TEXT("=")),V);return V;}
FString UOpusNative::ReadProfile(){FString J;FFileHelper::LoadFileToString(J,*(FPaths::ProjectSavedDir()/(Flag(TEXT("DWOMapItemProfileTest"))?TEXT("Verification/MapItems/Profile.json"):Flag(TEXT("DWOProfileTest"))?TEXT("Verification/ProfileRoundtrip.json"):TEXT("RuleTower/Profile.v2.json"))));return J;}
bool UOpusNative::WriteProfile(const FString& J){const FString P=FPaths::ProjectSavedDir()/(Flag(TEXT("DWOMapItemProfileTest"))?TEXT("Verification/MapItems/Profile.json"):Flag(TEXT("DWOProfileTest"))?TEXT("Verification/ProfileRoundtrip.json"):TEXT("RuleTower/Profile.v2.json"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(P),true);const FString Temp=P+TEXT(".tmp");if(J.IsEmpty()||!FFileHelper::SaveStringToFile(J,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return false;return IFileManager::Get().Move(*P,*Temp,true,true);}
void UOpusNative::WriteLog(const FString& M){UE_LOG(LogOpus,Display,TEXT("%s"),*M);}
void UOpusNative::Quit(bool Success){FPlatformMisc::RequestExitWithStatus(false,Success?0:1);}
bool UOpusNative::LuaSmoke(){return UnLua::Startup()&&UnLua::RunChunk(UnLua::GetState(),"local m=require('opus.bridge'); assert(m.contract_version==1 and type(m.challenge)=='function' and type(m.cast)=='function')");}

void UOpusNative::TargetPoint(UDWRuleWorldSubsystem* R,int32 Owner,FVector P){if(R)R->SetOwnerTargetPoint(Owner,P);}
void UOpusNative::StepIfNeeded(UDWRuleWorldSubsystem* R,float Dt)
{
 if(!R)return;static TMap<TWeakObjectPtr<UDWRuleWorldSubsystem>,double> Clocks;
 const double Current=R->RuleWorld().GetClock();double& Previous=Clocks.FindOrAdd(R,-1.0);
 if(Current==Previous||(Previous<0&&Current==0)){R->BeginTargets();R->StepFrame(Dt,nullptr);}
 Previous=R->RuleWorld().GetClock();
 if(Flag(TEXT("DWODebug"))){static TMap<TWeakObjectPtr<UDWRuleWorldSubsystem>,double> Next;auto& Deadline=Next.FindOrAdd(R);if(Current>=Deadline){Deadline=Current+2;auto* S=R->PlayerSession();if(S)UE_LOG(LogOpus,Display,TEXT("DEBUG_NATIVE player=%u status=%u life=%.12g targets=%d"),S->TargetId,R->RuleWorld().GetOwnerStatusId(S->Combat.Hero.Owner),S->Character.Life,R->RuleWorld().Targets().Num());for(TActorIterator<AActor> It(R->GetWorld());It;++It)if(auto* T=It->FindComponentByClass<UDWRuleTargetComponent>();T&&T->Team==0)UE_LOG(LogOpus,Display,TEXT("DEBUG_TARGET actor=%s internal=%u external=%d hp=%.9g"),*It->GetName(),uint32(T->TargetId),T->ExternalId,T->Health);}}

}
int64 UOpusNative::Stones(UDWRuleWorldSubsystem* R){return R?R->PlayerInventory().Stones:0;}
int32 UOpusNative::SummonCount(UDWRuleWorldSubsystem* R){return R?R->RuleWorld().GetSummons().Num():0;}

void UOpusNative::Velocity(AActor* A,FVector Value){if(A&&A->GetRootComponent())A->GetRootComponent()->ComponentVelocity=Value;}
void UOpusNative::Tint(UPrimitiveComponent* C,FLinearColor V){if(!C)return;auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));if(!Base)return;auto* M=UMaterialInstanceDynamic::Create(Base,C);M->SetVectorParameterValue(TEXT("Color"),V);C->SetMaterial(0,M);}
void UOpusNative::SetGeometry(UDWRuleWorldSubsystem* R,FOpusSweep Sweep)
{
 if(!R)return;
 R->RuleWorld().SweepGeometry=[Sweep](const FVector3f& A,const FVector3f& B,float Radius,float& Time){if(!Sweep.IsBound())return false;Time=Sweep.Execute(FVector(A)*100,FVector(B)*100,Radius*100);return Time>=0&&Time<=1;};
 const TWeakObjectPtr<UDWRuleWorldSubsystem> Weak(R);
 R->RuleWorld().ProjectGround=[Weak](const FVector3f& At,FVector3f& Point,FVector3f& Normal){FVector P,N;if(!Weak.IsValid()||!UOpusDungeon::Surface(Weak.Get(),FVector(At)*100,P,N))return false;Point=FVector3f(P*.01);Normal=FVector3f(N);return true;};
}
