#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DWRuleHUDDps.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWRuleHUDDpsTest,"DWRules.Tower.HUDDps",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDWRuleHUDDpsTest::RunTest(const FString& Parameters)
{
 FDWRuleHUDDps Meter; double Value=0; int32 Targets=0;
 for(uint32 I=0;I<10000;++I) Meter.Record(0,I%3,1);
 Meter.Read(0,Value,Targets); TestEqual(TEXT("all hits survive floating-text limits"),Targets,3); TestTrue(TEXT("full-second total divided by unique targets"),FMath::IsNearlyEqual(Value,10000./3,1.e-8));
 Meter.Record(.5,0,12); Meter.Record(.5,4,8); Meter.Read(.5,Value,Targets);
 TestEqual(TEXT("same target across frames remains unique"),Targets,4); TestEqual(TEXT("new frame is included without startup spike"),Value,2505.);
 Meter.Read(.5,Value,Targets); TestEqual(TEXT("a paused clock keeps its window"),Value,2505.);
 Meter.Read(1,Value,Targets); TestEqual(TEXT("old frame expires at exactly one second"),Targets,2); TestEqual(TEXT("remaining frame uses one-second denominator"),Value,10.);
 Meter.Read(1.5,Value,Targets); TestEqual(TEXT("finished-tail clock drains targets"),Targets,0); TestEqual(TEXT("finished-tail clock drains damage"),Value,0.);
 Meter.Record(3,99,100); Meter.Reset(); Meter.Read(3,Value,Targets); TestEqual(TEXT("new floor clears the meter"),Targets,0);
 return true;
}
#endif
