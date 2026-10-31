#include "DWWebVFXActor.h"
#include "DWWebVFXComponent.h"
#include "DWWebVFXPrefab.h"
ADWWebVFXActor::ADWWebVFXActor(){PrimaryActorTick.bCanEverTick=true;Renderer=CreateDefaultSubobject<UDWWebVFXComponent>(TEXT("Effect"));SetRootComponent(Renderer);SetActorEnableCollision(false);}
void ADWWebVFXActor::Tick(float Dt)
{
	Super::Tick(Dt);if(!Prefab){Renderer->BeginFrame(Age);Renderer->EndFrame();return;}Age+=Dt*FMath::Max(0.f,PlayRate);
	const float Time=bLoop&&!Prefab->bContinuous?FMath::Fmod(Age,Prefab->Duration+.4f):Age;
	Renderer->BeginFrame(Age);Renderer->DrawPrefab(Prefab,Time,GetActorTransform());Renderer->EndFrame();
}
