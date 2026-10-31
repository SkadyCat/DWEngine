#include "DWRuleWorldSubsystem.h"

bool UDWRuleWorldSubsystem::DropPlayerInventory(FName Kind,const FString& Id,int32 Count,FVector Position,FString& Key,FString& Error)
{
 Key.Reset();Error.Reset();
 if(!FloorRewards||!Player||!Host){Error=TEXT("当前没有可放置掉落的地图");return false;}
 if(IsPlayerItemLocked(Id)){Error=TEXT("该装备已被祭坛锁定");return false;}
 auto NextProfile=PlayerProfile;auto NextRewards=*FloorRewards;FString NextKey;
 if(!NextRewards.DropInventory(Host->Library,NextProfile,Kind,Id,Count,FVector2d(Position.X,Position.Y),Player->Character.bDead,NextKey,Error))return false;
 if(!ApplyPlayerProfile(MoveTemp(NextProfile),Error))return false;
 *FloorRewards=MoveTemp(NextRewards);Key=MoveTemp(NextKey);return true;
}
