// UICreator owns the card and list geometry; AS owns selection and persistence.
class UDWOMapCoreUI : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() TArray<FOpusEntry> Cores;
 TArray<int> Packs;
 FString LastState;
 FName LastCore;
 void Initialize(UDWOSvgHUD InHud)
 {
  Hud=InHud;Cores=UOpusData::MapCores(Hud.Tower.Rules);
  for(auto Core:Cores){FOpusMapData Data;FString Error;if(UOpusNative::MapCoreData(Hud.Tower.Rules,FName(Core.Id),Data,Error))Packs.Add(int(Data.Packs));else Packs.Add(0);}
  Hud.Screen.OnSourceAction.AddUFunction(this,n"OnAction");Refresh();
 }
 void Close(){Hud.Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Hud.Tower.Rules)+1));Refresh();}
 UFUNCTION() void OnAction(USvgCanvasNode Node)
 {
  auto Action=UOpusUI::Action(Node);if(Action.Type!="map-core")return;
  Hud.Screen.SourceActionHandled=true;
  if(Action.Op=="open"){if(Hud.Screen.CurrentState=="map-core-picker")Close();else {Hud.Screen.ApplySceneState("map-core-picker");Refresh();}}
  else if(Action.Op=="close")Close();
  else if(Action.Op=="equip"&&Hud.Tower.ApplyMapCore(FName(Action.Value)))Close();
 }
 bool FilterKey(FKey Key)
 {if(Key==EKeys::Escape&&Hud!=nullptr&&Hud.Screen!=nullptr&&Hud.Screen.CurrentState=="map-core-picker"){Close();return true;}return false;}
 void Refresh()
 {
  if(Hud==nullptr||Hud.Screen==nullptr)return;
  auto Journey=UDWOTowerJourney::Get();int Selected=-1;
  for(int I=0;I<Cores.Num();I++)
  {
   auto Core=Cores[I];bool Equipped=FName(Core.Id)==Journey.MapCore;FString Binding="map-core-choice-"+I;
   if(Equipped){Selected=I;UOpusUI::SetText(Hud.Screen,"map-core-equipped:name",Core.Name);UOpusUI::SetText(Hud.Screen,"map-core-equipped:tier","T"+Core.Tier);UOpusUI::SetText(Hud.Screen,"map-core-equipped:packs","已装配 · 点击更换");}
   UOpusUI::SetText(Hud.Screen,Binding+":name",Core.Name);UOpusUI::SetText(Hud.Screen,Binding+":tier","T"+Core.Tier);
   UOpusUI::SetText(Hud.Screen,Binding+":packs","怪物群 +"+Packs[I]+"%");UOpusUI::SetText(Hud.Screen,Binding+":selected",Equipped?"已装配":"");
  }
  if(Selected>=0)UOpusUI::SetSelection(Hud.Screen,"map-core-choice-",Selected,Cores.Num());
  LastState=Hud.Screen.CurrentState;LastCore=Journey.MapCore;
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {if(Hud!=nullptr&&Hud.Screen!=nullptr&&(LastState!=Hud.Screen.CurrentState||LastCore!=UDWOTowerJourney::Get().MapCore))Refresh();}
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){if(Hud!=nullptr&&IsValid(Hud.Screen))Hud.Screen.OnSourceAction.UnbindObject(this);}
}
