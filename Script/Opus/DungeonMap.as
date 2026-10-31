// Live native dungeon geometry with UICreator's circular mini / expanded controls.
class UDWODungeonMap : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UOpusWorldScreen Mini;
 UPROPERTY() UOpusWorldScreen Panel;
 TArray<FVector4> Cells,Rooms;TArray<int> Visited;
 FVector4 WorldBounds;
 FVector2D Center,Last,ReboundFrom,ReboundTo;
 double Zoom=1,FitZoom=1,MiniZoom=1,Poll=0,Rebound=-1;
 bool GeometryReady=false,Visible=false,WasPaused=false,Held=false,Panning=false,Suppress=false;
 void Initialize(ADWOTowerGameMode T)
 {
  Tower=T;Mini=Create("dungeon-minimap",115);if(Mini!=nullptr)UOpusUI::HidePrefix(T.Hud.Screen,"rs-minimap");
 }
 UOpusWorldScreen Create(FString Id,int Layer)
 {
  auto S=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Id,"UICreatorStudio",Layer);
  if(S!=nullptr){S.OnSourceAction.AddUFunction(this,n"Action");S.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");}else Tower.LastError="缺少 UICreator 地牢地图 "+Id;return S;
 }
 FVector2D Hero() const{return FVector2D(Tower.Hero.ActorLocation.X,Tower.Hero.ActorLocation.Y)/100;}
 void Geometry()
 {
  if(GeometryReady||!Tower.Dungeon.Ready)return;Cells=UOpusDungeon::MapCells(Tower);Rooms=UOpusDungeon::MapRooms(Tower);if(Cells.Num()==0)return;
  double Left=1e20,Top=1e20,Right=-1e20,Bottom=-1e20;for(auto C:Cells){Left=Math::Min(Left,C.X);Top=Math::Min(Top,C.Y);Right=Math::Max(Right,C.X+C.Z);Bottom=Math::Max(Bottom,C.Y+C.W);}
  WorldBounds=FVector4(Left,Top,Right-Left,Bottom-Top);FitZoom=Math::Min(660.0/WorldBounds.Z,440.0/WorldBounds.W);Zoom=FitZoom;Center=FVector2D(Left+WorldBounds.Z/2,Top+WorldBounds.W/2);GeometryReady=true;
 }
 bool Open()
 {
  if(Tower.Traveling||Tower.Preparing||Tower.Hud.MapDevice.Visible||Tower.Hud.Talents.Visible||Tower.Events.IsOpen()||Tower.Recent.UI.Panel!=nullptr||Tower.Studio.UI.Panel!=nullptr)return false;
  Geometry();if(!GeometryReady)return false;if(Visible){Close();return true;}
  if(Panel==nullptr)Panel=Create("dungeon-map",155);if(Panel==nullptr)return false;
  WasPaused=Tower.Paused;Tower.SetPaused(true);Visible=true;Held=UOpusMapUI::MouseDown();Panning=false;Rebound=-1;Panel.SetVisibility(ESlateVisibility::SelfHitTestInvisible);UOpusMapUI::Fit(Panel,Gameplay::GetPlayerController(0));Fit();Draw();return true;
 }
 void Close(){Visible=false;Panning=false;Rebound=-1;if(Panel!=nullptr)Panel.SetVisibility(ESlateVisibility::Collapsed);Tower.SetPaused(WasPaused);}
 void Fit(){Zoom=FitZoom;Center=FVector2D(WorldBounds.X+WorldBounds.Z/2,WorldBounds.Y+WorldBounds.W/2);Rebound=-1;}
 FVector2D ClampCenter(FVector2D P) const
 {
  double HalfX=(724.0/2-32)/Zoom,HalfY=(504.0/2-32)/Zoom;
  return FVector2D(WorldBounds.Z<=HalfX*2?WorldBounds.X+WorldBounds.Z/2:Math::Clamp(P.X,WorldBounds.X+HalfX,WorldBounds.X+WorldBounds.Z-HalfX),WorldBounds.W<=HalfY*2?WorldBounds.Y+WorldBounds.W/2:Math::Clamp(P.Y,WorldBounds.Y+HalfY,WorldBounds.Y+WorldBounds.W-HalfY));
 }
 void Locate(){Center=ClampCenter(Hero());Rebound=-1;Draw();}
 void Scale(bool Small,double Factor)
 {if(Small)MiniZoom=Math::Clamp(MiniZoom*Factor,1.0,4.0);else{Zoom=Math::Clamp(Zoom*Factor,FitZoom,FitZoom*6);Center=ClampCenter(Center);Rebound=-1;}Draw();}
 UFUNCTION() void Action(USvgCanvasNode Node)
 {
  auto A=UOpusUI::Action(Node);bool Small=Node.SourceId.Contains("mini");if(Panel!=nullptr)Panel.SourceActionHandled=true;
  if(A.Op=="open")Open();else if(A.Op=="close")Close();else if(A.Op=="fit")Fit();else if(A.Op=="locate")Locate();else if(A.Op=="in"||A.Op=="out")Scale(Small,A.Op=="in"?1.25:1/1.25);Draw();
 }
 UFUNCTION() bool FilterKey(FKey Key)
 {
  if(!Visible)return false;
  if(Key==EKeys::Escape||Key==EKeys::Tab){Close();Suppress=true;return true;}
  if(Key==EKeys::F){Locate();Suppress=true;return true;}
  if(Key==EKeys::MouseScrollUp||Key==EKeys::MouseScrollDown){Scale(false,Key==EKeys::MouseScrollUp?1.25:1/1.25);Suppress=true;return true;}return false;
 }
 bool HandleKeys(APlayerController PC)
 {
  if(Suppress){Suppress=false;return true;}
  if(!Visible){if(Mini!=nullptr&&Mini.IsVisible()){auto A=UOpusHoverUI::AnchorRect(Mini,PC);auto P=Pointer();if(P.X>=A.X&&P.X<=A.Z&&P.Y>=A.Y&&P.Y<=A.W){if(PC.WasInputKeyJustPressed(EKeys::MouseScrollUp)){Scale(true,1.25);return true;}if(PC.WasInputKeyJustPressed(EKeys::MouseScrollDown)){Scale(true,1/1.25);return true;}}}if(PC.WasInputKeyJustPressed(EKeys::Tab)&&!UOpusUI::BlocksCombat(Tower.Hud.Screen)){Open();return true;}return false;}
  if(PC.WasInputKeyJustPressed(EKeys::Escape)||PC.WasInputKeyJustPressed(EKeys::Tab)){Close();return true;}
  if(PC.WasInputKeyJustPressed(EKeys::F))Locate();if(PC.WasInputKeyJustPressed(EKeys::Zero)){Fit();Draw();}
  if(PC.WasInputKeyJustPressed(EKeys::MouseScrollUp))Scale(false,1.25);if(PC.WasInputKeyJustPressed(EKeys::MouseScrollDown))Scale(false,1/1.25);
  FVector2D Move((PC.IsInputKeyDown(EKeys::Right)?1:0)-(PC.IsInputKeyDown(EKeys::Left)?1:0),(PC.IsInputKeyDown(EKeys::Down)?1:0)-(PC.IsInputKeyDown(EKeys::Up)?1:0));if(!Move.IsNearlyZero()){Center=ClampCenter(Center+Move*8/Zoom);Rebound=-1;Draw();}return true;
 }
 FVector2D Pointer() const
 {auto PC=Gameplay::GetPlayerController(0);float32 X=0,Y=0;PC.GetMousePosition(X,Y);int W=0,H=0;PC.GetViewportSize(W,H);auto V=UOpusHoverUI::ViewSize(PC);return FVector2D(W>0?X*V.X/W:X,H>0?Y*V.Y/H:Y);}
 void PointerFrame(double Dt)
 {
  if(!Visible||Panel==nullptr)return;auto PC=Gameplay::GetPlayerController(0);auto N=Panel.FindSourceNode("dungeon-map-dmap-panel-viewport");if(N==nullptr)return;
  auto A=UOpusHoverUI::AnchorRect(N,PC);auto P=Pointer();bool Inside=P.X>=A.X&&P.X<=A.Z&&P.Y>=A.Y&&P.Y<=A.W,Down=UOpusMapUI::MouseDown();
  if(Down&&!Held&&Inside){Panning=true;Last=P;Rebound=-1;}
  if(Panning&&Down){double S=(A.Z-A.X)/724;auto Next=Center-(P-Last)/Math::Max(.01,S*Zoom);auto Bound=ClampCenter(Next);auto D=Next-Bound;Center=Bound+FVector2D(D.X/(1+Math::Abs(D.X)/25),D.Y/(1+Math::Abs(D.Y)/25));Last=P;Draw();}
  if(Panning&&!Down){Panning=false;Rebound=0;ReboundFrom=Center;ReboundTo=ClampCenter(Center);}
  Held=Down;if(Rebound>=0){Rebound+=Dt;double T=Math::Min(1.0,Rebound/.26),Ease=1-Math::Pow(1-T,3);Center=ReboundFrom+(ReboundTo-ReboundFrom)*Ease;if(T>=1)Rebound=-1;Draw();}
 }
 FOpusPlotPoint Marker(FVector P,FString Color,int Glyph,double Scale) const
 {FOpusPlotPoint M;M.Position=FVector2D(P.X,P.Y)/100;M.Radius=float32(5/Scale);M.Fill="#0a141d";M.Stroke=Color;M.Width=1.2;M.Glyph=Glyph;M.GlyphRadius=float32(5/Scale);return M;}
 void Paint(UOpusWorldScreen S,bool Small)
 {
  if(S==nullptr||!GeometryReady)return;double Z=Small?152.0/70*MiniZoom:Zoom;TArray<FOpusPlotPoint> Marks;TArray<FOpusPlotPath> Paths;TArray<FOpusPlotRect> Rects;
  for(auto C:Cells){FOpusPlotRect R;R.Rect=C;R.Fill="#263442";Rects.Add(R);}
  for(int I=0;I<Rooms.Num();I++){FOpusPlotRect R;R.Rect=Rooms[I];R.Fill=Visited.Contains(I)?"#346c65":"#485b70";Rects.Add(R);}
  Marks.Add(Marker(Tower.Dungeon.PlayerSpawnLocation,"#92b8d0",1,Z));
  for(auto E:Tower.Enemies)if(IsValid(E)&&E.Boss&&!E.Dead){Marks.Add(Marker(E.ActorLocation,"#ed9294",2,Z));break;}
  if(Tower.Studio.PortalReady)Marks.Add(Marker(Tower.Studio.PortalPosition,"#76e5cc",3,Z));
  auto Player=Marker(Tower.Hero.ActorLocation,"#ffe2a1",0,Z);Player.Angle=float32(Tower.Hero.ActorRotation.Yaw+90);Marks.Add(Player);
  FString Id=Small?"dungeon-minimap-dmap-mini-viewport":"dungeon-map-dmap-panel-viewport";
  UOpusMapUI::Plot(S,Id,Marks,Paths,Small?Hero():Center,float32(Z));UOpusMapUI::Rectangles(S,Id,Rects);
  UOpusUI::SetText(S,Small?"dmap-mini-zoom":"dmap-panel-zoom",""+int(Math::RoundToInt((Small?MiniZoom:Zoom/FitZoom)*100))+"%");
 }
 void Draw(){Paint(Mini,true);if(Visible)Paint(Panel,false);}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Tower==nullptr||Tower.Traveling)return;Geometry();auto PC=Gameplay::GetPlayerController(0);auto V=UOpusHoverUI::ViewSize(PC);
  if(Mini!=nullptr){UOpusHoverUI::Place(Mini,FVector2D(V.X-400,12),.65f);Mini.SetVisibility(!Tower.Paused&&!UOpusUI::BlocksCombat(Tower.Hud.Screen)?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);}
  PointerFrame(Dt);Poll-=Dt;if(Poll>0||!GeometryReady)return;Poll=.1;
  auto P=Hero();for(int I=0;I<Rooms.Num();I++){auto R=Rooms[I];if(P.X>=R.X&&P.X<=R.X+R.Z&&P.Y>=R.Y&&P.Y<=R.Y+R.W)Visited.AddUnique(I);}
  if(Mini!=nullptr)UOpusUI::SetText(Mini,"dmap-mini-floor","T"+Tower.Run.Floor+" · 本层");
  if(Visible){UOpusMapUI::Fit(Panel,PC);UOpusUI::SetText(Panel,"dmap-panel-subtitle","T"+Tower.Run.Floor+" · 当前地牢");UOpusUI::SetText(Panel,"dmap-panel-progress",""+Visited.Num()+" / "+Rooms.Num());UOpusUI::SetText(Panel,"dmap-panel-status",Tower.Studio.PortalReady?"首领已击败 · 传送阵已开启":"首领尚未击败");}
  
  Draw();
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){if(Mini!=nullptr)Mini.RemoveFromParent();if(Panel!=nullptr)Panel.RemoveFromParent();}
}
