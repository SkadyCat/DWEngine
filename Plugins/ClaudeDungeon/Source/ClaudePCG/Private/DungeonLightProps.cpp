#include "DungeonLightProps.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADungeonLightProp::ADungeonLightProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f / 30.f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(BaseIntensity);
	Light->SetCastShadows(true);
	Light->SetSourceRadius(6.f);
	Light->SetSoftSourceRadius(20.f);
	Light->SetVolumetricScatteringIntensity(1.5f);
	Light->bUseInverseSquaredFalloff = true;
}

void ADungeonLightProp::BeginPlay()
{
	Super::BeginPlay();
	BaseLightLocation = Light->GetRelativeLocation();
	Phase = FMath::FRandRange(0.f, 100.f);
}

void ADungeonLightProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Phase += DeltaSeconds * FlickerSpeed;

	// Two layered noises: slow breathing plus fast crackle.
	const float Slow = FMath::PerlinNoise1D(Phase * 0.35f);
	const float Fast = FMath::PerlinNoise1D(Phase * 2.3f + 17.f);
	const float Flicker = 1.f + FlickerAmount * (Slow * 0.7f + Fast * 0.6f);
	// Only intensity flickers: moving the light would invalidate its cached virtual shadow map every frame.
	Light->SetIntensity(BaseIntensity * Flicker);
}

ADungeonTorch::ADungeonTorch()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TorchMesh(TEXT("/Game/ClaudeDungeonKit/Meshes/SM_Torch.SM_Torch"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> FireFx(TEXT("/Game/ClaudeDungeonKit/Particles/P_Fire.P_Fire"));

	if (TorchMesh.Succeeded())
	{
		Mesh->SetStaticMesh(TorchMesh.Object);
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Fire = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Fire"));
	Fire->SetupAttachment(Root);
	Fire->SetRelativeLocation(FVector(0.0, 30.0, 70.0));
	Fire->SetRelativeRotation(FRotator(0.0, 0.0, 0.0));
	if (FireFx.Succeeded())
	{
		Fire->SetTemplate(FireFx.Object);
	}

	BaseIntensity = 22.f;
	Light->SetRelativeLocation(FVector(0.0, 70.0, 95.0));
	Light->SetLightColor(FLinearColor(1.0f, 0.48f, 0.18f));
	Light->SetAttenuationRadius(1100.f);
	Light->SetIntensity(BaseIntensity);
}

ADungeonCandelabra::ADungeonCandelabra()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> StandMesh(TEXT("/Game/ClaudeDungeonKit/Meshes/SM_ChandelierStand.SM_ChandelierStand"));
	if (StandMesh.Succeeded())
	{
		Mesh->SetStaticMesh(StandMesh.Object);
	}

	BaseIntensity = 16.f;
	FlickerAmount = 0.1f;
	FlickerSpeed = 5.f;
	// Wide source radius softens the shadow the stand's arms throw on the floor.
	Light->SetRelativeLocation(FVector(0.0, 0.0, 205.0));
	Light->SetSourceRadius(32.f);
	Light->SetSoftSourceRadius(40.f);
	Light->SetLightColor(FLinearColor(1.0f, 0.62f, 0.32f));
	Light->SetAttenuationRadius(900.f);
	Light->SetIntensity(BaseIntensity);
}
