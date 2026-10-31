#pragma once
#include "DWRuleLibrary.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"

namespace OpusTraitText
{
inline FString Format(const FString& Text,const DWRules::FDWTraitEntry& Entry,const DWRules::FDWRuleLibrary& Library)
{
 const auto* Definition=Library.Traits.Find(Entry.Id);auto Params=Definition?Definition->Source.Params:TMap<FName,double>();
 for(const auto& P:Entry.Params)Params.Add(P.Key,P.Value);
 FString Result=Text;
 // Older authoring labels repeat "n" for differently named parameters. The
 // JSON authoring order defines those slots; never use TMap iteration order.
 if(Definition&&!Params.Contains(TEXT("n")))
 {
  static TMap<FString,TArray<FName>> Orders;auto* Order=Orders.Find(Definition->Path);
  if(!Order)
  {
   TArray<FName> Keys;FString Source;
   if(FFileHelper::LoadFileToString(Source,*Definition->Path))
   {
    FRegexMatcher Block(FRegexPattern(TEXT("\"params\"\\s*:\\s*\\{([^}]*)\\}")),Source);
    if(Block.FindNext()){FRegexMatcher Key(FRegexPattern(TEXT("\"([^\"]+)\"\\s*:")),Block.GetCaptureGroup(1));while(Key.FindNext())Keys.Add(FName(Key.GetCaptureGroup(1)));}
   }
   Order=&Orders.Add(Definition->Path,MoveTemp(Keys));
  }
  if(!Order->IsEmpty())
  {
   FRegexMatcher Match(FRegexPattern(TEXT("(?<![A-Za-z_])n(?![A-Za-z_])")),Result);FString Next;int32 Last=0,Index=0;
   while(Match.FindNext())
   {
    const auto* Value=Params.Find((*Order)[FMath::Min(Index++,Order->Num()-1)]);
    Next+=Result.Mid(Last,Match.GetMatchBeginning()-Last)+(Value?FString::Printf(TEXT("%.4g"),*Value):Match.GetCaptureGroup(0));Last=Match.GetMatchEnding();
   }
   if(Last)Result=Next+Result.Mid(Last);
  }
 }
 TArray<FName> Keys;Params.GetKeys(Keys);Keys.Sort([](FName A,FName B){return A.ToString().Len()>B.ToString().Len();});
 for(FName Key:Keys)
 {
  const FString Name=Key.ToString(),Value=FString::Printf(TEXT("%.4g"),Params[Key]);Result.ReplaceInline(*(TEXT("{")+Name+TEXT("}")),*Value);
  FRegexMatcher Match(FRegexPattern(TEXT("(?<![A-Za-z_])")+Name+TEXT("(?![A-Za-z_])")),Result);FString Next;int32 Last=0;
  while(Match.FindNext()){Next+=Result.Mid(Last,Match.GetMatchBeginning()-Last)+Value;Last=Match.GetMatchEnding();}if(Last)Result=Next+Result.Mid(Last);
 }
 return Result;
}
}
