// Studio3D quick-apply flow. UICreator owns all geometry and skins.
struct FDWOBuildMenuRecord
{
 FString Id,Name,GroupId,GroupName,GradeName,Mechanism,Description,Skill,Provenance;
 int Grade=1;
 TArray<FString> Skills,EquipmentIds,EquipmentNames;
}
class UDWOBuildMenu : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() UOpusWorldScreen Panel;
 UPROPERTY() UBackgroundBlur Backdrop;
 UPROPERTY() UCanvasPanelSlot BackdropSlot;
 UPROPERTY() TArray<UOpusWorldScreen> Rows;
 TArray<FDWOBuildMenuRecord> Catalogue;
 TArray<FString> FamilyIds;
 FString Family,Expanded,Feedback;
 bool Visible=false;
 void Initialize(UDWOSvgHUD H){Hud=H;Catalogue=DWOBuildCatalogue::Read();for(auto R:Catalogue)FamilyIds.AddUnique(R.GroupId);}
 UOpusWorldScreen Create(FString Id,bool Viewport=false)
 {
  auto W=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Id,"UICreatorBuilds",140,Viewport);
  if(W!=nullptr){W.OnSourceAction.AddUFunction(this,n"Action");W.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");}return W;
 }
 void Open()
 {
  if(Visible){Close();return;}
  if(Panel==nullptr)
  {
   Panel=Create("bd-menu",true);
  }
  if(Panel!=nullptr&&Backdrop==nullptr){auto Root=Panel.FindSourceNode("bd-menu");Backdrop=Cast<UBackgroundBlur>(NewObject(Panel,UBackgroundBlur));Backdrop.SetBlurStrength(DWOBuildCatalogue::BackdropBlur());Backdrop.SetApplyAlphaToBlur(false);Backdrop.SetCornerRadius(FVector4(12,12,12,12));Backdrop.SetVisibility(ESlateVisibility::HitTestInvisible);BackdropSlot=Root.AddChildToCanvas(Backdrop);BackdropSlot.SetPosition(FVector2D::ZeroVector);BackdropSlot.SetSize(FVector2D(300,506));BackdropSlot.SetZOrder(1);auto Content=Panel.FindSourceNode("bd-menu-content");if(Content!=nullptr){auto ContentSlot=Cast<UCanvasPanelSlot>(Content.Slot);if(ContentSlot!=nullptr)ContentSlot.SetZOrder(2);}}
  if(Panel==nullptr){Hud.Tower.LastError="Studio3D BD 资源尚未导入";return;}
  Hud.Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Hud.Tower.Rules)+1));Visible=true;Panel.SetVisibility(ESlateVisibility::SelfHitTestInvisible);Refresh();Place();
 }
 void Close(){Visible=false;if(Panel!=nullptr)Panel.SetVisibility(ESlateVisibility::Collapsed);}
 UFUNCTION() bool FilterKey(FKey Key){if(Visible&&Key==EKeys::Escape){Close();return true;}return false;}
 bool HandleKeys(APlayerController PC)
 {if(!Visible)return false;if(PC.WasInputKeyJustPressed(EKeys::Escape)){Close();return true;}if(PC.WasInputKeyJustPressed(EKeys::Tab))return true;if(PC.WasInputKeyJustPressed(EKeys::B)||PC.WasInputKeyJustPressed(EKeys::C)||PC.WasInputKeyJustPressed(EKeys::P)||PC.WasInputKeyJustPressed(EKeys::M))Close();return false;}
 int Find(FString Id) const{for(int I=0;I<Catalogue.Num();I++)if(Catalogue[I].Id==Id)return I;return -1;}
 int ItemState(FString Id,const TArray<FOpusEntry>& Bag) const{for(auto E:Bag)if(E.Kind=="equipment"&&E.Id==Id)return E.Equipped?2:1;return 0;}
 bool Equipped(FDWOBuildMenuRecord R,const TArray<FOpusEntry>& Bag) const
 {for(auto Id:R.EquipmentIds)if(ItemState(Id,Bag)!=2)return false;for(auto Id:R.Skills){bool Found=false;for(auto E:Bag)if(E.Kind=="skill"&&E.Id==Id&&E.Equipped)Found=true;if(!Found){bool Granted=false;for(auto E:UOpusData::Skills(Hud.Tower.Rules))if(E.Id==Id&&E.Equipped)Granted=true;if(!Granted)return false;}}return true;}
 UFUNCTION() void Action(USvgCanvasNode N)
 {
  auto A=UOpusUI::Action(N);if(A.Type!="build-menu"&&A.Type!="opus")return;Panel.SourceActionHandled=true;
  if(A.Op=="close"){Close();return;}
  if(A.Op=="back"){Family="";Expanded="";}
  if(A.Op=="family"){Family=A.Value;Expanded="";}
  if(A.Op=="details")Expanded=Expanded==A.Value?"":A.Value;
  if(A.Op=="guide"){Close();Hud.Screen.ApplySceneState("builds");return;}
  if(A.Op=="apply")
  {
   int I=Find(A.Value);if(I<0)return;FString Error;
   if(UOpusNative::ApplyPreset(Hud.Tower.Rules,FName(A.Value),Error)){Hud.Tower.Rules.RespawnPlayer();Hud.Tower.SaveProfile();Feedback="";Hud.RefreshIn=0;Hud.RefreshPart=0;}else Feedback=Error;
   Hud.Tower.LastError=Error;
  }
  Refresh();
 }
 void Detail(FDWOBuildMenuRecord R,const TArray<FOpusEntry>& Bag)
 {
  auto W=Create("bd-detail");if(W==nullptr)return;
  UOpusUI::SetText(W,"bd-description",R.Description);float Y=UOpusTalentBridge::Measure(W,"bd-description",R.Description,244);
  UOpusTalentBridge::Rect(W,"bd-description",FVector4(10,0,244,Y));Y+=6;
  UOpusUI::SetShown(W,"bd-provenance",!R.Provenance.IsEmpty());
  if(!R.Provenance.IsEmpty()){float H=UOpusTalentBridge::Measure(W,"bd-provenance",R.Provenance,244);UOpusTalentBridge::Rect(W,"bd-provenance",FVector4(10,Y,244,H));UOpusUI::SetText(W,"bd-provenance",R.Provenance);Y+=H+6;}
  for(int I=0;I<10;I++)
  {
   bool Show=R.EquipmentIds.IsValidIndex(I);UOpusUI::SetShown(W,"bd-equipment-"+I,Show);UOpusUI::SetShown(W,"bd-owned-"+I,Show);if(!Show)continue;
   int State=ItemState(R.EquipmentIds[I],Bag);UOpusUI::SetText(W,"bd-equipment-"+I,R.EquipmentNames[I]);UOpusUI::SetText(W,"bd-owned-"+I,State==2?"已装备":State==1?"已拥有":"未拥有");
   UOpusTalentBridge::Rect(W,"bd-equipment-"+I,FVector4(10,Y,195,20));UOpusTalentBridge::Rect(W,"bd-owned-"+I,FVector4(205,Y,49,20));Y+=20;
  }
  UOpusTalentBridge::Rect(W,"bd-detail",FVector4(0,0,264,Y+9));Rows.Add(W);
 }
 void Refresh()
 {
  if(Panel==nullptr)return;auto Bag=UOpusData::Inventory(Hud.Tower.Rules);Rows.Empty();
  if(!Family.IsEmpty())
  {
   auto Head=Create("bd-subhead");if(Head==nullptr)return;for(auto R:Catalogue)if(R.GroupId==Family){UOpusUI::SetText(Head,"bd-family-title",R.GroupName);break;}Rows.Add(Head);
   TArray<int> Order;for(int I=0;I<Catalogue.Num();I++)if(Catalogue[I].GroupId==Family)Order.Add(I);
   for(int I=0;I<Order.Num();I++)for(int J=I+1;J<Order.Num();J++)if(Catalogue[Order[J]].Grade<Catalogue[Order[I]].Grade){int Save=Order[I];Order[I]=Order[J];Order[J]=Save;}
   for(int I:Order)
   {
    auto R=Catalogue[I];auto W=Create("bd-grade-row");if(W==nullptr)return;
    UOpusUI::SetText(W,"bd-grade-icon",R.Name.Left(1));UOpusUI::SetText(W,"bd-grade-name",R.GradeName);UOpusUI::SetText(W,"bd-grade-meta",R.Mechanism);UOpusUI::SetText(W,"bd-grade-state",Equipped(R,Bag)?"已应用":"一键应用");UOpusUI::SetText(W,"bd-details-text",Expanded==R.Id?"−":"+");
    UOpusUI::SetAction(W,"bd-apply","apply",R.Id);UOpusUI::SetAction(W,"bd-details","details",R.Id);UOpusWorldUI::Emphasize(W,"bd-apply",Equipped(R,Bag));Rows.Add(W);
    if(Expanded==R.Id)Detail(R,Bag);
   }
  }
  else for(auto Id:FamilyIds)
  {
   auto W=Create("bd-family-row");if(W==nullptr)return;int Count=0,First=-1;bool Used=false;
   for(int I=0;I<Catalogue.Num();I++)if(Catalogue[I].GroupId==Id){Count++;if(First<0)First=I;Used=Used||Equipped(Catalogue[I],Bag);}
   auto R=Catalogue[First];UOpusUI::SetText(W,"bd-family-icon",R.GroupName.Left(1));UOpusUI::SetText(W,"bd-family-name",R.GroupName);UOpusUI::SetText(W,"bd-family-meta",Count>1?""+Count+" 档强度":"默认配装");UOpusUI::SetText(W,"bd-family-state",Used?"已应用 · ›":"›");UOpusUI::SetAction(W,"bd-family-row","family",Id);UOpusWorldUI::Emphasize(W,"bd-family-row",Used);Rows.Add(W);
  }
  UOpusWorldUI::MountRows(Panel,"bd-rows",Rows);for(auto Row:Rows){auto DetailRoot=Row.FindSourceNode("bd-detail");auto Box=Cast<USizeBox>(Row.GetParent());if(DetailRoot!=nullptr&&Box!=nullptr)Box.SetHeightOverride(DetailRoot.DesignRect.W);}UOpusUI::SetText(Panel,"bd-count",""+FamilyIds.Num());UOpusUI::SetText(Panel,"bd-guide-text",Feedback.IsEmpty()?"构筑原理与词条追求  ▤ ›":Feedback);
 }
 void Place()
 {
  if(!Visible||Panel==nullptr)return;auto PC=Gameplay::GetPlayerController(0);auto Size=UOpusHoverUI::ViewSize(PC);auto Anchor=Hud.Screen.FindSourceNode("hud-screen-instance::rs-map-instance");auto R=UOpusHoverUI::AnchorRect(Anchor,PC);float Scale=1;
  float H=Math::Min(506.0,Size.Y-180);H=Math::Max(H,220.0);UOpusTalentBridge::Rect(Panel,"bd-menu",FVector4(0,0,300,H));UOpusTalentBridge::Rect(Panel,"bd-menu-content",FVector4(0,0,300,H));UOpusTalentBridge::Rect(Panel,"bd-rows",FVector4(13,45,274,H-86));UOpusTalentBridge::Rect(Panel,"bd-guide",FVector4(13,H-41,274,30));Panel.SetDesiredSizeInViewport(FVector2D(300,H));
  if(BackdropSlot!=nullptr){BackdropSlot.SetSize(FVector2D(300,H));BackdropSlot.SetZOrder(1);}auto Content=Panel.FindSourceNode("bd-menu-content");if(Content!=nullptr){auto ContentSlot=Cast<UCanvasPanelSlot>(Content.Slot);if(ContentSlot!=nullptr)ContentSlot.SetZOrder(2);}
  float X=R.Z>R.X?R.X-312:Size.X-600;UOpusHoverUI::Place(Panel,FVector2D(Math::Clamp(X,12.0,Math::Max(12.0,Size.X-312)),64),Scale);
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt){if(!Visible)return;if(UOpusUI::BlocksCombat(Hud.Screen)||(Hud.Talents!=nullptr&&Hud.Talents.Visible)||(Hud.MapDevice!=nullptr&&Hud.MapDevice.Visible)||(Hud.Tower.Recent!=nullptr&&Hud.Tower.Recent.UI!=nullptr&&Hud.Tower.Recent.UI.Panel!=nullptr)){Close();return;}Place();}
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();}Rows.Empty();}
}
