// Studio placement policy. This metadata never grants or removes ownership.
class UDWOInventoryLayout : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 TArray<FOpusStoredStack> Cells;int NextKey=1;
 void Initialize(UDWOSvgHUD H){Hud=H;}
 int Limit(FOpusEntry E){return E.Kind=="skill"?1:9999;}
 int EntryAt(FString Ref){for(int I=0;I<Hud.Bag.Num();I++)if(Hud.Bag[I].Key==Ref)return I;return -1;}
 FString KeyFor(FString Preferred,TArray<FString>& Used){if(!Preferred.IsEmpty()&&!Used.Contains(Preferred)){Used.Add(Preferred);return Preferred;}FString K;do{K="stack:"+NextKey++;}while(Used.Contains(K));Used.Add(K);return K;}
 void Trim(){while(Cells.Num()>0&&Cells.Last().Ref.IsEmpty())Cells.RemoveAt(Cells.Num()-1);}
 void Commit(){FString Error;UOpusGameplayStore::SetInventoryCells(Hud.Tower.Rules,Cells,NextKey,Error);}
 void Reconcile()
 {
  // Placement describes ordinary stock only. Keeping worn copies here creates
  // invisible occupied cells which appear/disappear during equip and drop.
  Cells=UOpusGameplayStore::InventoryCells(Hud.Tower.Rules);NextKey=UOpusGameplayStore::InventoryNextKey(Hud.Tower.Rules);TArray<int> Left;for(auto E:Hud.Bag)Left.Add(DWOInventory::Count(E,"stock"));TArray<FString> Used;
  for(int I=0;I<Cells.Num();I++){auto C=Cells[I];int At=EntryAt(C.Ref);if(At<0||Left[At]<=0||C.Count<1){Cells[I]=FOpusStoredStack();continue;}C.Count=Math::Min(C.Count,Math::Min(Left[At],Limit(Hud.Bag[At])));C.Key=KeyFor(C.Key,Used);Left[At]-=C.Count;Cells[I]=C;}
  for(int I=0;I<Hud.Bag.Num();I++){
   auto E=Hud.Bag[I];for(int J=0;J<Cells.Num()&&Left[I]>0;J++)if(Cells[J].Ref==E.Key){int Add=Math::Min(Left[I],Limit(E)-Cells[J].Count);Cells[J].Count+=Add;Left[I]-=Add;}
   while(Left[I]>0){FOpusStoredStack C;C.Key=KeyFor(E.Key,Used);C.Ref=E.Key;C.Count=Math::Min(Left[I],Limit(E));Left[I]-=C.Count;int At=-1;for(int J=0;J<Cells.Num();J++)if(Cells[J].Ref.IsEmpty()){At=J;break;}if(At<0)Cells.Add(C);else Cells[At]=C;}
  }Trim();Commit();
 }
 FOpusUIRow CellRow(int Index,FString Query="")
 {
  FOpusUIRow R;R.BagCell=Index;R.Enabled=false;if(!Cells.IsValidIndex(Index)||Cells[Index].Ref.IsEmpty())return R;
  auto C=Cells[Index];int At=EntryAt(C.Ref);if(At<0)return R;auto E=Hud.Bag[At];int Count=C.Count;
  if(Count<=0||(!Query.IsEmpty()&&!E.Name.Contains(Query)&&!E.Description.Contains(Query)))return R;
  R=Hud.BagRow(E,"stock");R.Count=Count;R.PlacementKey=C.Key;R.BagCell=Index;R.Selected=Hud.SelectedKey==C.Key&&Hud.SelectedCompartment=="stock";return R;
 }
 TArray<FOpusUIRow> CharacterGrid(FString Query)
 {
  TArray<FOpusUIRow> Out;for(int I=0;I<Cells.Num();I++){auto R=CellRow(I,Query);int At=EntryAt(Cells[I].Ref);if(At>=0&&Hud.GearFilter!="all"&&(Hud.Bag[At].Kind!="equipment"||(Hud.GearFilter!="equipment"&&Hud.Bag[At].Slot!=Hud.GearFilter)))continue;if(!Cells[I].Ref.IsEmpty()&&R.Value.IsEmpty())continue;Out.Add(R);}
  int Size=Math::Max(36,Math::CeilToInt(double(Out.Num())/4)*4);for(int I=Cells.Num();Out.Num()<Size;I++){FOpusUIRow R;R.BagCell=I;R.Enabled=false;Out.Add(R);}return Out;
 }
 TArray<FOpusUIRow> PageGrid(FString Query,int Page)
 {TArray<FOpusUIRow> Out;for(int I=Page*72;I<(Page+1)*72;I++)Out.Add(CellRow(I,Query));return Out;}
 int Find(FString Key){for(int I=0;I<Cells.Num();I++)if(Cells[I].Key==Key)return I;return -1;}
 bool Move(FString Key,int Target,bool EmptyOnly=true)
 {
  Reconcile();int From=Find(Key),Capacity=Math::Max(72,Math::CeilToInt(double(Cells.Num())/72)*72)+72;if(From<0||Target<0||Target>=Capacity)return false;
  if(From==Target)return true;if(Cells.IsValidIndex(Target)&&!Cells[Target].Ref.IsEmpty()&&EmptyOnly)return false;
  while(Cells.Num()<=Target)Cells.Add(FOpusStoredStack());auto C=Cells[From];Cells[From]=Cells[Target];Cells[Target]=C;Trim();Commit();return true;
 }
}
