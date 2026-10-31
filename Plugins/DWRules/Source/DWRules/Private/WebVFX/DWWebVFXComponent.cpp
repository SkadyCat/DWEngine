#include "DWWebVFXComponent.h"
#include "DWWebVFXPrefab.h"
#include "DWUEVFXMap.h"
#include "DWUEVFXRouter.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"

namespace
{
constexpr int32 Stride=20, CustomCount=10, InstanceBudget=65536;
struct FPose { float V[Stride]; };
struct FPart { FString Geometry, Material; bool bBillboard=false; };
struct FFramePart { int32 Part=0; TArray<FPose> Poses; };
struct FClip
{
	FName Id; FString File; float Duration=1,Fps=60; bool bContinuous=false,bInternal=false;
	TArray<FPart> Parts; TArray<TArray<FFramePart>> Frames;
	TStrongObjectPtr<UDWWebVFXPrefab> Asset;
	bool bLoadFailed=false;
};
struct FBatch
{
	TObjectPtr<UInstancedStaticMeshComponent> Component;
	TArray<FInstancedStaticMeshInstanceData> Instances; TArray<float> Custom; int32 Allocated=0;
};
bool ReadClip(FClip& Clip,const UDWWebVFXPrefab& Prefab)
{
	TArray<uint8> Bytes=Prefab.AnimationData;
	if(Bytes.Num()<20)return false;
	FMemoryReader Reader(Bytes,true);
	uint32 Magic=0,Version=0,Count=0;Reader<<Magic<<Version<<Count<<Clip.Fps<<Clip.Duration;
	if(Magic!=0x58465744||Version!=1||Count<2||Count>36000||!FMath::IsFinite(Clip.Fps)||!FMath::IsFinite(Clip.Duration)||Clip.Fps<=0||Clip.Duration<=0)return false;
	Clip.Frames.SetNum(Count);
	for(auto& Frame:Clip.Frames)
	{
		if(Reader.Tell()+4>Bytes.Num())return false;
		uint32 Parts=0;Reader<<Parts;if(Parts>uint32(Clip.Parts.Num()))return false;
		Frame.SetNum(Parts);
		for(auto& Part:Frame)
		{
			if(Reader.Tell()+8>Bytes.Num())return false;
			uint32 CountPoses=0;Reader<<Part.Part<<CountPoses;
			if(!Clip.Parts.IsValidIndex(Part.Part)||CountPoses>16384||Reader.Tell()+int64(CountPoses)*sizeof(FPose)>Bytes.Num())return false;
			Part.Poses.SetNumUninitialized(CountPoses);Reader.Serialize(Part.Poses.GetData(),CountPoses*sizeof(FPose));
			for(const auto& Pose:Part.Poses)for(float V:Pose.V)if(!FMath::IsFinite(V))return false;
		}
	}
	return !Reader.IsError()&&Reader.Tell()==Bytes.Num();
}
FString PrefabPath(FName Id,bool Internal)
{
	const FString Safe=Id.ToString().Replace(TEXT("-"),TEXT("_")).Replace(TEXT("@"),TEXT("_element_"));
	return Internal?TEXT("/Game/WebVFX/Variants/DA_")+Safe:TEXT("/Game/WebVFX/Prefabs/")+Safe+TEXT("/DA_")+Safe;
}
}

struct FDWWebVFXState
{
	TMap<FName,FClip> Clips;
	TMap<FString,TStrongObjectPtr<UStaticMesh>> Geometry;
	TMap<FString,TStrongObjectPtr<UMaterialInterface>> Materials;
	TMap<FString,FBatch> Batches;
	TArray<FName> Drawn;
	bool bAttempted=false,bLoaded=false;int32 Visible=0;float Clock=0;FQuat Camera=FQuat::Identity;
};

UDWWebVFXComponent::UDWWebVFXComponent(){PrimaryComponentTick.bCanEverTick=false;State=MakeShared<FDWWebVFXState>();}
UDWWebVFXComponent::~UDWWebVFXComponent()=default;

