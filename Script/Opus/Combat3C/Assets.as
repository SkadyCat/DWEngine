namespace DWO3CAssets
{
 FString Montage(FString Id,FName Key){return DWOModels::ObjectPath("/Game/ThreeCLab/Animation/"+Id+"/AM_"+Key);}
 bool Calibrated(FString Id){return DWO3CEquipment::ForModel(Id).Num()>0||DWO3CEquipment::BuiltInCount(Id)>0;}
 void Prepare(FString Id,TArray<FString>& Assets)
 {
  auto Model=DWOModels::Streamable(Id);Assets.AddUnique(Model.Mesh);
  for(auto C:Model.Clips){Assets.AddUnique(C.Value);Assets.AddUnique(Montage(Id,C.Key));}
  for(auto G:DWO3CEquipment::ForModel(Id)){Assets.AddUnique(DWOModels::ObjectPath(G.Asset));if(!G.Animation.IsEmpty())Assets.AddUnique(DWOModels::ObjectPath(G.Animation));}
 }
}
