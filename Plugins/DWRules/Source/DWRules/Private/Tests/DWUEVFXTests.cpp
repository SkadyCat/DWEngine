#include "DWUEVFXMap.h"
#include "DWUEVFXRouter.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterfaceCurve.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWUEVFXMapContract, "DWRules.Presentation.UEVFXMapContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWUEVFXMapContract::RunTest(const FString& Parameters)
{
	UDWUEVFXMap* Map = LoadObject<UDWUEVFXMap>(nullptr, TEXT("/Game/WebVFX/DA_UEVFXMap.DA_UEVFXMap"));
	if (!TestNotNull(TEXT("editable UE override map exists"), Map)) return false;
	const FDWUEVFXEntry* Flight = Map->Effects.Find(TEXT("ice_lance.flight"));
	const FDWUEVFXEntry* Impact = Map->Effects.Find(TEXT("ice_lance.impact"));
	if (!Flight || !Impact) { AddError(TEXT("Missing ice mappings")); return false; }
	TestTrue(TEXT("flight follows existing carrier"), Flight->bFollowSource);
	TestFalse(TEXT("impact is a one-shot"), Impact->bFollowSource);
	TestTrue(TEXT("flight is the independently migrated Niagara asset"), Flight->System.ToSoftObjectPath().ToString().Contains(TEXT("NS_IceSpear_Projectile")));
	TestTrue(TEXT("impact is the matching Niagara asset"), Impact->System.ToSoftObjectPath().ToString().Contains(TEXT("NS_Ice_Hit_Small")));
	UDWUEVFXRouter* Router = NewObject<UDWUEVFXRouter>();
	Router->Initialize(Map);
	TestEqual(TEXT("mapped assets are resolved before Draw"), Router->MappingCount(), Map->Effects.Num());
	const FName Missing(TEXT("unmapped.effect"));
	const double Started = FPlatformTime::Seconds();
	for (uint64 I = 1; I <= 100000; ++I)
	{
		if (Router->Draw(Missing, I, FTransform::Identity))
		{
			AddError(TEXT("Unmapped effect must delegate to the original renderer"));
			return false;
		}
	}
	AddInfo(FString::Printf(TEXT("100000 cached negative map lookups: %.3f ms"), (FPlatformTime::Seconds() - Started) * 1000));
	TestEqual(TEXT("unmapped calls do not create Niagara instances"), Router->SpawnCount(), 0);
	TestFalse(TEXT("missing stable source cannot create per-frame bursts"), Router->Draw(TEXT("ice_lance.impact"), 0, FTransform::Identity));
	Router->Clear();
	TestEqual(TEXT("cleanup has no live systems"), Router->ActiveSystems(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWUEVFXWindContract, "DWRules.Presentation.UEVFXWindContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWUEVFXWindContract::RunTest(const FString& Parameters)
{
	UDWUEVFXMap* Map = LoadObject<UDWUEVFXMap>(nullptr, TEXT("/Game/WebVFX/DA_UEVFXMap.DA_UEVFXMap"));
	if (!TestNotNull(TEXT("override map exists"), Map)) return false;
	TestEqual(TEXT("existing global instance budget retained"), Map->MaxActiveSystems, 128);
	TSet<FLinearColor> Colors;
	for (int32 I = -1; I < 6; ++I)
	{
		const FName Key(I < 0 ? TEXT("whirlwind") : *FString::Printf(TEXT("whirlwind@%d"), I));
		const FDWUEVFXEntry* Entry = Map->Effects.Find(Key);
		if (!TestNotNull(*Key.ToString(), Entry)) continue;
		TestTrue(TEXT("wind follows its authoritative projectile"), Entry->bFollowSource);
		TestTrue(TEXT("derived top-down wind uses constant size"), Entry->System.ToSoftObjectPath().ToString().Contains(TEXT("NS_OpusWhirlwind")));
		const FLinearColor* Color = Entry->UserColors.Find(TEXT("User.Color01"));
		if (TestNotNull(TEXT("explicit tint before activation"), Color)) Colors.Add(*Color);
		TestFalse(TEXT("no per-projectile light"), Entry->UserBools.FindRef(TEXT("User.LightOn")));
		TestFalse(TEXT("no per-projectile audio spam"), Entry->UserBools.FindRef(TEXT("User.AudioOn")));
		TestTrue(TEXT("vendor optimization enabled"), Entry->UserBools.FindRef(TEXT("User.Optimized")));
	}
	TestTrue(TEXT("elemental wind tints stay distinguishable"), Colors.Num() >= 6);
	UNiagaraSystem* Wind=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/WebVFX/Systems/NS_OpusWhirlwind.NS_OpusWhirlwind"));
	if(TestNotNull(TEXT("saved constant-size wind"),Wind))
	{
		int32 Curves=0;ForEachObjectWithOuter(Wind,[&](UObject* Object){auto* C=Cast<UNiagaraDataInterfaceCurve>(Object);if(!C||C->GetName()!=TEXT("FloatFromCurve_FloatCurve")||!C->GetPathName().Contains(TEXT(":NE_Cycclone")))return;++Curves;for(float T:{0.f,.1f,.5f,1.f})TestTrue(TEXT("wind mesh scale stays full size throughout life"),FMath::IsNearlyEqual(C->Curve.Eval(T),1.f));},true);
		TestTrue(TEXT("all twelve wind emitters retain constant size"),Curves>=12);
		TestEqual(TEXT("no expensive spawn warmup"),Wind->GetWarmupTickCount(),0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDWUEVFXFallback, "DWRules.Presentation.UEVFXFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDWUEVFXFallback::RunTest(const FString& Parameters)
{
	UDWUEVFXRouter* Router = NewObject<UDWUEVFXRouter>();
	Router->Initialize(nullptr);
	Router->BeginFrame(0);
	TestFalse(TEXT("no map preserves the default"), Router->Draw(TEXT("ice_lance.flight"), 1, FTransform::Identity));
	UDWUEVFXMap* Empty = NewObject<UDWUEVFXMap>();
	Empty->Effects.Add(TEXT("empty.mapping"), FDWUEVFXEntry());
	Router->Initialize(Empty);
	TestEqual(TEXT("an empty asset reference is not an override"), Router->MappingCount(), 0);
	TestFalse(TEXT("empty reference preserves default"), Router->Draw(TEXT("empty.mapping"), 1, FTransform::Identity));
	Router->EndFrame();
	Router->BeginFrame(-1);
	TestEqual(TEXT("world clock reset leaves no native instances"), Router->ActiveSystems(), 0);
	return true;
}
#endif
