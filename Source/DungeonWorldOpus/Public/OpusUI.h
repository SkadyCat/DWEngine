#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SvgCanvasScreen.h"
#include "SvgCanvasMotionNode.h"
#include "OpusUI.generated.h"

USTRUCT(BlueprintType)
struct FOpusUIAction
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Type;
 UPROPERTY(BlueprintReadOnly) FString Op;
 UPROPERTY(BlueprintReadOnly) FString Value;
 UPROPERTY(BlueprintReadOnly) int32 Index=0;
};
USTRUCT(BlueprintType)
struct FOpusUIRow
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadWrite) FString Label;
 UPROPERTY(BlueprintReadWrite) FString Detail;
 UPROPERTY(BlueprintReadWrite) FString Op;
 UPROPERTY(BlueprintReadWrite) FString Value;
 UPROPERTY(BlueprintReadWrite) FString Icon;
 UPROPERTY(BlueprintReadWrite) FString Kind;
 UPROPERTY(BlueprintReadWrite) FString Rarity;
 UPROPERTY(BlueprintReadWrite) FString Meta;
 UPROPERTY(BlueprintReadWrite) FString Slot;
 UPROPERTY(BlueprintReadWrite) FString Badge;
 UPROPERTY(BlueprintReadWrite) int32 Count=0;
 UPROPERTY(BlueprintReadWrite) int32 Tier=0;
 UPROPERTY(BlueprintReadWrite) bool Equipped=false;
 UPROPERTY(BlueprintReadWrite) bool Enabled=true;
 UPROPERTY(BlueprintReadWrite) bool NewItem=false;
 UPROPERTY(BlueprintReadWrite) bool Selected=false;
 UPROPERTY(BlueprintReadWrite) FString PlacementKey;
 UPROPERTY(BlueprintReadWrite) int32 BagCell=-1;
};
UCLASS()
class DUNGEONWORLDOPUS_API UOpusHoverEvents : public UObject
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintAssignable) FSvgCanvasInvoked Entered;
 UPROPERTY(BlueprintAssignable) FSvgCanvasInvoked Left;
};
UCLASS()
class DUNGEONWORLDOPUS_API UOpusActionNode : public USvgCanvasMotionNode
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintReadOnly) TObjectPtr<USvgCanvasScreen> Screen;
 UPROPERTY(BlueprintReadOnly) FString HoverId;
 UPROPERTY(BlueprintReadOnly) FString PlacementKey;
 UPROPERTY(BlueprintReadOnly) int32 BagCell=-1;
 UPROPERTY(BlueprintReadOnly) FString EquipmentSlot;
 UPROPERTY() TObjectPtr<UOpusActionNode> HoverOwner;
 UPROPERTY() TObjectPtr<UOpusHoverEvents> HoverEvents;
 UPROPERTY() float FlexibleInset=0;
 UFUNCTION() void FitSurface();
 UFUNCTION() void Relay(USvgCanvasNode* Node){if(Screen)Screen->OnSourceAction.Broadcast(Node);}
 UFUNCTION() void EnterItem(){if(HoverEvents&&HoverOwner&&!HoverOwner->HoverId.IsEmpty())HoverEvents->Entered.Broadcast(HoverOwner);}
 UFUNCTION() void LeaveItem(){if(HoverEvents&&HoverOwner&&!HoverOwner->HoverId.IsEmpty())HoverEvents->Left.Broadcast(HoverOwner);}
};
UCLASS()
class DUNGEONWORLDOPUS_API UOpusUI : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 // Commandlet authoring adapters for UMG fields intentionally absent from Python.
 UFUNCTION(BlueprintCallable) static USvgCanvasNode* CreateEditorNode(UObject* Blueprint,const FString& Name);
 UFUNCTION(BlueprintCallable) static bool FinalizeEditorHUD(UObject* Blueprint);
 UFUNCTION(BlueprintCallable) static UOpusHoverEvents* HoverEvents(USvgCanvasScreen* Screen);
 UFUNCTION(BlueprintCallable) static USvgCanvasScreen* CreateHUD(APlayerController* Player);
 UFUNCTION(BlueprintPure) static FOpusUIAction Action(USvgCanvasNode* Node);
 UFUNCTION(BlueprintCallable) static void HidePrefix(USvgCanvasScreen* Screen,const FString& Prefix);
 UFUNCTION(BlueprintCallable) static void HideSurface(USvgCanvasScreen* Screen,const FString& SourceId);
 UFUNCTION(BlueprintCallable) static void SetImage(USvgCanvasScreen* Screen,const FString& Binding,const FString& Path);
 UFUNCTION(BlueprintCallable) static void SetText(USvgCanvasScreen* Screen,const FString& Binding,const FString& Value);
 UFUNCTION(BlueprintCallable) static void SetProgress(USvgCanvasScreen* Screen,const FString& Binding,float Value);
 UFUNCTION(BlueprintCallable) static void SetShown(USvgCanvasScreen* Screen,const FString& Binding,bool Shown);
 UFUNCTION(BlueprintCallable) static void BindNode(USvgCanvasScreen* Screen,const FString& SourceId,const FString& Binding);
 UFUNCTION(BlueprintCallable) static void SetAction(USvgCanvasScreen* Screen,const FString& Binding,const FString& Op,const FString& Value);
 UFUNCTION(BlueprintCallable) static void SetRect(USvgCanvasScreen* Screen,const FString& Binding,FVector4 Rect);
 UFUNCTION(BlueprintCallable) static void SetEnabled(USvgCanvasScreen* Screen,const FString& Binding,bool Enabled);
 UFUNCTION(BlueprintCallable) static void SetSelection(USvgCanvasScreen* Screen,const FString& BindingPrefix,int32 Selected,int32 Count);
 UFUNCTION(BlueprintCallable) static void Region(USvgCanvasScreen* Screen,const FString& ParentId,const FString& Binding,FVector4 Rect,const FString& HidePrefix);
 UFUNCTION(BlueprintCallable) static void SetInput(USvgCanvasScreen* Screen,const FString& Binding,const FString& Initial);
 UFUNCTION(BlueprintPure) static FString Input(USvgCanvasScreen* Screen,const FString& Binding);
 UFUNCTION(BlueprintPure) static bool TextFocused(USvgCanvasScreen* Screen);
 UFUNCTION(BlueprintPure) static int32 IntInput(USvgCanvasScreen* Screen,const FString& Binding,int32 Fallback);
 UFUNCTION(BlueprintCallable) static void SetRows(USvgCanvasScreen* Screen,const FString& Binding,const TArray<FOpusUIRow>& Rows,int32 Columns=1);
 UFUNCTION(BlueprintPure) static bool BlocksPointer(USvgCanvasScreen* Screen);
 UFUNCTION(BlueprintCallable) static void SetActionText(USvgCanvasScreen* Screen,const FString& Type,int32 Index,const FString& Text);
 UFUNCTION(BlueprintPure) static bool BlocksCombat(USvgCanvasScreen* Screen);
 UFUNCTION(BlueprintPure) static FString SourceHash(USvgCanvasScreen* Screen);
 UFUNCTION(BlueprintPure) static int32 StateCount(USvgCanvasScreen* Screen);
 UFUNCTION(BlueprintPure) static bool HasVisibleBinding(USvgCanvasScreen* Screen,const FString& Binding);
};
