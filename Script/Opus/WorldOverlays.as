// UICreator owns the visuals; AS owns RuleStudio loot presentation and input.
class UDWOWorldOverlays : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() TMap<FString,UOpusWorldScreen> Labels;
 UPROPERTY() TMap<FString,ADWOFloorLootMarker> Markers;
 UPROPERTY() TArray<FOpusEntry> Ground;
 UPROPERTY() UDWOLootHunter Hunter;
 UPROPERTY() UOpusWorldScreen DetailPanel;
 UPROPERTY() UOpusActionNode DetailAnchor;
 FString DetailKey;
 FString Highlighted;
 bool Pinned=false;
 double RefreshIn=0,Clock=0,LeaveIn=-1;
 void Initialize(ADWOTowerGameMode InTower){Tower=InTower;Hunter=UDWOLootHunter::Create(Tower);Hunter.Initialize(Tower);}
 bool TextFocused() const{return Hunter!=nullptr&&Hunter.TextFocused();}
 bool PanelUnderPointer(UOpusWorldScreen Panel,APlayerController PC) const
 {
  if(Panel==nullptr||!Panel.IsVisible()||Panel.GetVisibility()==ESlateVisibility::HitTestInvisible)return false;
  float32 X=0,Y=0;if(!PC.GetMousePosition(X,Y))return false;auto A=UOpusHoverUI::AnchorRect(Panel,PC);
  return X>=A.X&&X<=A.Z&&Y>=A.Y&&Y<=A.W;
 }
 bool BlocksPointer(APlayerController PC) const{return PanelUnderPointer(Hunter==nullptr?nullptr:Hunter.Panel,PC)||PanelUnderPointer(DetailPanel,PC);}
 UFUNCTION() bool FilterKey(FKey Key)
 {
  if(!DetailKey.IsEmpty())
  {
   if(Key==EKeys::LeftControl||Key==EKeys::RightControl){Pin();return true;}
   if(Key==EKeys::Escape){CloseDetails();return true;}
  }
  return Hunter!=nullptr&&Hunter.FilterKey(Key);
 }
 bool HandleKeys(APlayerController PC)
 {
  if(!DetailKey.IsEmpty()&&(PC.WasInputKeyJustPressed(EKeys::LeftControl)||PC.WasInputKeyJustPressed(EKeys::RightControl)))Pin();
  if(PC.WasInputKeyJustPressed(EKeys::Z)){Tower.Hud.ShowLoot=!Tower.Hud.ShowLoot;Tower.Hud.SaveHunter();RefreshIn=0;}
  return PC.WasInputKeyJustPressed(EKeys::Escape)&&FilterKey(EKeys::Escape);
 }
 UFUNCTION() void OnAction(USvgCanvasNode Node)
 {
  if(Tower==nullptr)return;auto A=UOpusUI::Action(Node);
  if(A.Op=="pickup")Tower.Hero.PickLoot(A.Value);
  if(A.Op=="pickup-detail"&&!DetailKey.IsEmpty())Tower.Hero.PickLoot(DetailKey);
  if(A.Op=="details-close")CloseDetails();
 }
 UFUNCTION() void Enter(USvgCanvasNode Node)
 {
  auto N=Cast<UOpusActionNode>(Node);if(Pinned||N==nullptr||N.HoverId.IsEmpty())return;
  if(DetailKey==N.HoverId){LeaveIn=-1;return;}CloseDetails();DetailKey=N.HoverId;DetailAnchor=N;
  auto PC=Gameplay::GetPlayerController(0);Pinned=PC!=nullptr&&(PC.IsInputKeyDown(EKeys::LeftControl)||PC.IsInputKeyDown(EKeys::RightControl));RefreshDetails();PlaceDetails();
 }
 UFUNCTION() void Leave(USvgCanvasNode Node){if(!Pinned&&Node==DetailAnchor)LeaveIn=0;}
 void Pin(){if(DetailKey.IsEmpty())return;Pinned=true;LeaveIn=-1;RefreshDetails();}
 void CloseDetails(){if(DetailPanel!=nullptr){DetailPanel.OnSourceAction.UnbindObject(this);DetailPanel.RemoveFromParent();}DetailPanel=nullptr;DetailAnchor=nullptr;DetailKey="";Pinned=false;LeaveIn=-1;}
 void RefreshDetails()
 {
  if(DetailKey.IsEmpty())return;FOpusEntry Item;bool Found=false;for(auto E:Ground)if(E.Key==DetailKey){Item=E;Found=true;break;}
  if(!Found||!Tower.Hud.HunterAllows(Item)||!Labels.Contains(DetailKey)){CloseDetails();return;}
  if(DetailPanel==nullptr)
  {
   DetailPanel=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"details","UICreatorLoot",120);
   if(DetailPanel==nullptr){Tower.Fail("Missing UICreator loot details");return;}
   DetailPanel.OnSourceAction.AddUFunction(this,n"OnAction");DetailPanel.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");
  }
  FString Category=Item.Category=="currency"?"通货":Item.Category=="omen"?"征兆":Item.Category=="consumable"?"消耗品":Item.Category=="material"?"材料":"";
  UOpusUI::SetText(DetailPanel,"details:name",DWOLoot::Name(Item));UOpusUI::SetText(DetailPanel,"details:type",(Item.Kind=="equipment"?"装备":"道具")+(Category.IsEmpty()?"":" · "+Category)+(Pinned?" · 已固定":""));UOpusUI::SetText(DetailPanel,"details:description",Item.Description.IsEmpty()?Item.Name:Item.Description);
  UOpusWorldUI::CopyTextColor(DetailPanel,"details:name",Labels[DetailKey],"loot:name");
  DetailPanel.SetVisibility(Pinned?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::HitTestInvisible);
 }
 void PlaceDetails()
 {
  if(DetailPanel==nullptr||DetailAnchor==nullptr)return;auto PC=Gameplay::GetPlayerController(0);auto V=UOpusHoverUI::ViewSize(PC);auto A=UOpusHoverUI::AnchorRect(DetailAnchor,PC);
  double Scale=Math::Max(0.1,Math::Min(1.0,Math::Min((V.X-24)/320.0,(V.Y-24)/340.0)));double W=320*Scale,H=340*Scale,X=A.Z+12,Y=A.Y;if(X+W>V.X-12)X=A.X-W-12;
  X=Math::Clamp(X,12.0,Math::Max(12.0,V.X-W-12));Y=Math::Clamp(Y,12.0,Math::Max(12.0,V.Y-H-12));UOpusHoverUI::Place(DetailPanel,FVector2D(X,Y),float32(Scale));
 }
 void Refresh()
 {
  Ground=UOpusData::Ground(Tower.Rules);TArray<FString> Live,Visible;TArray<FOpusEntry> Eligible;auto PC=Gameplay::GetPlayerController(0);
  for(auto E:Ground)
  {
   Live.Add(E.Key);if(!Markers.Contains(E.Key)){auto M=Cast<ADWOFloorLootMarker>(SpawnActor(ADWOFloorLootMarker));M.Initialize(E,Clock);Markers.Add(E.Key,M);}
   if(Tower.Hud.HunterAllows(E))Eligible.Add(E);
  }
  for(int I=0;I<Math::Min(96,Eligible.Num());I++)
  {
   int Best=I;double D=(Eligible[I].Position-Tower.Hero.ActorLocation).SizeSquared2D();
   for(int J=I+1;J<Eligible.Num();J++){double Next=(Eligible[J].Position-Tower.Hero.ActorLocation).SizeSquared2D();if(Next<D){D=Next;Best=J;}}
   if(Best!=I){auto Swap=Eligible[I];Eligible[I]=Eligible[Best];Eligible[Best]=Swap;}
  }
  for(int I=0;I<Math::Min(96,Eligible.Num());I++)
  {
   auto E=Eligible[I];auto Marker=Markers[E.Key];if(!DWOLoot::Landed(Clock-Marker.Born,Marker.Seed))continue;Visible.Add(E.Key);
   if(Labels.Contains(E.Key))continue;
   auto Label=UOpusWorldUI::CreateTemplate(PC,DWOLoot::Family(E)+"-template","UICreatorLoot",60);if(Label==nullptr){Tower.Fail("Missing UICreator ground loot template");return;}
   UOpusUI::SetAction(Label,"loot:pick","pickup",E.Key);UOpusWorldUI::SetHover(Label,"loot:pick",E.Key);UOpusWorldUI::FitLabel(Label,"loot:name",DWOLoot::Name(E),E.Category=="currency"||E.Category=="omen"?45:18);
   Label.OnSourceAction.AddUFunction(this,n"OnAction");auto Events=UOpusUI::HoverEvents(Label);Events.Entered.AddUFunction(this,n"Enter");Events.Left.AddUFunction(this,n"Leave");Labels.Add(E.Key,Label);
  }
  TArray<FString> Old;Labels.GetKeys(Old);for(auto Key:Old)if(!Visible.Contains(Key)){auto L=Labels[Key];L.OnSourceAction.UnbindObject(this);UOpusUI::HoverEvents(L).Entered.UnbindObject(this);UOpusUI::HoverEvents(L).Left.UnbindObject(this);L.RemoveFromParent();Labels.Remove(Key);}
  Old.Empty();Markers.GetKeys(Old);for(auto Key:Old)if(!Live.Contains(Key)){Markers[Key].DestroyActor();Markers.Remove(Key);}if(!DetailKey.IsEmpty())RefreshDetails();
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Tower==nullptr||Tower.Rules==nullptr||Tower.Hero==nullptr||UOpusNative::Flag("DWONoHUD"))return;
  if(!Tower.Paused)Clock+=Math::Clamp(double(DeltaSeconds),0.0,0.05);
  RefreshIn-=DeltaSeconds;if(RefreshIn<=0){RefreshIn=0.1;Refresh();}
  bool Shown=Tower.Hud.ShowLoot&&!UOpusUI::BlocksCombat(Tower.Hud.Screen);
  TArray<FVector> Anchors;TArray<UOpusWorldScreen> Screens;
  for(auto E:Ground)
  {
   if(Markers.Contains(E.Key))Markers[E.Key].Update(Clock,Shown&&Tower.Hud.HunterAllows(E));if(!Labels.Contains(E.Key))continue;
   auto Label=Labels[E.Key];if(!Shown){Label.SetVisibility(ESlateVisibility::Collapsed);continue;}
   if(Highlighted!=Tower.Hero.PickupTarget&&(E.Key==Highlighted||E.Key==Tower.Hero.PickupTarget))UOpusWorldUI::Emphasize(Label,"loot:pick",Tower.Hero.PickupTarget==E.Key);Anchors.Add(E.Position+FVector(0,0,20));Screens.Add(Label);
  }
  // The source view budgets 96 labels; spend that budget on nearby loot.
  for(int I=0;I<Math::Min(96,Anchors.Num());I++)
  {
   int Best=I;double Distance=(Anchors[I]-Tower.Hero.ActorLocation).SizeSquared2D();
   for(int J=I+1;J<Anchors.Num();J++){double D=(Anchors[J]-Tower.Hero.ActorLocation).SizeSquared2D();if(D<Distance){Distance=D;Best=J;}}
   if(Best!=I){auto At=Anchors[I];auto Screen=Screens[I];Anchors[I]=Anchors[Best];Screens[I]=Screens[Best];Anchors[Best]=At;Screens[Best]=Screen;}
  }
  UOpusWorldUI::PlaceLabels(Gameplay::GetPlayerController(0),Screens,Anchors);
  Highlighted=Tower.Hero.PickupTarget;if(!Shown)CloseDetails();if(UOpusUI::BlocksCombat(Tower.Hud.Screen)&&Hunter!=nullptr&&Hunter.Visible())Hunter.Close();
  if(!DetailKey.IsEmpty())
  {
   if(LeaveIn>=0&&!Pinned){LeaveIn+=DeltaSeconds;if(LeaveIn>=0.06)CloseDetails();}
   if(DetailPanel!=nullptr&&(!Labels.Contains(DetailKey)||!Labels[DetailKey].IsVisible()))CloseDetails();PlaceDetails();
  }
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason)
 {
  CloseDetails();for(auto Pair:Labels)if(Pair.Value!=nullptr){Pair.Value.OnSourceAction.UnbindObject(this);UOpusUI::HoverEvents(Pair.Value).Entered.UnbindObject(this);UOpusUI::HoverEvents(Pair.Value).Left.UnbindObject(this);Pair.Value.RemoveFromParent();}Labels.Empty();for(auto Pair:Markers)if(IsValid(Pair.Value))Pair.Value.DestroyActor();Markers.Empty();
 }
}
