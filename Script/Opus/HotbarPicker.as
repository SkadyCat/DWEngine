// Studio's compact icon palette. UICreator owns every visible control and brush.
struct FDWOHotbarChoice
{
 FString Id,Name,Kind,Icon,Description,Slot;
 int Count=0;
 bool Equipped=false;
}
class UDWOHotbarPicker : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UOpusWorldScreen Panel;
 UPROPERTY() UOpusWorldScreen Tip;
 UPROPERTY() TArray<UOpusWorldScreen> QuickSlots;
 TArray<FDWOHotbarChoice> Choices;
 FString Mode;
 int Slot=0,Page=0,RowCount=1,FocusedChoice=0;
 bool ConsumeShortcut=false;
 double Poll=0;
 FVector2D PalettePosition;
 void Initialize(UDWOSvgHUD H){Hud=H;Tower=H.Tower;}
 UOpusWorldScreen Create(FString Id,int Layer=120)
 {
  auto W=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Id,"UICreatorHotbar",Layer);
  if(W==nullptr){Tower.LastError="缺少 UICreator HUD 选择栏："+Id;return nullptr;}
  W.OnSourceAction.AddUFunction(this,n"Action");W.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");return W;
 }
 FString Current() const
 {
  if(Mode=="skill"){for(auto E:Choices)if(E.Equipped&&E.Slot==""+Slot)return E.Id;return "";}
  return !Tower.Recent.AuraSlots[Slot].IsNone()?Tower.Recent.AuraSlots[Slot].ToString():UOpusNative::Belt(Tower.Rules,Slot).ToString().Replace("None","");
 }
 void ReadChoices()
 {
  Choices.Empty();
  if(Mode=="skill"){for(auto Raw:UOpusData::Skills(Tower.Rules))Choices.Add(Entry(Raw));return;}
  Tower.Recent.SyncAuras();
  for(auto C:Tower.Recent.Choices){FDWOHotbarChoice E;E.Id=C.Id.ToString();E.Name=C.Name;E.Kind="aura";E.Icon=UOpusGameplayStore::SkillIcon(C.Id);E.Description=C.Description+"\n魔力封印 "+int(C.Reservation)+"% · 装备授予";Choices.Add(E);}
  for(auto E:UOpusData::Inventory(Tower.Rules))if(E.Kind=="item"&&E.Category=="consumable"&&E.Count>0)Choices.Add(Entry(E));
 }
 FDWOHotbarChoice Entry(FOpusEntry Raw){FDWOHotbarChoice E;E.Id=Raw.Id;E.Name=Raw.Name;E.Kind=Raw.Kind;E.Icon=Raw.Icon;E.Description=Raw.Description;E.Slot=Raw.Slot;E.Equipped=Raw.Equipped;E.Count=Raw.Count;return E;}
 bool Open(FString InMode,int Index)
 {
  if(Index<0||Index>=4||Tower.Traveling||Tower.Rules.GetPlayerPools().bDead)return false;
  if(Panel!=nullptr&&Mode==InMode&&Slot==Index){Close();return true;}
  Close();if(Tower.Recent.UI.Panel!=nullptr)Tower.Recent.UI.Close();
  Mode=InMode;Slot=Index;Page=0;
  FString Error;if(Mode=="skill")Tower.Rules.SelectPlayerSkill(Slot,Error);
  Hud.Screen.ApplySceneState("selected-"+(UOpusNative::Selected(Tower.Rules)+1));Tower.Rules.SetPlayerCasting(false);
  ReadChoices();FString Assigned=Current();for(int I=0;I<Choices.Num();I++)if(Choices[I].Id==Assigned)Page=I/20;
  return Refresh();
 }
 bool Refresh()
 {
  if(Panel!=nullptr)Panel.RemoveFromParent();Panel=nullptr;CloseTip();
  int Count=Math::Min(20,Math::Max(0,Choices.Num()-Page*20));RowCount=Math::Clamp((Count+4)/5,1,4);
  Panel=Create("hotbar-picker-"+RowCount);if(Panel==nullptr)return false;
  FString Assigned=Current();UOpusUI::SetEnabled(Panel,"picker-clear",!Assigned.IsEmpty());
  UOpusUI::SetEnabled(Panel,"picker-prev",Page>0);UOpusUI::SetEnabled(Panel,"picker-next",(Page+1)*20<Choices.Num());
  UOpusUI::SetShown(Panel,"picker-prev",Choices.Num()>20);UOpusUI::SetShown(Panel,"picker-next",Choices.Num()>20);
  UOpusUI::SetText(Panel,"picker-page",Choices.Num()>20?""+(Page+1)+" / "+((Choices.Num()+19)/20):"");
  for(int I=0;I<RowCount*5;I++)
  {
   int At=Page*20+I;bool Found=Choices.IsValidIndex(At);FString B="picker-cell-"+I;
   UOpusUI::SetEnabled(Panel,B,Found);UOpusUI::SetAction(Panel,B,"choose",""+At);
   UOpusUI::SetImage(Panel,"picker-icon-"+I,Found?Choices[At].Icon:"");
   UOpusUI::SetShown(Panel,"picker-icon-"+I,Found);UOpusUI::SetShown(Panel,"picker-check-"+I,Found&&Choices[At].Id==Assigned);
   UOpusWorldUI::SetHover(Panel,B,""+At);
  }
  auto Hover=UOpusUI::HoverEvents(Panel);Hover.Entered.AddUFunction(this,n"Enter");Hover.Left.AddUFunction(this,n"Leave");Place();
  int Focus=0;for(int I=0;I<Count;I++)if(Choices[Page*20+I].Id==Assigned)Focus=I;FocusChoice(Focus);return true;
 }
 bool Choose(FString Id)
 {
  FString Requested=Id;
  if(Panel==nullptr)return false;if(Mode=="quick"&&!Requested.IsEmpty()&&Requested==Current()){Close();return true;}ReadChoices();FDWOHotbarChoice Pick;bool Found=Requested.IsEmpty();for(auto E:Choices)if(E.Id==Requested){Pick=E;Found=true;}if(!Found)return false;
  FString Error;bool Ok=false;
  if(Mode=="skill")Ok=Tower.Rules.ChangePlayerSkill(Slot,FName(Requested),Error);
  else Ok=Tower.Rules.SetPlayerBeltSlot(Slot,Pick.Kind=="aura"?FName():FName(Requested),Error);
  // A native profile edit rebuilds the loadout; restore existing enabled auras afterwards.
  if(Ok){Tower.Recent.Applied="";if(Mode=="quick")Ok=Tower.Recent.EquipAura(Slot,Pick.Kind=="aura"?FName(Requested):FName());else Tower.Recent.SyncAuras();}
  Tower.LastError=Error;if(!Ok)return false;Tower.SaveProfile();Hud.RefreshIn=0;Hud.RefreshPart=0;Close();RefreshQuick();return true;
 }
 void CloseTip(){if(Tip!=nullptr){Tip.RemoveFromParent();Tip=nullptr;}}
 void Close(){CloseTip();if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();Panel=nullptr;if(Hud.Screen!=nullptr)Hud.Screen.SetKeyboardFocus();}}
 UButton Input(UWidget W){auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Input(C);if(Found!=nullptr)return Found;}return nullptr;}
 void FocusChoice(int Index){if(Panel==nullptr)return;int Count=Math::Min(20,Math::Max(0,Choices.Num()-Page*20));if(Count<1)return;FocusedChoice=(Index%Count+Count)%Count;auto B=Input(Panel.FindSourceNode("r"+RowCount+"-cell-"+FocusedChoice));if(B!=nullptr)B.SetKeyboardFocus();}
 UFUNCTION() void Action(USvgCanvasNode N)
 {
  auto A=UOpusUI::Action(N);
  if(A.Op=="quick"){Open("quick",String::Conv_StringToInt(A.Value));return;}
  if(A.Op=="close"){Close();return;}if(A.Op=="clear"){Choose("");return;}
  if(A.Op=="page"){Page=Math::Clamp(Page+String::Conv_StringToInt(A.Value),0,Math::Max(0,(Choices.Num()-1)/20));Refresh();return;}
  if(A.Op=="choose"){int At=String::Conv_StringToInt(A.Value);if(Choices.IsValidIndex(At))Choose(Choices[At].Id);}
 }
 UFUNCTION() void Enter(USvgCanvasNode N)
 {
  auto Node=Cast<UOpusActionNode>(N);if(Node==nullptr||Panel==nullptr)return;int At=String::Conv_StringToInt(Node.HoverId);if(!Choices.IsValidIndex(At))return;
  CloseTip();Tip=Create("hotbar-tip",125);if(Tip==nullptr)return;Tip.SetVisibility(ESlateVisibility::HitTestInvisible);
  auto E=Choices[At];UOpusUI::SetText(Tip,"tip-name",E.Name);UOpusUI::SetText(Tip,"tip-detail",E.Description+"\n点击图标装配"+(E.Kind=="item"?" · 持有 "+E.Count:""));UOpusUI::SetImage(Tip,"tip-icon",E.Icon);
  UOpusHoverUI::Place(Tip,FVector2D(PalettePosition.X,Math::Max(12.0,PalettePosition.Y-168)),1);
 }
 UFUNCTION() void Leave(USvgCanvasNode N){CloseTip();}
 UFUNCTION() bool FilterKey(FKey Key)
 {
  if(Panel==nullptr)return false;
  if(Key==EKeys::Left||Key==EKeys::Right||Key==EKeys::Up||Key==EKeys::Down){FocusChoice(FocusedChoice+(Key==EKeys::Left?-1:Key==EKeys::Right?1:Key==EKeys::Up?-5:5));return true;}
  if(Key==EKeys::Home||Key==EKeys::End){FocusChoice(Key==EKeys::Home?0:Math::Min(20,Choices.Num()-Page*20)-1);return true;}
  if(Key==EKeys::Escape){Close();ConsumeShortcut=true;return true;}
  TArray<FKey> Digits;Digits.Add(EKeys::One);Digits.Add(EKeys::Two);Digits.Add(EKeys::Three);Digits.Add(EKeys::Four);
  for(int I=0;I<4;I++)if(Key==Digits[I]){FString Error;Tower.Rules.SelectPlayerSkill(I,Error);Hud.Screen.ApplySceneState("selected-"+(I+1));Close();ConsumeShortcut=true;return true;}
  if(Key==EKeys::Q||Key==EKeys::E||Key==EKeys::T||Key==EKeys::G){Close();ConsumeShortcut=true;return true;}return false;
 }
 bool HandleKeys(APlayerController PC)
 {
  if(ConsumeShortcut){ConsumeShortcut=false;return true;}if(Panel==nullptr)return false;
  TArray<FKey> Keys;Keys.Add(EKeys::Escape);Keys.Add(EKeys::One);Keys.Add(EKeys::Two);Keys.Add(EKeys::Three);Keys.Add(EKeys::Four);Keys.Add(EKeys::Q);Keys.Add(EKeys::E);Keys.Add(EKeys::T);Keys.Add(EKeys::G);
  for(auto Key:Keys)if(PC.WasInputKeyJustPressed(Key)&&FilterKey(Key)){ConsumeShortcut=false;return true;}
  if(PC.WasInputKeyJustPressed(EKeys::LeftMouseButton)&&!UOpusWorldUI::PointerBlocked(PC)){Close();return true;}return false;
 }
 USvgCanvasNode Anchor(bool Quick,int Index)
 {
  FString Key=Index==0?"Q":Index==1?"E":Index==2?"F":"G";
  return Hud.Screen.FindSourceNode("hud-screen-instance::rs-hotbar-instance::"+(Quick?"rs-consumables::hud-quick-instance-"+Key:"rs-abilities::hud-ability-instance-"+(Index+1)));
 }
 void Place()
 {
  if(Panel==nullptr)return;auto PC=Gameplay::GetPlayerController(0);auto V=UOpusHoverUI::ViewSize(PC);auto R=UOpusHoverUI::AnchorRect(Anchor(Mode=="quick",Slot),PC);
  PalettePosition=FVector2D(Math::Clamp((R.X+R.Z)*.5-147,12.0,Math::Max(12.0,V.X-306)),Math::Max(12.0,R.Y-Panel.TemplateSize.Y-10));UOpusHoverUI::Place(Panel,PalettePosition,1);
 }
 void RefreshQuick()
 {
  if(Tower.Recent==nullptr||!Tower.Recent.Ready)return;
  if(QuickSlots.IsEmpty())for(int I=0;I<4;I++){auto W=Create("hotbar-quick-slot",60);if(W==nullptr)return;UOpusUI::SetAction(W,"hotbar-quick-slot","quick",""+I);QuickSlots.Add(W);}
  auto PC=Gameplay::GetPlayerController(0);auto Inventory=UOpusData::Inventory(Tower.Rules);
  for(int I=0;I<QuickSlots.Num();I++)
  {
   auto W=QuickSlots[I];bool Visible=!Tower.Paused&&!UOpusUI::BlocksCombat(Hud.Screen);W.SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
   FString Id=Tower.Recent.AuraSlots[I].ToString(),Name="空槽",Icon,State;
   bool Aura=!Tower.Recent.AuraSlots[I].IsNone();
   if(Aura){for(auto C:Tower.Recent.Choices)if(C.Id==Tower.Recent.AuraSlots[I]){Name=C.Name;Icon=UOpusGameplayStore::SkillIcon(C.Id);}State=Tower.Recent.Enabled[I]?"启用":"未启用";}
   else {FName Item=UOpusNative::Belt(Tower.Rules,I);for(auto E:Inventory)if(E.Kind=="item"&&E.Id==Item.ToString()&&E.Count>0){Name=E.Name;Icon=E.Icon;State="×"+E.Count;}}
   UOpusUI::SetText(W,"quick-key",I==0?"Q":I==1?"E":I==2?"T":"G");UOpusUI::SetText(W,"quick-name",Name);UOpusUI::SetText(W,"quick-state",State);UOpusUI::SetImage(W,"quick-icon",Icon);UOpusUI::SetShown(W,"quick-icon",!Icon.IsEmpty());UOpusUI::SetShown(W,"quick-plus",Icon.IsEmpty());
   auto Edge=Cast<USvgCanvasMotionNode>(W.FindSourceNode("aura-slot-active"));if(Edge!=nullptr)Edge.SetMotionEnabled(Visible&&Aura&&Tower.Recent.Enabled[I]);
   auto R=UOpusHoverUI::AnchorRect(Anchor(true,I),PC);if(R.Z>R.X)UOpusHoverUI::Place(W,FVector2D(R.X,R.Y),float32((R.Z-R.X)/64));
  }
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Tower==nullptr||Hud.Screen==nullptr||Tower.Traveling)return;Poll-=Dt;
  if(Poll<=0){Poll=.15;RefreshQuick();}
  if(Panel!=nullptr){if(Tower.Paused||Tower.Events.IsOpen()||UOpusUI::BlocksCombat(Hud.Screen))Close();else Place();}
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){Close();for(auto W:QuickSlots)if(W!=nullptr)W.RemoveFromParent();QuickSlots.Empty();}
}