bool UDWWebVFXComponent::LoadPrefabs()
{
	if(State->bAttempted)return State->bLoaded;
	State->bAttempted=true;
	if (!UERouter)
	{
		const bool Enabled = !FParse::Param(FCommandLine::Get(), TEXT("DWOVFXDefault")) && FPackageName::DoesPackageExist(TEXT("/Game/WebVFX/DA_UEVFXMap"));
		SetUEVFXMap(Enabled ? LoadObject<UDWUEVFXMap>(nullptr, TEXT("/Game/WebVFX/DA_UEVFXMap.DA_UEVFXMap")) : nullptr);
	}
	const FString Directory=FPaths::ProjectContentDir()/TEXT("WebVFX/Data");
	FString Text;TSharedPtr<FJsonObject> Manifest;
	if(!FFileHelper::LoadFileToString(Text,*(Directory/TEXT("catalog.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest))return false;
	for(const auto& Value:Manifest->GetArrayField(TEXT("clips")))
	{
		const auto Data=Value->AsObject();FClip Clip;
		Clip.Id=FName(*Data->GetStringField(TEXT("id")));
		Clip.bContinuous=Data->GetBoolField(TEXT("continuous"));Clip.bInternal=Data->GetBoolField(TEXT("internal"));
		State->Clips.Add(Clip.Id,MoveTemp(Clip));
	}
	State->bLoaded=true;
	UE_LOG(LogTemp,Display,TEXT("WEB_VFX_READY clips=%d; native prefab assets load independently on demand"),State->Clips.Num());
	return true;
}

void UDWWebVFXComponent::BeginFrame(float Clock)
{
	LoadPrefabs();State->Clock=Clock;State->Visible=0;State->Drawn.Reset();
	if (UERouter) UERouter->BeginFrame(Clock);
	if(auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))State->Camera=Camera->GetCameraRotation().Quaternion();
	for(auto& Pair:State->Batches){Pair.Value.Instances.Reset();Pair.Value.Custom.Reset();}
}

bool UDWWebVFXComponent::Draw(FName Prefab,float Age,const FTransform& Placement,const FDWWebVFXParameters& Parameters)
{
	if (UERouter && UERouter->Draw(Parameters.MappingId.IsNone() ? Prefab : Parameters.MappingId, Parameters.InstanceId, Placement)) return true;
	if(!State->bLoaded)return false;
	auto* Clip=State->Clips.Find(Prefab);if(!Clip||Clip->bLoadFailed)return false;
	if(Clip->Frames.IsEmpty())
	{
		auto* Asset=Clip->Asset.Get();if(!Asset)Asset=LoadObject<UDWWebVFXPrefab>(nullptr,*PrefabPath(Clip->Id,Clip->bInternal));
		if(!Asset){Clip->bLoadFailed=true;UE_LOG(LogTemp,Error,TEXT("WebVFX missing native prefab %s"),*Prefab.ToString());return false;}
		Clip->Asset=TStrongObjectPtr<UDWWebVFXPrefab>(Asset);Clip->Parts.Reset();Clip->bContinuous=Asset->bContinuous;
		for(const auto& Resource:Asset->Parts)
		{
			if(!Resource.Mesh||!Resource.Material){Clip->bLoadFailed=true;return false;}
			auto& Part=Clip->Parts.AddDefaulted_GetRef();Part.Geometry=Resource.Mesh->GetPathName();Part.Material=Resource.Material->GetPathName();Part.bBillboard=Resource.bBillboard;
			if(!State->Geometry.Contains(Part.Geometry))State->Geometry.Add(Part.Geometry,TStrongObjectPtr<UStaticMesh>(Resource.Mesh.Get()));
			if(!State->Materials.Contains(Part.Material))State->Materials.Add(Part.Material,TStrongObjectPtr<UMaterialInterface>(Resource.Material.Get()));
		}
		if(!ReadClip(*Clip,*Asset)){Clip->bLoadFailed=true;UE_LOG(LogTemp,Error,TEXT("WebVFX invalid animation data %s"),*Prefab.ToString());return false;}
	}
	if(Age<0||(!Clip->bContinuous&&!Parameters.bPersistent&&Age>=Clip->Duration))return true;
	State->Drawn.AddUnique(Prefab);
	const float Time=Clip->bContinuous?FMath::Fmod(Age,Clip->Duration):FMath::Min(Age,Clip->Duration-1.f/Clip->Fps);
	const float Frame=Time*Clip->Fps;const int32 Index=FMath::Clamp(FMath::FloorToInt(Frame),0,Clip->Frames.Num()-1);
	const int32 Next=FMath::Min(Index+1,Clip->Frames.Num()-1);const float Alpha=FMath::Frac(Frame);
	const auto& A=Clip->Frames[Index];const auto& B=Clip->Frames[Next];
	for(int32 P=0;P<A.Num();++P)
	{
		const auto& Current=A[P];const auto& Part=Clip->Parts[Current.Part];
		const FString Key=Part.Geometry+Part.Material;auto* Batch=State->Batches.Find(Key);
		if(!Batch)
		{
			auto* Component=NewObject<UInstancedStaticMeshComponent>(GetOwner(),NAME_None,RF_Transient);
			Component->SetMobility(EComponentMobility::Movable);Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->SetCastShadow(false);Component->SetCanEverAffectNavigation(false);
			Component->SetStaticMesh(State->Geometry[Part.Geometry].Get());Component->SetMaterial(0,State->Materials[Part.Material].Get());
			Component->NumCustomDataFloats=CustomCount;Component->SetupAttachment(this);Component->RegisterComponent();MeshBatches.Add(Component);
			Batch=&State->Batches.Add(Key);Batch->Component=Component;
		}
		const FMatrix ComponentInverse=Batch->Component->GetComponentTransform().ToInverseMatrixWithScale();
		for(int32 I=0;I<Current.Poses.Num()&&State->Visible<InstanceBudget;++I)
		{
			const auto& V=Current.Poses[I].V;
			const float* W=B.IsValidIndex(P)&&B[P].Part==Current.Part&&B[P].Poses.IsValidIndex(I)?B[P].Poses[I].V:V;
			FVector Position,Scale;for(int32 K=0;K<3;++K){Position[K]=FMath::Lerp(V[K],W[K],Alpha);Scale[K]=FMath::Lerp(V[K+7],W[K+7],Alpha);}
			const FQuat Q=FQuat::Slerp(FQuat(V[3],V[4],V[5],V[6]),FQuat(W[3],W[4],W[5],W[6]),Alpha).GetNormalized();
			// A matrix preserves a rotated child's axes under a nonuniform region
			// scale. FTransform multiplication would turn circular rings elliptical.
			FMatrix Pose=FTransform(Q,Position,Scale).ToMatrixWithScale()*Placement.ToMatrixWithScale();
			if(Part.bBillboard)Pose=FTransform(State->Camera*FQuat(FVector::UpVector,-UE_DOUBLE_PI/2)*Q,Placement.TransformPosition(Position),Scale*(Parameters.BillboardScale>=0?FVector(Parameters.BillboardScale):Placement.GetScale3D())).ToMatrixWithScale();
			Batch->Instances.Emplace(Pose*ComponentInverse);
			for(int32 K=10;K<Stride;++K)Batch->Custom.Add(FMath::Lerp(V[K],W[K],Alpha));
			const int32 Base=Batch->Custom.Num()-CustomCount;
			if(Parameters.Inner>=0)Batch->Custom[Base+8]=Parameters.Inner;
			if(Parameters.Angle>=0)Batch->Custom[Base+9]=Parameters.Angle;
			if(Clip->bContinuous)Batch->Custom[Base+7]=Age;
			if(Parameters.Clock>=0)Batch->Custom[Base+7]=Parameters.Clock;
			if(Parameters.bPersistent)Batch->Custom[Base+5]=.5f+.15f*FMath::Sin(State->Clock*3);
			if(Parameters.Fade>=0)Batch->Custom[Base+5]*=Parameters.Fade;
			++State->Visible;
		}
	}
	return true;
}

bool UDWWebVFXComponent::DrawPrefab(UDWWebVFXPrefab* Prefab,float Age,const FTransform& Placement)
{
	if(!Prefab)return false;
	if(!State->Clips.Contains(Prefab->Id)){FClip Clip;Clip.Id=Prefab->Id;Clip.Asset=TStrongObjectPtr<UDWWebVFXPrefab>(Prefab);State->Clips.Add(Clip.Id,MoveTemp(Clip));}
	auto& Clip=State->Clips[Prefab->Id];
	if(Clip.Asset.Get()!=Prefab){Clip.Asset=TStrongObjectPtr<UDWWebVFXPrefab>(Prefab);Clip.Frames.Reset();Clip.Parts.Reset();Clip.bLoadFailed=false;}
	State->bLoaded=true;
	return Draw(Prefab->Id,Age,Placement);
}

void UDWWebVFXComponent::EndFrame()
{
	if (UERouter) UERouter->EndFrame();
	for(auto& Pair:State->Batches)
	{
		auto& Batch=Pair.Value;const int32 Needed=Batch.Instances.Num();
		if(Needed>Batch.Allocated)
		{
			TArray<FTransform> Extra;Extra.Init(FTransform::Identity,Needed-Batch.Allocated);Batch.Component->AddInstances(Extra,false,false);Batch.Allocated=Needed;
		}
		Batch.Instances.SetNum(Batch.Allocated);Batch.Custom.SetNumZeroed(Batch.Allocated*CustomCount);
		for(int32 I=Needed;I<Batch.Allocated;++I)Batch.Instances[I].Transform=FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector).ToMatrixWithScale();
		if(Batch.Allocated)
		{
			Batch.Component->BatchUpdateInstancesData(0,Batch.Allocated,Batch.Instances.GetData(),false,true);
			Batch.Component->SetCustomData(0,Batch.Allocated-1,MakeArrayView(Batch.Custom),false);
			Batch.Component->MarkRenderStateDirty();
		}
	}
}
bool UDWWebVFXComponent::HasPrefab(FName Id)const{return State->Clips.Contains(Id);}
TArray<FName> UDWWebVFXComponent::Catalog()const{TArray<FName> Out;for(const auto& P:State->Clips)if(!P.Value.bInternal)Out.Add(P.Key);Out.Sort(FNameLexicalLess());return Out;}
int32 UDWWebVFXComponent::VisibleInstances()const{return State->Visible;}
TArray<FName> UDWWebVFXComponent::DrawnPrefabs()const{return State->Drawn;}

void UDWWebVFXComponent::SetUEVFXMap(UDWUEVFXMap* Map)
{
	if (!UERouter) UERouter = NewObject<UDWUEVFXRouter>(this);
	UERouter->Initialize(Map);
}
void UDWWebVFXComponent::ClearUEVFX() { if (UERouter) UERouter->Clear(); }
void UDWWebVFXComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearUEVFX();
	Super::EndPlay(Reason);
}
