#include "Components/InstancedStaticMeshComponent.h"
#include "DWRuleAssets.h"
#include "DWRuleVisualHost.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

void ADWRuleVisualHost::SyncEffects(TArray<DWRules::FDWCombatEffect> Effects, float Dt,
									const UDWRuleWorldConfig *Config)
{
	if(bUsingWebEffects)
	{
		for(auto& E:LiveEffects)E.Age+=WebDelta;
		LiveEffects.RemoveAll([](const auto& E){return E.Age>(E.Data.Kind==TEXT("__treasury_gate")?.32f:1.1f);});
		for(const auto& E:Effects){if(LiveEffects.Num()>=2048)LiveEffects.RemoveAt(0);auto& Added=LiveEffects.AddDefaulted_GetRef();Added.Data=E;Added.VisualId=NextEffectVisualId++;}
		return;
	}
	for (auto &E : LiveEffects)
		E.Age += Dt;
	LiveEffects.RemoveAll([](const auto &E) { return E.Age > .45f; });
	for (const auto &E : Effects)
		if (LiveEffects.Num() < 512)
			LiveEffects.Add({E});
	// Effect batches have their own names, so carrier transforms are uploaded independently.
	for (auto &Pair : Batches)
		if (Pair.Key.ToString().StartsWith(TEXT("__effect_")))
			Pair.Value.Transforms.Reset();
	static const FLinearColor Colors[] = {{1, .18f, .025f}, {.15f, .7f, 1}, {1, .85f, .05f},
										  {.7f, .7f, .75f}, {.25f, 1, .1f}, {.65f, .2f, 1}};
	for (const auto &E : LiveEffects)
	{
		const bool Lightning = E.Data.Element == 2;
		const FName Kind(
			*FString::Printf(TEXT("__effect_%s_%d"), Lightning ? TEXT("bolt") : TEXT("pulse"), E.Data.Element));
		if (!Batches.Contains(Kind))
		{
			auto *Component = EnsureComponent(Kind, Config);
			Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
			if (auto *Base = LoadObject<UMaterialInterface>(
					nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				auto *Material = UMaterialInstanceDynamic::Create(Base, this);
				Material->SetVectorParameterValue(TEXT("Color"), Colors[FMath::Min<int32>(E.Data.Element, 5)] * 2);
				Component->SetMaterial(0, Material);
			}
		}
		auto &Batch = Batches[Kind];
		auto Beam = [&](FVector A, FVector B, double Width) {
			const FVector Delta = B - A;
			if (Delta.SizeSquared() > 1.e-6)
				Batch.Transforms.Emplace(FQuat::FindBetweenNormals(FVector::UpVector, Delta.GetSafeNormal()),
										 (A + B) * .5, FVector(Width, Width, Delta.Size() / 100));
		};
		const double Size = FMath::Max(.15, E.Data.Radius) * 2 * (.5 + E.Age / .45);
		FVector At = FVector(E.Data.Position) * 100;
		if (E.Data.Shape == TEXT("beam"))
		{
			const FVector End = FVector(E.Data.End) * 100;
			const FVector Mid = (At + End) * .5 + FVector(0, 0, 15 * FMath::Sin(E.Age * 90));
			Beam(At, Mid, .04);
			Beam(Mid, End, .04);
			continue;
		}
		if (E.Data.Shape == TEXT("line"))
		{
			const FVector Dir(FMath::Cos(E.Data.Direction), FMath::Sin(E.Data.Direction), 0);
			Beam(At, At + Dir * E.Data.Length * 100, E.Data.Width);
			continue;
		}
		if (E.Data.Shape == TEXT("ring") || E.Data.Shape == TEXT("sector"))
		{
			const bool Sector = E.Data.Shape == TEXT("sector");
			const double Span = Sector ? E.Data.Angle : 2 * UE_DOUBLE_PI;
			const double Start = Sector ? E.Data.Direction - Span * .5 : 0;
			auto Point = [&](double A, double R) { return At + FVector(FMath::Cos(A), FMath::Sin(A), 0) * R * 100; };
			for (int32 I = 0; I < 32; ++I)
			{
				const double A = Start + Span * I / 32, B = Start + Span * (I + 1) / 32;
				Beam(Point(A, E.Data.Radius), Point(B, E.Data.Radius), .035);
				if (!Sector && E.Data.Inner > 0)
					Beam(Point(A, E.Data.Inner), Point(B, E.Data.Inner), .035);
			}
			if (Sector)
			{
				Beam(At, Point(Start, E.Data.Radius), .035);
				Beam(At, Point(Start + Span, E.Data.Radius), .035);
			}
			continue;
		}
		Batch.Transforms.Emplace(FQuat::Identity, At, FVector(Size, Size, .025));
		if (Lightning)
		{
			At.Z += 500;
			Batch.Transforms.Emplace(FQuat::Identity, At, FVector(.08, .08, 10));
		}
	}
	for (auto &Pair : Batches)
	{
		if (!Pair.Key.ToString().StartsWith(TEXT("__effect_")))
			continue;
		auto &B = Pair.Value;
		const int32 Needed = B.Transforms.Num();
		if (Needed > B.Allocated)
		{
			TArray<FTransform> Extra;
			Extra.Append(B.Transforms.GetData() + B.Allocated, Needed - B.Allocated);
			B.Component->AddInstances(Extra, false, true);
			B.Allocated = Needed;
		}
		while (B.Transforms.Num() < B.Allocated)
			B.Transforms.Emplace(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
		if (B.Allocated)
			B.Component->BatchUpdateInstancesTransforms(0, B.Transforms, true, true, true);
	}
}
