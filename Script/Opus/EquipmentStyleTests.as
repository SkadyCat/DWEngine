// Presentation-only fixture; enabled only for transient verification sessions.
class UDWOEquipmentStyleTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 double Clock=0;bool Done=false;
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 bool Check(bool Good,FString What){if(!Good){UOpusNative::WriteLog("EQUIPMENT_STYLE_FAIL "+What);UOpusNative::Quit(false);}return Good;}
 UOpusActionNode Find(UWidget W,FString Id)
 {
  auto N=Cast<UOpusActionNode>(W);if(N!=nullptr&&N.HoverId==Id)return N;
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Find(C,Id);if(Found!=nullptr)return Found;}return nullptr;
 }
 USvgCanvasNode Cell(UWidget W)
 {
  auto N=Cast<USvgCanvasNode>(W);if(N!=nullptr&&N.SourceType=="Button"&&N.SourceId.EndsWith("::battle-cell"))return N;
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Cell(C);if(Found!=nullptr)return Found;}return nullptr;
 }
 USvgCanvasNode Icon(UWidget W)
 {
  auto N=Cast<USvgCanvasNode>(W);if(N!=nullptr&&N.SourceId.EndsWith("::battle-cell-icon"))return N;
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Icon(C);if(Found!=nullptr)return Found;}return nullptr;
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  Clock+=Dt;if(Done||Clock<1||Tower==nullptr||Tower.Hud.Screen==nullptr)return;Done=true;
  auto H=Tower.Hud;H.SetComponentTickEnabled(false);auto S=H.Screen;
  FString View=UOpusNative::StringOption("DWOView");S.ApplySceneState(View=="inventory"?"inventory":"character");
  TArray<FString> Qualities;Qualities.Add("common");Qualities.Add("magic");Qualities.Add("rare");Qualities.Add("legendary");
  TArray<FString> Labels;Labels.Add("普通 · 霜棘法杖");Labels.Add("魔法 · 霜棘法杖");Labels.Add("稀有 · 霜棘法杖");Labels.Add("传奇 · 霜棘法杖");
  TArray<FString> Slots;Slots.Add("weapon");Slots.Add("offhand");Slots.Add("helmet");Slots.Add("chest");
  TArray<FOpusUIRow> Rows;
  for(int I=0;I<4;I++){
   FOpusUIRow R;R.Value="quality-"+Qualities[I];R.Label=Labels[I];R.Rarity=Qualities[I];R.Kind="equipment";R.Op="select";R.Icon=FPaths::ProjectContentDir()+"OpusIcons/equipment/frostthorn_staff.png";R.Tier=1;R.Count=1;R.Slot=Slots[I];R.Equipped=true;Rows.Add(R);
  }
  FString Binding=View=="inventory"?"bag-grid":"character-grid";UOpusUI::SetRows(S,Binding,Rows);
  for(int I=0;I<4;I++){
   auto N=Cell(Find(S.GetRootWidget(),Rows[I].Value));
   if(!Check(N!=nullptr,"live equipment row exists "+Qualities[I]))return;
   auto Artwork=Icon(N);if(!Check(Artwork!=nullptr&&Artwork.DesignRect.Z>0&&Artwork.DesignRect.W>0,"newly filled cell retains visible icon geometry "+Qualities[I]))return;
   if(!Check(N.Normal.SourceSvg.Contains("equipment-cell-"+Qualities[I]+"-normal"),"actual rarity selects authored skin "+Qualities[I]))return;
   if(!Check(N.Hovered.SourceSvg.Contains("equipment-cell-"+Qualities[I]+"-hovered")&&N.Pressed.SourceSvg.Contains("equipment-cell-"+Qualities[I]+"-pressed"),"hover and press keep actual rarity "+Qualities[I]))return;
  }
  // A reroll can change rarity without changing the instance ID or its icon.
  Rows[0].Rarity="legendary";UOpusUI::SetRows(S,Binding,Rows);
  if(!Check(Cell(Find(S.GetRootWidget(),Rows[0].Value)).Normal.SourceSvg.Contains("equipment-cell-legendary-normal"),"rarity-only update invalidates row cache"))return;
  Rows[0].Rarity="common";UOpusUI::SetRows(S,Binding,Rows);
  if(View!="inventory")UOpusUI::SetRows(S,"gear-list",Rows);
  UOpusUI::SetText(S,"character-capacity","4 / 4");UOpusUI::SetText(S,"gear-count","4 / 9");
  UOpusNative::WriteLog("EQUIPMENT_STYLE_PASS FourRaritiesAndReplacement");
 }
}
