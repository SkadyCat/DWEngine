class UDWOTalentTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UDWOTalentConstellation Talents;
 bool Ok=true;
 double Clock=0;
 int Stage=0,First=-1;FString VisualState;
 void Check(bool Value,FString Name){UOpusNative::WriteLog((Value?"TALENT_TEST_PASS ":"TALENT_TEST_FAIL ")+Name);if(!Value)Ok=false;}
 void Initialize(ADWOTowerGameMode InTower)
 {
  Tower=InTower;Talents=Tower.Hud.Talents;if(Talents==nullptr){Check(false,"HostExists");UOpusNative::Quit(false);return;}
  if(UOpusNative::Flag("DWOTalentLoadTest"))return;
  if(UOpusNative::Flag("DWOTalentZoomProof")){Check(Talents.Nodes.Num()==1000,"ZoomCatalog1000");return;}
  Check(Talents.Nodes.Num()==1000&&Talents.Attributes.Num()==4,"FrozenCatalog1000AndFourAttributes");
  int Legends=0,Edges=0;for(auto N:Talents.Nodes){if(N.Kind=="legendary")Legends++;Edges+=N.Neighbors.Num();}Check(Legends==100&&Edges==2226,"HundredLegends1113Edges");
  int Root=Talents.Index("origin");for(int I:Talents.Nodes[Root].Neighbors)if(Talents.Nodes[I].Kind=="attribute"){First=I;break;}
  if(First<0){Check(false,"RootAttribute");UOpusNative::Quit(false);return;}
  if(UDWOTowerJourney::Get().Visit>1){auto Carry=UDWOTowerJourney::Get().Carry;auto Pools=Tower.Rules.GetPlayerPools();Check(Talents.Choices[First]==3&&UOpusTalentBridge::Stat(Tower.Rules,n"attr.constitution")==5,"CrossFloorRestoresTalentBeforePools");Check(Math::Abs(Pools.Life-Carry.Life)<.001&&Math::Abs(Pools.MaxLife-Carry.MaxLife)<.001,"CrossFloorKeepsFullTalentLife");Check(Ok,"TravelComplete");UOpusNative::Quit(Ok);Stage=99;return;}
  for(int A=0;A<4;A++){FName Key=FName("attr."+Talents.Attributes[A].Id);double Base=UOpusTalentBridge::Stat(Tower.Rules,Key);Check(Talents.Attributes[A].Value==5&&Talents.Choose(First,A)&&UOpusTalentBridge::Stat(Tower.Rules,Key)==Base+5,"ActualFive_"+Talents.Attributes[A].Id);Check(Talents.Refund(First)&&UOpusTalentBridge::Stat(Tower.Rules,Key)==Base,"RefundFive_"+Talents.Attributes[A].Id);}
  double Before=UOpusTalentBridge::Stat(Tower.Rules,n"attr.intelligence");
  Check(Talents.Choose(First,2)&&Talents.Spent()==1,"SingleClickConsumesOnePoint");
  Check(UOpusTalentBridge::Stat(Tower.Rules,n"attr.intelligence")==Before+5,"NativeRulesIntelligencePlus5");
  Check(Talents.Choose(First,0)&&Talents.Spent()==1&&UOpusTalentBridge::Stat(Tower.Rules,n"attr.intelligence")==Before,"FreeAttributeReplacementNoDuplicate");
  FString Saved=Talents.Serialize();FString Exported=UOpusTalentBridge::PlanJson(Saved,true);Check(Exported.Contains("\"schema\"")&&Talents.Restore(UOpusTalentBridge::PlanJson(Exported,false))&&Talents.Serialize()==Saved,"WebCompatibleJsonRoundTrip");auto Previous=Talents.Choices;
  Check(!Talents.Restore("opus_constellation|3|120\nunknown|0\n")&&Talents.Serialize()==Saved,"InvalidSaveIsTransactional");
  int Unsupported=0;Talents.Budget=999;for(int I=0;I<Talents.Nodes.Num();I++)if(Talents.Nodes[I].Kind!="origin"){if(Talents.Unsupported[I].IsEmpty())Talents.Choices[I]=0;else {Unsupported++;UOpusNative::WriteLog("TALENT_UNSUPPORTED "+Talents.Nodes[I].Id+" "+Talents.Unsupported[I]);}}
  FString Error;UOpusNative::WriteLog("TALENT_KNOWN_GAPS count="+Unsupported);Check(Talents.Valid()&&Talents.Apply(Error),"All999SupportedPoints100LegendsCompileNative");if(!Error.IsEmpty())UOpusNative::WriteLog("TALENT_COMPILE_DETAIL "+Error);
  Talents.Choices=Previous;Talents.Budget=120;Talents.Apply(Error);Check(Talents.Restore(Saved),"RestoreAllocation");
  FString Profile=Tower.Rules.ExportPlayerState();Check(Tower.Rules.ImportPlayerState(Profile,Error)&&Talents.Apply(Error),"InventoryProfileRetainsHostParts");
  int Distant=-1;for(int I=0;I<Talents.Nodes.Num();I++)if(Talents.Nodes[I].Kind=="attribute"&&!Talents.CanReach(I)){Distant=I;break;}
  Check(Distant>=0&&!Talents.Choose(Distant,2)&&Talents.Spent()==1,"UnreachableSpendsNothing");
  int Child=-1;for(int I:Talents.Nodes[First].Neighbors)if(Talents.Nodes[I].Kind=="attribute"&&!Talents.Lit(I)){Child=I;break;}
  if(Child>=0){Check(Talents.Choose(Child,3),"ConnectedChild");Check(!Talents.Refund(First)&&Talents.Spent()==2,"CannotDisconnectDescendants");Check(Talents.Refund(Child),"RefundLeaf");}
  Check(Talents.Refund(First)&&Talents.Spent()==0,"RefundRestoresNativeValues");
 }
 UButton InputButton(USvgCanvasNode N){if(N!=nullptr)for(auto W:N.GetAllChildren()){auto B=Cast<UButton>(W);if(B!=nullptr)return B;}return nullptr;}
 bool RasterHidden(USvgCanvasNode N){if(N!=nullptr)for(auto W:N.GetAllChildren()){auto I=Cast<UImage>(W);if(I!=nullptr)return I.GetVisibility()==ESlateVisibility::Collapsed;}return false;}
 FString LiveText(FString Id){auto N=Talents.Screen.FindSourceNode(Id);if(N!=nullptr)for(auto W:N.GetAllChildren()){auto Fit=Cast<UScaleBox>(W);if(Fit!=nullptr){auto T=Cast<UTextBlock>(Fit.GetChildAt(0));if(T!=nullptr)return T.GetText().ToString();}}return "";}
 void TestUI()
 {
  Talents.Open();Check(Talents.Visible&&Talents.Screen!=nullptr,"SavedUICreatorScreenOpens");
  if(Talents.Screen==nullptr){UOpusNative::Quit(false);return;}
  for(auto N:Talents.Nodes)if(N.Kind=="legendary")Check(Talents.Screen.FindSourceNode("legend-sample-"+N.Id)!=nullptr,"LegendRaster_"+N.Id);
  int Distant=-1;for(int I=0;I<Talents.Nodes.Num();I++)if(Talents.Nodes[I].Kind=="attribute"&&!Talents.CanReach(I)){Distant=I;break;}
  TArray<FString> Controls;Controls.Add("talent-search");Controls.Add("talent-cat-all");Controls.Add("talent-cat-attack");Controls.Add("talent-cat-defense");Controls.Add("talent-cat-special");Controls.Add("talent-sector");Controls.Add("talent-selected-list");Controls.Add("talent-preview-skill");Controls.Add("talent-export");Controls.Add("talent-import");Controls.Add("talent-attribute-tip");Controls.Add("talent-legend-tip");for(FString Id:Controls)Check(Talents.Screen.FindSourceNode(Id)!=nullptr,"OriginalHTMLControl_"+Id);
  int VectorGlyphs=0;if(RasterHidden(Talents.Screen.FindSourceNode("star-origin")))VectorGlyphs++;for(auto N:Talents.Nodes)if(N.Kind=="legendary"&&RasterHidden(Talents.Screen.FindSourceNode("crown-"+N.Id)))VectorGlyphs++;Check(VectorGlyphs==101,"OriginAndHundredCrownsUseVectorSurfaces");
  double MaxCenterError=0;for(auto N:Talents.Nodes){auto P=Talents.Screen.FindSourceNode("point-"+N.Id);double R=N.Kind=="origin"?31.0:N.Kind=="attribute"?9.5:N.SourceBuild.IsEmpty()?22.0:26.0;auto C=FVector2D(P.DesignRect.X+(R+6)*.182078666319354,P.DesignRect.Y+(N.Kind=="legendary"?36.0:R+6)*.182078666319354);auto Expected=FVector2D(641,503.5)+(N.Position-FVector2D(3,18))*.182078666319354;MaxCenterError=Math::Max(MaxCenterError,(C-Expected).Size());}Check(MaxCenterError<.002,"ThousandVisualCentersMatchCurveCoordinates");
  Check(Talents.Screen.SourceDocument.Rasters.Num()==0,"RuntimeAvoidsFullEditorRasterDocument");
  bool SharedIcons=true;for(auto N:Talents.Nodes)if(N.Kind=="attribute"){auto Icon=Talents.Screen.FindSourceNode("icon-"+N.Id);SharedIcons=SharedIcons&&Icon.Normal.Texture==Talents.Screen.FindSourceNode("attr-sample-intelligence").Normal.Texture;}Check(SharedIcons,"UnallocatedAttributeIconsReuseLibraryTexture");
  Talents.Query="护盾";Talents.RefreshSearch();Check(Talents.ResultRows.Num()>=10,"SearchUsesActualLegendCatalog");Talents.Query="";Talents.RefreshSearch();
  Talents.Select(Distant);Check(Talents.Feedback=="无法到达"&&!Talents.Pinned,"UnreachableShowsOnlyTip");
  Talents.Select(First);Check(Talents.Pinned&&Talents.Selected==First,"FourIconSelector");
  for(auto A:Talents.Attributes){auto B=Talents.Screen.FindSourceNode("talent-choose-"+A.Id);Check(B!=nullptr&&InputButton(B)!=nullptr&&B.Hovered.Texture!=nullptr&&B.Hovered.Texture!=B.Normal.Texture,"AuthoredHoverBrush_"+A.Id);if(B!=nullptr&&InputButton(B)!=nullptr){InputButton(B).OnHovered.Broadcast();Check(LiveText("talent-choice-tip-text")==A.Name+" +5"&&Talents.Screen.FindSourceNode("talent-choice-tip").GetVisibility()!=ESlateVisibility::Collapsed,"NativeHoverTip_"+A.Id);InputButton(B).OnUnhovered.Broadcast();Check(Talents.Screen.FindSourceNode("talent-choice-tip").GetVisibility()==ESlateVisibility::Collapsed,"NativeHoverLeave_"+A.Id);}}
  auto Button=Talents.Screen.FindSourceNode("talent-choose-intelligence");if(Button!=nullptr)Talents.Screen.OnSourceAction.Broadcast(Button);
  Check(Talents.Choices[First]==2&&!Talents.Pinned&&UOpusTalentBridge::Stat(Tower.Rules,n"attr.intelligence")==5,"AuthoredButtonUpdatesPNGAndNativeRules");
  Talents.Select(First);InputButton(Talents.Screen.FindSourceNode("talent-choose-dexterity")).OnHovered.Broadcast();UOpusNative::WriteLog("TALENT_TEST_STAGE visual-ready");
 }
 void Capture(FString Name){if(UOpusNative::Flag("DWOTalentVisual"))System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/Talents/"+Name+".png -nosuffix");}
 void CurveTick()
 {
  if(Stage==0){Talents.Open();Talents.Zoom=8;Talents.Pan=-(Talents.Nodes[Talents.Index("poison_crit_detonate")].Position-FVector2D(3,18))*.182078666319354*Talents.Zoom;Talents.Transform();Stage=1;Clock=0;return;}
  if(Stage==1){Capture("LoadCurves/native-poison-curves");Stage=2;Clock=0;return;}
  if(Stage==2){FString Fixture="opus_constellation|3|120\nembers_hub|2\nblades_cross_1|2\nblades_cross_2|1\nshield_arm_1_5|3\nshield_arm_1_6|2\nshield_arm_1_7|2\noverflow_cast|0\nsurvival_cross_2|3\nsurvival_cross_3|1\n";FString Error;Check(Talents.Restore(Fixture)&&Talents.Apply(Error),"CurveProofAllocation");Talents.Refresh();Talents.HideDetails();Talents.Zoom=8;Talents.Pan=FVector2D::ZeroVector;Talents.Transform();Stage=3;Clock=0;return;}
  if(Stage==3){Capture("LoadCurves/native-origin-curves");Stage=4;Clock=0;return;}
  if(Stage==4){Talents.Zoom=14;Talents.Pan=-(Talents.Nodes[Talents.Index("poison_no_damage")].Position-FVector2D(3,18))*.182078666319354*Talents.Zoom;Talents.Transform();Stage=5;Clock=0;return;}
  if(Stage==5){Capture("LoadCurves/native-legend-curves");Stage=6;Clock=0;return;}
  if(Stage==6){Check(Talents.Screen!=nullptr&&Talents.Visible,"BezierScreenshotsComplete");UOpusNative::Quit(Ok);Stage=99;}
 }
 void LoadTick()
 {
  if(Stage==0||Stage==2||Stage==4){FString Phase=Stage==0?"cold":Stage==2?"warm1":"warm2";if(Stage>0)Talents.Close();UOpusNative::WriteLog("TALENT_PERF_BEGIN phase="+Phase);Talents.Open();UOpusNative::WriteLog("TALENT_PERF_READY phase="+Phase);Stage++;return;}
  if(Stage==1||Stage==3||Stage==5){FString Phase=Stage==1?"cold":Stage==3?"warm1":"warm2";UOpusNative::WriteLog("TALENT_PERF_PRESENTED phase="+Phase);if(Stage==1)Capture("LoadCurves/native-cold-loaded");Stage++;return;}
  if(Stage==6){Talents.Close();Check(Talents.Screen!=nullptr&&!Talents.Visible,"LoadAndReopenComplete");UOpusNative::Quit(Ok);Stage=99;}
 }
 void PolishTick()
 {
  if(Stage==0){Talents.Open();Talents.HideDetails();Talents.Zoom=5.63;Talents.Pan=FVector2D(-180,-60);Talents.Transform();Stage=1;Clock=0;return;}
  if(Stage==1){Talents.Select(First);InputButton(Talents.Screen.FindSourceNode("talent-choose-dexterity")).OnHovered.Broadcast();Stage=2;Clock=0;return;}
  if(Stage==2){Capture("AttributePolish/native-hover-563");Stage=3;Clock=0;return;}
  if(Stage==3){Talents.HideDetails();Talents.Zoom=14;Talents.Pan=FVector2D::ZeroVector;Talents.Transform();Stage=4;Clock=0;return;}
  if(Stage==4){Capture("AttributePolish/native-origin-1400");Stage=5;Clock=0;return;}
  if(Stage==5){Talents.Pan=-(Talents.Nodes[Talents.Index("overflow_cast")].Position-FVector2D(3,18))*.182078666319354*Talents.Zoom;Talents.Transform();Stage=6;Clock=0;return;}
  if(Stage==6){Capture("AttributePolish/native-legend-1400");Stage=7;Clock=0;return;}
  if(Stage==7){Check(Talents.Screen!=nullptr&&Talents.Visible,"AttributePolishScreenshotsComplete");UOpusNative::Quit(Ok);Stage=99;}
 }
 void ZoomTick()
 {
  FString Prefix=UOpusNative::Flag("DWOTalentZoomBefore")?"zoom-before-":"zoom-after-";
  if(Stage==0){Talents.Open();Talents.Zoom=5.63;Talents.Pan=-(Talents.Nodes[Talents.Index("poison_eternal")].Position-FVector2D(3,18))*.182078666319354*Talents.Zoom;Talents.Transform();Stage=1;Clock=0;return;}
  if(Stage==1){Capture(Prefix+"563");Stage=2;Clock=0;return;}
  if(Stage==2){Talents.Zoom=14;Talents.Pan=-(Talents.Nodes[Talents.Index("poison_eternal")].Position-FVector2D(3,18))*.182078666319354*Talents.Zoom;Talents.Transform();Stage=3;Clock=0;return;}
  if(Stage==3){Capture(Prefix+"1400");Stage=4;Clock=0;return;}
  if(Stage==4){Talents.Zoom=1;Talents.Pan=FVector2D::ZeroVector;Talents.Transform();Stage=5;Clock=0;return;}
  if(Stage==5){Capture(Prefix+"100");Stage=6;Clock=0;return;}
  if(Stage==6){Check(Talents.Screen!=nullptr&&Talents.Visible,"ZoomScreenshotsComplete");UOpusNative::Quit(Ok);Stage=99;}
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  Clock+=DeltaSeconds;if(Talents==nullptr||Clock<1||Stage==99)return;
  if(UOpusNative::Flag("DWOTalentCurveProof")){CurveTick();return;}
  if(UOpusNative::Flag("DWOTalentLoadTest")){LoadTick();return;}
  if(UOpusNative::Flag("DWOTalentPolishProof")){PolishTick();return;}
  if(UOpusNative::Flag("DWOTalentZoomProof")){ZoomTick();return;}
  if(Stage==0){Talents.Open();Stage=10;Clock=0;return;}
  if(Stage==10){Capture("native-default");Stage=11;Clock=0;return;}
  if(Stage==11){TestUI();Stage=1;Clock=0;return;}
  if(Talents.Screen==nullptr){UOpusNative::Quit(false);return;}
  if(Stage==1){Capture("native-attribute-selector");Stage=2;Clock=0;return;}
  if(Stage==2){VisualState=Talents.Serialize();FString Fixture="opus_constellation|3|120\nembers_hub|2\nblades_cross_1|2\nblades_cross_2|1\nshield_arm_1_5|3\nshield_arm_1_6|2\nshield_arm_1_7|2\noverflow_cast|0\nsurvival_cross_2|3\nsurvival_cross_3|1\n";FString Error;Check(Talents.Restore(Fixture)&&Talents.Apply(Error),"OriginalHTMLNinePointFixtureCompiles");Talents.Refresh();Talents.HideDetails();Talents.Zoom=1;Talents.Pan=FVector2D::ZeroVector;Talents.Transform();Stage=3;Clock=0;return;}
  if(Stage==3){Capture("native-overview");Stage=13;Clock=0;return;}
  if(Stage==13){Talents.HideDetails();int Longest=-1;for(int I=0;I<Talents.Nodes.Num();I++)if(Talents.Nodes[I].Kind=="legendary"&&(Longest<0||Talents.Nodes[I].Description.Len()>Talents.Nodes[Longest].Description.Len()))Longest=I;Talents.Hover(Longest);Check(Talents.Hovered==Longest&&!Talents.Pinned&&Talents.Screen.FindSourceNode("talent-legend-tip").GetVisibility()==ESlateVisibility::Visible,"LongLegendHoverUsesAuthoredTip");Stage=14;Clock=0;return;}
  if(Stage==14){Capture("native-legend-tip");Stage=16;Clock=0;return;}
  if(Stage==16){Talents.HideDetails();auto Point=Talents.Screen.FindSourceNode("point-overflow_cast");Talents.Screen.OnSourceAction.Broadcast(Point);Check(Talents.Pinned&&Talents.Selected==Talents.Index("overflow_cast")&&Talents.Screen.FindSourceNode("talent-card").GetVisibility()==ESlateVisibility::Visible,"LegendClickOpensAuthoredDetails");Stage=15;Clock=0;return;}
  if(Stage==15){Capture("native-legend-card");Stage=4;Clock=0;return;}
  if(Stage==4){FString Error;Check(Talents.Restore(VisualState)&&Talents.Apply(Error),"RestoreAfterReferenceFixture");Talents.Refresh();Check(Talents.FilterKey(EKeys::Escape)&&Talents.Visible&&!Talents.Pinned,"EscapeClosesLegendDetailsFirst");Check(Talents.FilterKey(EKeys::Escape)&&!Talents.Visible,"EscapeRestoresBattle");Stage=5;Clock=0;return;}
  if(Stage==5){Check(!Tower.Paused,"ClosingRestoresPreviousPause");Talents.Open();Check(Talents.Choices[First]==2,"ReopenKeepsAllocation");Check(Ok,"Complete");if(Ok&&UOpusNative::Flag("DWOTalentTravelTest")){Talents.Choose(First,3);Tower.Rules.RespawnPlayer();Talents.Close();Stage=99;Tower.Travel(true);}else {UOpusNative::Quit(Ok);Stage=6;}}
 }
}
