// Cold-load the actual authoring catalogue. No player save or replacement rule data.
class ADWOContentAuditMode : AGameModeBase
{
 default DefaultPawnClass=ADefaultPawn;
 UFUNCTION(BlueprintOverride) void BeginPlay()
 {
  if(!UOpusNative::Flag("DWOTransient")){UOpusNative::WriteLog("CONTENT_TEST_FAIL transient session required");UOpusNative::Quit(false);return;}
  auto Rules=UOpusNative::GetRules(this);FString Error;
  if(Rules==nullptr||!Rules.LoadRuleLibrary("Rules",Error)){UOpusNative::WriteLog("CONTENT_TEST_FAIL library "+Error);UOpusNative::Quit(false);return;}
  UOpusNative::SetEnabled(Rules,false);
  TArray<FName> Builds;Builds.Add(n"whirlwind");Builds.Add(n"echo");Builds.Add(n"royal_treasury");Builds.Add(n"vector_control");Builds.Add(n"summon_twisted_eyes");Builds.Add(n"monster_twisted_eye_bolt");
  bool Ok=true;TArray<FName> Extra;
  for(auto Id:Builds){Error="";int Plan=Rules.CompileNamedBuild(Id,Extra,Error);UOpusNative::WriteLog((Plan>=0?"CONTENT_TEST_PASS build ":"CONTENT_TEST_FAIL build ")+Id+" "+Error);Ok=Ok&&Plan>=0;}
  auto Pawn=Gameplay::GetPlayerPawn(0);if(Pawn==nullptr)Pawn=SpawnActor(ADefaultPawn,FVector(0,0,100));Rules.UpdateActorTarget(Pawn,1,0,400,400,45);
  int Owner=Rules.RegisterOwner(1,0);if(!Rules.EnablePlayer(Owner,Error)){UOpusNative::WriteLog("CONTENT_TEST_FAIL session "+Error);UOpusNative::Quit(false);return;}
  Error="";if(!Rules.StartPlayerProgression(Error)){UOpusNative::WriteLog("CONTENT_TEST_FAIL initial player "+Error);UOpusNative::Quit(false);return;}
  int Passed=0;auto Presets=UOpusData::Presets();
  for(auto Preset:Presets){Error="";bool Good=UOpusNative::ApplyPreset(Rules,FName(Preset.Id),Error);UOpusNative::WriteLog((Good?"CONTENT_TEST_PASS preset ":"CONTENT_TEST_FAIL preset ")+Preset.Id+" "+Error);if(Good)Passed++;Ok=Ok&&Good;}
  UOpusNative::WriteLog((Ok?"CONTENT_TEST_PASS Catalog ":"CONTENT_TEST_FAIL Catalog ")+Passed+"/"+Presets.Num());UOpusNative::Quit(Ok);
 }
}
