class AThreeCHUD : AHUD
{
 UFUNCTION(BlueprintOverride) void DrawHUD(int SizeX,int SizeY)
 {
  auto G=Cast<AThreeCGameMode>(Gameplay::GetGameMode());if(G==nullptr)return;
  DrawRect(FLinearColor(0.015,0.025,0.04,0.83),12,12,710,116);
  DrawText(G.PoseModel.IsEmpty()?"OPUS 3C LAB  |  100 HUMANOIDS / 3 FACTIONS":"POSE PROBE | "+G.PoseModel+" "+G.PoseState+" @ "+G.PoseTime,FLinearColor::White,24,20,nullptr,1.15,false);
  TArray<int> Alive;Alive.SetNum(3);int Attacks=0,Hits=0,Deaths=0;for(auto F:G.Fighters){if(!F.Dead)Alive[F.Team]++;Attacks+=F.AttackCount;Hits+=F.HitCount;Deaths+=F.DeathCount;}
  for(int I=0;I<3;I++)DrawText("FACTION "+(I+1)+": "+Alive[I]+(I==0?" / 34":" / 33"),G.TeamColor(I),24+I*170,46,nullptr,1,false);
  DrawText((G.Controlled?"STABLE CONTROL + POSE BLEND":"ORIGINAL REPRODUCTION")+" | attacks "+Attacks+" hits "+Hits+" deaths "+Deaths+(G.Paused?" | PAUSED":""),FLinearColor(0.95,0.83,0.50),24,69,nullptr,0.95,false);
  if(G.Fighters.IsValidIndex(G.Focus)){auto F=G.Fighters[G.Focus];DrawText("FOCUS #"+F.Index+" "+F.ModelId+" | "+F.State.ToString()+" | "+int(F.ActualSpeed)+" cm/s",FLinearColor::White,24,91,nullptr,0.9,false);}
  DrawText("V camera  | Tab next unit  | click select  | C reference mode  | P pause  | R reset  | free: WASD/QE + RMB",FLinearColor::White,20,SizeY-30,nullptr,0.85,false);
 }
}
