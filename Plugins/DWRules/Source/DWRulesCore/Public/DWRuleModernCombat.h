#pragma once
#include "DWRuleCombat.h"

namespace DWRules
{
struct FDWModernGround
{
    uint32 Id=0;
    FName Type;
    FVector3d Position=FVector3d::ZeroVector;
    FVector3d Normal=FVector3d::UpVector;
    double Radius=2,Left=4,Tick=0,Strength=0,Born=0;
    int32 Element=-1;
};
struct FDWModernShot
{
    FVector3d Previous=FVector3d::ZeroVector,Origin=FVector3d::ZeroVector;
    FVector3d GroundPoint=FVector3d::ZeroVector,GroundNormal=FVector3d::UpVector;
    double TurnLeft=.6,Interval=.6,Range=12,Speed0=0,RoamingSpeed=0;
    double Heading=0;
    FVector3f LastVelocity=FVector3f::ZeroVector;
    bool bHeading=false;
    uint32 Seed=1;
    uint32 VisualSeed=0;
    bool bRoaming=false,bContact=false,bTint=false;
    bool bGrounded=false;
    int32 GroundTint=-1;
    TSet<uint64> Inside;
    TMap<uint32,double> Rehit;
    TArray<FVector3d> Curve;
    double Travel=0,CurveLength=0;
};
struct FDWCaptureSpec
{
    uint64 Identity=0;
    FVector3d Target=FVector3d::ZeroVector;
    double Bend=2;
};
class DWRULESCORE_API FDWModernCombat
{
public:
    explicit FDWModernCombat(FDWCombatRuntime& Runtime):R(Runtime){}
    FDWModernCombat(const FDWModernCombat&)=delete;
    FDWModernCombat& operator=(const FDWModernCombat&)=delete;
    FDWCombatRuntime& R;
    TArray<FDWModernGround> Grounds;
    TMap<uint32,FDWModernShot> Shots;
    TMap<int32,FDWCaptureSpec> PendingCaptures;
    int32 Captured=0,Mirrors=0;
    void Spawn(int32 Index,int32 Root);
    bool Motion(int32 Index,double Dt);
    bool Wall(int32 Index);
    bool Contact(int32 Index,int32 Target);
    bool Intercept(int32 Index,int32 Target,const FVector3f& At);
    void Hit(const FDWCombatHit& Hit);
    void Tick(double Dt);
    void AfterMotion();
    const FDWSheet& Sheet(int32 Plan);
private:
    uint32 NextGroundId=1;
    TMap<FString,double> GroundReady;
    TMap<int32,TUniquePtr<FDWSheet>> Sheets;
    TMap<FString,int32> CapturePlans;
    void Gate(int32 Index,uint32 VisualSeed);
};
}
