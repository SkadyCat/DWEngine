#pragma once
#include "CoreMinimal.h"

// Presentation-only mirror of public/sim/damage-meter.mjs. One native frame,
// rather than one hit, occupies a queue entry; floating-text budgets never apply.
class FDWRuleHUDDps
{
 struct FFrame { double Time=0,Amount=0; TSet<uint32> Targets; };
 TArray<FFrame> Frames;
public:
 void Reset() { Frames.Reset(); }
 void Record(double Time,uint32 Target,double Amount)
 {
  if(!(Amount>0)||!FMath::IsFinite(Amount)) return;
  Frames.RemoveAll([&](const FFrame& Frame){return Frame.Time<=Time-1+1.e-9;});
  if(Frames.IsEmpty()||Frames.Last().Time!=Time) { FFrame Frame; Frame.Time=Time; Frames.Add(MoveTemp(Frame)); }
  Frames.Last().Amount+=Amount; Frames.Last().Targets.Add(Target);
 }
 void Read(double Time,double& Value,int32& Targets) const
 {
  TSet<uint32> Unique; double Total=0;
  for(const auto& Frame:Frames) if(Frame.Time>Time-1+1.e-9) { Total+=Frame.Amount; Unique.Append(Frame.Targets); }
  Targets=Unique.Num(); Value=Targets?FMath::Max(0.,Total)/Targets:0;
 }
};
