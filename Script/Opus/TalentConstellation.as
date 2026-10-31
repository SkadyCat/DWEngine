// UICreator owns the screen, node geometry, PNGs and menus. AS owns allocation.
class UDWOTalentConstellation : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() UOpusWorldScreen Screen;
 UPROPERTY() TArray<FOpusTalentRecord> Nodes;
 UPROPERTY() TArray<FOpusTalentRecord> Attributes;
 UPROPERTY() TArray<int> Choices;
 TArray<FString> Unsupported;
 UPROPERTY() UOpusHoverEvents Events;
 int Budget=120,Selected=-1;
 bool Visible=false,Pinned=false,Dragging=false,Overview=false,SuppressKeys=false;
 float Zoom=1;
 int Hovered=-1,PendingHover=-1,Focused=0;bool SourcesExpanded=false;float HoverDelay=0,Poll=0;
 TArray<FString> RouteIds;
 FString Query,Category="all",PreviewSkill="frost_lance",LastSector;
 UPROPERTY() TArray<FOpusTalentRecord> Sectors;
 UPROPERTY() TArray<UOpusWorldScreen> SelectedRows;
 UPROPERTY() TArray<UOpusWorldScreen> ResultRows;
 FVector2D Pan,LastPointer;
 double TipTime=0,FitTime=0;
 FString Feedback;
 void Initialize(UDWOSvgHUD InHud)
 {
  Hud=InHud;Sectors=UOpusTalentBridge::Sectors();Nodes=UOpusTalentBridge::Catalog();Attributes=UOpusTalentBridge::Attributes();
  for(int I=0;I<Nodes.Num();I++){Choices.Add(-1);Unsupported.Add(Nodes[I].Kind=="legendary"?UOpusTalentBridge::TraitError(Hud.Tower.Rules,Nodes[I].Traits):"");}
  if(Nodes.Num()!=1000||Attributes.Num()!=4){UOpusNative::WriteLog("TALENT_ERROR invalid catalog");return;}
  FString State=UDWOTowerJourney::Get().TalentState;
  if(State.IsEmpty()&&!UOpusNative::Flag("DWOTransient"))State=UOpusTalentBridge::ReadState();
  if(!State.IsEmpty()&&!Restore(State))UOpusNative::WriteLog("TALENT_STATE_REJECTED invalid or disconnected allocation preserved on disk");
  FString Error;if(!Apply(Error))UOpusNative::WriteLog("TALENT_ERROR "+Error);
  UOpusNative::WriteLog("TALENT_READY nodes="+Nodes.Num()+" legends=100 allocated="+Spent());
 }
 int Index(FString Id) const {for(int I=0;I<Nodes.Num();I++)if(Nodes[I].Id==Id)return I;return -1;}
 int Spent() const {int N=0;for(int I=0;I<Choices.Num();I++)if(Choices[I]>=0)N+=Nodes[I].Cost;return N;}
 bool Lit(int I) const{return Nodes.IsValidIndex(I)&&(Nodes[I].Kind=="origin"||Choices[I]>=0);}
 bool CanReach(int I) const {if(!Nodes.IsValidIndex(I))return false;if(Lit(I))return true;for(int J:Nodes[I].Neighbors)if(Lit(J))return true;return false;}
 bool Valid() const
 {
  if(Budget<0||Budget>999||Spent()>Budget||Choices.Num()!=Nodes.Num())return false;
  int Root=Index("origin");if(Root<0||Choices[Root]!=-1)return false;
  for(int I=0;I<Nodes.Num();I++)if((Choices[I]>=0&&!Unsupported[I].IsEmpty())||Choices[I]<-1||Choices[I]>3||(Nodes[I].Kind=="legendary"&&Choices[I]>0))return false;
  TArray<int> Seen;Seen.Add(Root);int At=0;
  while(At<Seen.Num()){int I=Seen[At++];for(int J:Nodes[I].Neighbors)if(Lit(J)&&!Seen.Contains(J))Seen.Add(J);}
  for(int I=0;I<Nodes.Num();I++)if(Lit(I)&&!Seen.Contains(I))return false;
  return true;
 }
 FString Serialize() const {FString S="opus_constellation|3|"+Budget+"\n";for(int I=0;I<Nodes.Num();I++)if(Choices[I]>=0)S+=Nodes[I].Id+"|"+Choices[I]+"\n";return S;}
 bool Restore(FString State)
 {
  auto Previous=Choices;int OldBudget=Budget;TArray<FString> Lines;State.ParseIntoArray(Lines,"\n",true);
  if(Lines.Num()==0)return false;TArray<FString> Header;Lines[0].ParseIntoArray(Header,"|",false);
  if(Header.Num()!=3||Header[0]!="opus_constellation"||Header[1]!="3"||!Header[2].IsNumeric())return false;
  Budget=String::Conv_StringToInt(Header[2]);for(int I=0;I<Choices.Num();I++)Choices[I]=-1;
  bool Ok=true;for(int L=1;L<Lines.Num();L++){TArray<FString> Pair;Lines[L].ParseIntoArray(Pair,"|",false);if(Pair.Num()!=2||!Pair[1].IsNumeric()){Ok=false;break;}int I=Index(Pair[0]);if(I<0||Choices[I]!=-1){Ok=false;break;}Choices[I]=String::Conv_StringToInt(Pair[1]);}
  if(!Ok||!Valid()){Choices=Previous;Budget=OldBudget;return false;}return true;
 }
 bool Apply(FString& Error)
 {
  TArray<FString> Ids,Traits;
  // Stable catalogue order matches RuleStudio; these are non-equipment sources.
  for(int I=0;I<Nodes.Num();I++)if(Choices[I]>=0){Ids.Add(Nodes[I].Id);Traits.Add(Nodes[I].Kind=="attribute"?Attributes[Choices[I]].Traits:Nodes[I].Traits);}
  return UOpusTalentBridge::ApplyParts(Hud.Tower.Rules,Ids,Traits,Error);
 }
 bool Commit(TArray<int> Previous,int OldBudget)
 {
  FString Error;bool Ok=Valid();if(Ok)Ok=Apply(Error);
  if(Ok&&!UOpusNative::Flag("DWOTransient"))Ok=UOpusTalentBridge::WriteState(Serialize());
  if(!Ok){Choices=Previous;Budget=OldBudget;FString RestoreError;Apply(RestoreError);Tip(Error.IsEmpty()?"方案无效或保存失败":Error);return false;}
  UDWOTowerJourney::Get().TalentState=Serialize();Refresh();return true;
 }
 bool Choose(int I,int A)
 {
  if(!Nodes.IsValidIndex(I)||Nodes[I].Kind!="attribute"||!Attributes.IsValidIndex(A))return false;
  if(!CanReach(I)){Tip("无法到达");return false;}if(!Lit(I)&&Spent()+Nodes[I].Cost>Budget){Tip("规划点数不足");return false;}
  auto Previous=Choices;Choices[I]=A;return Commit(Previous,Budget);
 }
 bool Allocate(int I)
 {
  if(!Nodes.IsValidIndex(I)||Nodes[I].Kind!="legendary"||Lit(I))return false;
  if(!CanReach(I)){Tip("无法到达");return false;}if(!Unsupported[I].IsEmpty()){Tip("规则尚未接入");return false;}if(Spent()+Nodes[I].Cost>Budget){Tip("规划点数不足");return false;}
  auto Previous=Choices;Choices[I]=0;return Commit(Previous,Budget);
 }
 bool Refund(int I)
 {
  if(!Nodes.IsValidIndex(I)||Choices[I]<0)return false;auto Previous=Choices;Choices[I]=-1;
  if(!Valid()){Choices=Previous;Tip("退还后会断开后续星点");return false;}return Commit(Previous,Budget);
 }
 FVector2D Pointer() const {return UOpusTalentBridge::Pointer(Screen,Gameplay::GetPlayerController(0));}
 // Block descriptions wrap; compact inline labels follow the original CSS and stay on one line.
 void Label(USvgCanvasScreen S,FString Id,FString Value,bool Wrap=false){UOpusUI::SetText(S,Id,Value);if(Wrap||S==nullptr)return;auto N=S.FindSourceNode(Id);if(N==nullptr)return;for(auto W:N.GetAllChildren()){auto Fit=Cast<UScaleBox>(W);if(Fit==nullptr)continue;auto Line=Cast<UTextBlock>(Fit.GetChildAt(0));if(Line!=nullptr){Cast<UTextLayoutWidget>(Line).SetAutoWrapText(false);Line.SetWrapTextAt(0);}}}
 void Text(FString Id,FString Value){bool Wrap=Id.EndsWith("description")||Id.EndsWith("requirement")||Id=="legend-dialog-dom-127-text"||Id=="legend-tip-dom-120-text"||Id=="legend-dialog-dom-136-text"||Id=="talent-preview-result-text";Label(Screen,Id,Value,Wrap);}
 void Show(FString Id,bool Value){UOpusUI::SetShown(Screen,Id,Value);if(Value&&(Id=="talent-card"||Id=="talent-attribute-menu"||Id=="talent-legend-tip"||Id=="talent-attribute-tip"))Screen.FindSourceNode(Id).SetVisibility(ESlateVisibility::Visible);}
 // Same anchor/gap/inset as RuleStudio placeNodeCard, in authored 1600x900 coordinates.
 void Place(FString Id,float W,float H){int I=Selected>=0?Selected:Hovered>=0?Hovered:Focused;if(!Nodes.IsValidIndex(I))return;auto N=Nodes[I];auto Center=FVector2D(641,503.5)+(N.Position-FVector2D(3,18))*.182078666319354*Zoom+Pan;float R=(N.Kind=="origin"?31:N.Kind=="attribute"?9.5:N.SourceBuild.IsEmpty()?22:26)*.182078666319354*Zoom;float X=Center.X+R+18;if(X+W>1575)X=Center.X-R-18-W;UOpusTalentBridge::PlaceNode(Screen,Id,FVector2D(Math::Clamp(X,37.0,1575.0-W),Math::Clamp(Center.Y-H*.5,166.0,841.0-H)));}
 void Tip(FString Value){Feedback=Value;TipTime=2;if(Screen!=nullptr){HideDetails();TipTime=2;Text("notice-dom-112-text",Value);Show("talent-tip",true);Place("talent-tip",160,46);Text("talent-footer-text",Value);}}
 void HideDetails(){Pinned=false;Selected=-1;Hovered=-1;PendingHover=-1;TipTime=0;if(Screen!=nullptr){TArray<FString> Hidden;Hidden.Add("talent-choice-tip");Hidden.Add("talent-tip");Hidden.Add("talent-card");Hidden.Add("talent-legend-tip");Hidden.Add("talent-attribute-tip");Hidden.Add("talent-attribute-menu");for(FString Id:Hidden)Show(Id,false);}}
 void Open()
 {
  if(Nodes.Num()!=1000)return;UOpusNative::WriteLog("TALENT_OPEN_BEGIN");
  if(Screen==nullptr)
  {
   Screen=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"talent-screen","UICreatorTalents",150);
   if(Screen==nullptr){Hud.Tower.LastError="星盘 WBP 资源未就绪";return;}if(Screen.FindSourceNode("talent-search")==nullptr){Screen.RemoveFromParent();Screen=nullptr;Hud.Tower.LastError="完整星盘资源尚未导入";return;}
   UOpusTalentBridge::FitScreen(Screen,Gameplay::GetPlayerController(0));
   TArray<FString> Hidden;Hidden.Add("talent-icon-library");Hidden.Add("talent-results");Hidden.Add("talent-selected-row");Hidden.Add("talent-result-row");Hidden.Add("talent-plan-fill");for(FString Id:Hidden)Show(Id,false);
   Screen.OnSourceAction.AddUFunction(this,n"OnAction");Screen.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");
   Events=UOpusUI::HoverEvents(Screen);Events.Entered.AddUFunction(this,n"Enter");Events.Left.AddUFunction(this,n"Leave");
   for(auto N:Nodes)UOpusWorldUI::SetHover(Screen,"point-"+N.Id,N.Id);
   for(auto A:Attributes)UOpusWorldUI::SetHover(Screen,"talent-choose-"+A.Id,"attribute:"+A.Id);
   UOpusTalentBridge::Clip(Screen,"talent-viewport");
   for(auto N:Nodes)if(N.Kind=="legendary")UOpusTalentBridge::SmoothRaster(Screen,"legend-sample-"+N.Id);
   for(auto A:Attributes)UOpusTalentBridge::SmoothRaster(Screen,"attr-sample-"+A.Id);
   UOpusUI::SetInput(Screen,"talent-search",Query);UOpusUI::SetInput(Screen,"talent-budget-input",""+Budget);
   TArray<FString> Labels,Values;Labels.Add("定位星群…");Values.Add("");for(auto S:Sectors){Labels.Add(S.Name);Values.Add(S.Id);}UOpusTalentBridge::Options(Screen,"talent-sector",Labels,Values,"");
   Labels.Reset();Values.Reset();for(auto B:UOpusTalentBridge::Builds(Hud.Tower.Rules)){Labels.Add(B.Name);Values.Add(B.Id);}UOpusTalentBridge::Options(Screen,"talent-preview-skill",Labels,Values,PreviewSkill);
   Screen.FindSourceNode("talent-graph").SetRenderTransformPivot(FVector2D(.5,.5));
   UOpusTalentBridge::Scroll(Screen,"talent-inspector-scroll");
   UOpusTalentBridge::Scroll(Screen,"legend-dialog-dom-120");
   Show("legend-dialog-dom-136",false);Show("legend-dialog-dom-137",false);
  }
  Visible=true;FitTime=0;Screen.SetVisibility(ESlateVisibility::SelfHitTestInvisible);HideDetails();Refresh();Transform();if(Hud.ItemHover!=nullptr)Hud.ItemHover.Close();UOpusNative::WriteLog("TALENT_OPEN_END");
 }
 void Close(){Visible=false;HideDetails();Dragging=false;if(Screen!=nullptr)Screen.SetVisibility(ESlateVisibility::Collapsed);}
 UFUNCTION() bool FilterKey(FKey Key){if(!Visible)return false;if(Key==EKeys::Escape){SuppressKeys=true;if(Pinned||Selected>=0)HideDetails();else Close();return true;}if(Key==EKeys::P&&!UOpusUI::TextFocused(Screen)){SuppressKeys=true;Close();return true;}return false;}
 bool HandleKeys(APlayerController PC){if(SuppressKeys){SuppressKeys=false;return true;}if(UOpusUI::TextFocused(Screen)||(!Visible&&UOpusUI::TextFocused(Hud.Screen)))return Visible;if(PC.WasInputKeyJustPressed(EKeys::P)){if(Visible)Close();else Open();return true;}if(!Visible)return false;if(PC.WasInputKeyJustPressed(EKeys::Escape))FilterKey(EKeys::Escape);return true;}
 FString Kind(FOpusTalentRecord N) const{return N.Category=="attack"?"攻击":N.Category=="defense"?"防御":"特殊";}
 void LayoutCard(FOpusTalentRecord N,FString Effects){float Desc=UOpusTalentBridge::Measure(Screen,"talent-card-description",N.Description,368),Req=UOpusTalentBridge::Measure(Screen,"talent-card-requirement",N.Requirement,342),Eff=UOpusTalentBridge::Measure(Screen,"legend-dialog-dom-127-text",Effects,368);UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-121",FVector4(22,18,368,Desc));UOpusTalentBridge::Rect(Screen,"talent-card-description",FVector4(0,0,368,Desc));float Y=18+Desc+16;UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-122",FVector4(22,Y,368,Req+48));UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-124",FVector4(14,34,342,Req));UOpusTalentBridge::Rect(Screen,"talent-card-requirement",FVector4(0,0,342,Req));Y+=Req+66;UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-125",FVector4(22,Y,368,Eff+30));UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-127",FVector4(0,28.5,368,Eff));UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-127-text",FVector4(0,0,368,Eff));Y+=Eff+47;UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-133",FVector4(22,Y,368,18));Y+=25;float SourceHeight=SourcesExpanded?UOpusTalentBridge::Measure(Screen,"legend-dialog-dom-136-text",N.Sources,368):0;UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-134",FVector4(22,Y,368,18+SourceHeight));UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-136",FVector4(0,18,368,SourceHeight));UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-136-text",FVector4(0,0,368,SourceHeight));Show("legend-dialog-dom-136",SourcesExpanded);UOpusTalentBridge::Rect(Screen,"legend-dialog-dom-138",FVector4(22,Y+37+SourceHeight,368,154.5));Text("talent-card-description",N.Description);Text("talent-card-requirement",N.Requirement);Text("legend-dialog-dom-127-text",Effects);Text("legend-dialog-dom-136-text",N.Sources);}
 void LayoutHover(FOpusTalentRecord N,FString Effects)
 {
  float Desc=UOpusTalentBridge::Measure(Screen,"talent-hover-description",N.Description,308);
  float Req=UOpusTalentBridge::Measure(Screen,"talent-hover-requirement",N.Requirement,286);
  float Eff=UOpusTalentBridge::Measure(Screen,"legend-tip-dom-120-text",Effects,308);
  UOpusTalentBridge::Rect(Screen,"legend-tip-dom-118",FVector4(17,15,308,Desc));UOpusTalentBridge::Rect(Screen,"talent-hover-description",FVector4(0,0,308,Desc));
  float Y=15+Desc+12;UOpusTalentBridge::Rect(Screen,"legend-tip-dom-119",FVector4(17,Y,308,Eff+20));UOpusTalentBridge::Rect(Screen,"legend-tip-dom-120",FVector4(0,10,308,Eff));UOpusTalentBridge::Rect(Screen,"legend-tip-dom-120-text",FVector4(0,0,308,Eff));
  Y+=Eff+32;UOpusTalentBridge::Rect(Screen,"legend-tip-dom-122",FVector4(17,Y,308,Req+36.3));UOpusTalentBridge::Rect(Screen,"talent-hover-requirement",FVector4(12,27.3,286,Req));
  float Content=Math::Min(321.6,Y+Req+51.3);UOpusTalentBridge::Rect(Screen,"legend-tip-dom-117",FVector4(1,79.4,342,Content));UOpusTalentBridge::Rect(Screen,"legend-tip-dom-124",FVector4(1,79.4+Content,342,38));
  UOpusTalentBridge::Rect(Screen,"talent-legend-tip",FVector4(0,0,344,Content+118.4));Place("talent-legend-tip",344,Content+118.4);
  // Reflow after measured rectangles change, so Slate uses the current paragraph width.
  Text("talent-hover-description",N.Description);Text("talent-hover-requirement",N.Requirement);Text("legend-tip-dom-120-text",Effects);
  UOpusTalentBridge::Rect(Screen,"talent-hover-reach",FVector4(0,-1,180,14));
 }
 void ShowCard(int I,bool Pin)
 {
  if(Screen==nullptr||!Nodes.IsValidIndex(I))return;Selected=Pin?I:-1;Hovered=I;if(Pin){Focused=I;Route(I);SourcesExpanded=false;}Pinned=Pin;auto N=Nodes[I];
  Show("talent-attribute-menu",false);Show("talent-attribute-tip",false);Show("talent-card",Pin);Show("talent-legend-tip",!Pin);
  FString Prefix=Pin?"talent-card-":"talent-hover-";Text(Prefix+"title",N.Name);Text(Prefix+"description",N.Description);Text(Prefix+"requirement",N.Requirement);Text(Prefix+"kind",Kind(N)+" · 传奇特性");Text(Prefix+"state",Lit(I)?"已点亮":"未点亮");
  UOpusTalentBridge::CopyRaster(Screen,Prefix+"icon","legend-sample-"+N.Id);
  FString Effects=Unsupported[I].IsEmpty()?UOpusTalentBridge::Effects(Hud.Tower.Rules,N.Traits):"◇ 该特性的原生规则尚未接入";
  if(Pin){
   Text("talent-card-sector",N.SectorName+" · 节点消耗 1 点");Text("talent-card-remaining",""+(Budget-Spent()));Text("talent-card-allocate-label",Lit(I)?"退还此节点 · 1 点":Unsupported[I].IsEmpty()?"点亮节点 · 1 点":"当前规则暂不支持");
   Text("talent-card-source",N.SourceName.IsEmpty()?"来源 · 现有规则特性 / 原默认参数":"来源 BD · "+N.SourceName+" ↗");
   Text("legend-dialog-dom-127-text",Effects);Show("legend-dialog-dom-128",false);Show("legend-dialog-dom-129",false);Show("legend-dialog-dom-131",false);
   LayoutCard(N,Effects);
   Text("legend-dialog-dom-136-text",N.Sources);Show("legend-dialog-dom-137",false);
   int At=0;for(auto Other:Nodes)if(Other.Kind=="legendary"&&Other.Sector==N.Sector&&Other.Id!=N.Id&&At<4){FString Id="legend-dialog-dom-"+(140+At*2);Text(Id+"-text",Other.Name);UOpusUI::SetAction(Screen,Id,"focus",Other.Id);At++;}
   Place("talent-card",424,675);
  }else{
   TArray<FString> Rows;Effects.ParseIntoArray(Rows,"\n",true);FString Short;for(int R=0;R<Math::Min(4,Rows.Num());R++)Short+=Rows[R]+"\n";
   Text("legend-tip-dom-120-text",Short);Show("legend-tip-dom-121",false);UOpusTalentBridge::Rect(Screen,"legend-tip-dom-120-text",FVector4(0,0,308,60));
   Text("talent-hover-reach",!Unsupported[I].IsEmpty()?"当前规则暂不支持":Lit(I)?"已点亮":CanReach(I)?"点亮需要 1 点":"无法到达");Text("talent-hover-hint",CanReach(I)?"点击选择 ↗":"先点亮相邻节点");LayoutHover(N,Short);
  }
 }
 void Select(int I){HideDetails();if(!Nodes.IsValidIndex(I)||Nodes[I].Kind=="origin")return;Focused=I;Route(I);if(!CanReach(I)){Tip("无法到达");return;}if(!Lit(I)&&Spent()+Nodes[I].Cost>Budget){Tip("规划点数不足");return;}if(Nodes[I].Kind=="legendary"){ShowCard(I,true);Vectors();return;}Selected=I;Route(I);Pinned=true;Show("talent-attribute-menu",true);Show("talent-refund",Lit(I));Text("talent-menu-cost",Lit(I)?"免费更换":"消耗 1 点");Place("talent-attribute-menu",256,128.5);Vectors();}
 void HoverChoice(int I){if(!Visible||!Pinned||!Attributes.IsValidIndex(I))return;auto Menu=Screen.FindSourceNode("talent-attribute-menu");auto Button=Screen.FindSourceNode("talent-choose-"+Attributes[I].Id);auto Slot=Cast<UCanvasPanelSlot>(Menu.Slot);if(Button==nullptr||Slot==nullptr)return;Text("talent-choice-tip-text",Attributes[I].Name+" +"+Attributes[I].Value);auto At=Slot.GetPosition();float X=At.X+Button.DesignRect.X-Menu.DesignRect.X+Button.DesignRect.Z*.5-58;UOpusTalentBridge::PlaceNode(Screen,"talent-choice-tip",FVector2D(Math::Clamp(X,37.0,1459.0),Math::Min(At.Y+Menu.DesignRect.W+6,811.0)));Show("talent-choice-tip",true);}
 UFUNCTION() void Enter(USvgCanvasNode Node){if(!Visible)return;auto N=Cast<UOpusActionNode>(Node);if(N==nullptr)return;if(N.HoverId.StartsWith("attribute:")){for(int I=0;I<Attributes.Num();I++)if(N.HoverId=="attribute:"+Attributes[I].Id)HoverChoice(I);return;}if(Pinned)return;PendingHover=Index(N.HoverId);HoverDelay=.13;}
 void Hover(int I){if(!Nodes.IsValidIndex(I)||Pinned)return;Hovered=I;if(Nodes[I].Kind=="legendary"){ShowCard(I,false);return;}if(Nodes[I].Kind!="attribute")return;Show("talent-attribute-tip",true);Text("attribute-tip-dom-112-text",Lit(I)?Attributes[Choices[I]].Name+" +"+Attributes[Choices[I]].Value:"自由属性 +"+Attributes[0].Value);Text("attribute-tip-dom-113-text",Lit(I)?"已点亮":"1 点");Text("attribute-tip-dom-124-text",CanReach(I)?Lit(I)?"点击更换属性":"点击选择属性":"无法到达");Text("attribute-tip-dom-125-text",Lit(I)?"免费更换":"任选其一");Place("talent-attribute-tip",256,105.5);Vectors();}
 UFUNCTION() void Leave(USvgCanvasNode Node){auto N=Cast<UOpusActionNode>(Node);if(N!=nullptr&&N.HoverId.StartsWith("attribute:")){Show("talent-choice-tip",false);return;}PendingHover=-1;if(!Pinned){Hovered=-1;Show("talent-legend-tip",false);Show("talent-attribute-tip",false);}}
 void Focus(int I){if(!Nodes.IsValidIndex(I))return;Query="";Category="all";UOpusTalentBridge::InputValue(Screen,"talent-search","");Zoom=4.2;Pan=-(Nodes[I].Position-FVector2D(3,18))*.1820786663*Zoom;Transform();RefreshSearch();Select(I);}
 UFUNCTION() void OnAction(USvgCanvasNode Node)
 {
  if(!Visible)return;auto A=UOpusUI::Action(Node);if(A.Type!="talent"&&A.Type!="opus")return;Screen.SourceActionHandled=true;
  if(A.Op=="close")Close();else if(A.Op=="details-close")HideDetails();else if(A.Op=="node")Select(Index(A.Value));else if(A.Op=="focus")Focus(Index(A.Value));
  else if(A.Op=="attribute"){for(int I=0;I<Attributes.Num();I++)if(Attributes[I].Id==A.Value){if(Choose(Selected,I))HideDetails();break;}}
  else if(A.Op=="allocate"){if(Lit(Selected)){if(Refund(Selected))HideDetails();}else if(Allocate(Selected))HideDetails();}
  else if(A.Op=="refund"){if(Refund(Selected))HideDetails();}
  else if(A.Op=="clear"){auto Previous=Choices;for(int I=0;I<Choices.Num();I++)Choices[I]=-1;if(Commit(Previous,Budget))HideDetails();}
  else if(A.Op=="category"){Category=A.Value;RefreshSearch();}
  else if(A.Op=="overview"){Zoom=1;Pan=FVector2D::ZeroVector;Transform();HideDetails();}
  else if(A.Op=="zoom-in"||A.Op=="zoom-out"){Zoom=Math::Clamp(Zoom*(A.Op=="zoom-in"?1.25:.8),.7,14.0);Transform();HideDetails();}
  else if(A.Op=="export"){FString File=UOpusTalentBridge::PlanFile(UOpusTalentBridge::PlanJson(Serialize(),true),true);if(!File.IsEmpty())Text("talent-footer-text","方案已导出："+File);}
  else if(A.Op=="import"){FString File=UOpusTalentBridge::PlanFile("",false);if(!File.IsEmpty()){auto Previous=Choices;int Old=Budget;FString State=UOpusTalentBridge::PlanJson(File,false);if(!Restore(State))Tip("方案无效、版本不符或包含暂不支持的特性");else if(Commit(Previous,Old)){UOpusTalentBridge::InputValue(Screen,"talent-budget-input",""+Budget);HideDetails();Text("talent-footer-text","方案已导入");}}}
  else if(Node.SourceId=="legend-dialog-dom-135"){SourcesExpanded=!SourcesExpanded;if(Nodes.IsValidIndex(Selected))LayoutCard(Nodes[Selected],UOpusTalentBridge::Effects(Hud.Tower.Rules,Nodes[Selected].Traits));}
 }
 bool Matches(FOpusTalentRecord N) const{return (Category=="all"||N.Category==Category)&&(Query.IsEmpty()||(N.Name+N.Description+N.Requirement+N.SourceName+N.SectorName+N.Id+N.Traits).ToLower().Contains(Query));}
 void RefreshSearch(){if(Screen==nullptr)return;TArray<FString> Cats;Cats.Add("all");Cats.Add("attack");Cats.Add("defense");Cats.Add("special");for(FString C:Cats){UOpusTalentBridge::CopyRaster(Screen,"talent-cat-"+C,"talent-category-sample");UOpusTalentBridge::Tint(Screen,"talent-cat-"+C,FLinearColor(1,1,1,C==Category?1:0));}if(Query.IsEmpty()&&Category=="all"){Show("talent-results",false);for(auto N:Nodes)if(N.Kind=="legendary")Screen.FindSourceNode("point-"+N.Id).SetRenderOpacity(1);return;}TArray<UOpusWorldScreen> Rows;int Count=0;for(auto N:Nodes)if(N.Kind=="legendary"&&Matches(N)){Count++;if(Count>ResultRows.Num()){auto R=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"talent-result-row","UICreatorTalents",0,false);R.OnSourceAction.AddUFunction(this,n"OnAction");ResultRows.Add(R);}auto R=ResultRows[Count-1];Label(R,"talent-result-name",N.Name);Label(R,"talent-result-sector",N.SectorName);UOpusUI::SetAction(R,"talent-result-row","focus",N.Id);R.SetVisibility(ESlateVisibility::SelfHitTestInvisible);Rows.Add(R);}Show("talent-results",!Query.IsEmpty()||Category!="all");Text("search-dom-37-text",""+Count+" 个关键特性");UOpusWorldUI::MountRows(Screen,"search-dom-38",Rows);for(auto N:Nodes)if(N.Kind=="legendary")Screen.FindSourceNode("point-"+N.Id).SetRenderOpacity(Matches(N)?1:.18);}
 void RefreshPreview(){TArray<FString> Ids,Traits;for(int I=0;I<Nodes.Num();I++)if(Choices[I]>=0){Ids.Add(Nodes[I].Id);Traits.Add(Nodes[I].Kind=="attribute"?Attributes[Choices[I]].Traits:Nodes[I].Traits);}Text("talent-preview-result-text",UOpusTalentBridge::Preview(Hud.Tower.Rules,PreviewSkill,Ids,Traits));}
 void Route(int Target){RouteIds.Reset();if(!Nodes.IsValidIndex(Target))return;TArray<int> Prev,Queue;for(int I=0;I<Nodes.Num();I++){Prev.Add(-2);if(Lit(I)){Prev[I]=-1;Queue.Add(I);}}int At=0;while(At<Queue.Num()){int I=Queue[At++];if(I==Target)break;for(int J:Nodes[I].Neighbors)if(Prev[J]==-2&&(J==Target||Nodes[J].Kind!="legendary")){Prev[J]=I;Queue.Add(J);}}if(Prev[Target]==-2)return;int I=Target;while(I>=0){RouteIds.Add(Nodes[I].Id);I=Prev[I];}}
 void Vectors(){TArray<FString> On,Route;for(int I=0;I<Nodes.Num();I++){auto N=Nodes[I];if(Lit(I))On.Add(N.Id);int Mask=(Lit(I)?1:0)|(I==Focused?2:0)|(CanReach(I)&&!Lit(I)?4:0)|(I==Hovered?8:0);FString Color=N.Kind=="attribute"&&Lit(I)?Attributes[Choices[I]].Color:N.Color;UOpusTalentBridge::CircleState(Screen,"ring-"+N.Id,Mask,UOpusTalentBridge::CssColor(Color),Zoom);if(N.Kind=="origin")UOpusTalentBridge::CircleState(Screen,"star-"+N.Id,0,UOpusTalentBridge::CssColor("#cdb37b"),Zoom);if(N.Kind=="legendary")UOpusTalentBridge::CircleState(Screen,"crown-"+N.Id,0,UOpusTalentBridge::CssColor("#c5a875"),Zoom);}UOpusTalentBridge::LineState(Screen,"talent-lines",On,RouteIds,Zoom);}
 void Transform(){if(Screen==nullptr)return;auto G=Screen.FindSourceNode("talent-graph");G.SetRenderScale(FVector2D(Zoom,Zoom));G.SetRenderTranslation(Pan);Text("talent-zoom-value-text",""+Math::RoundToInt(Zoom*100)+"%");Vectors();}
 void Refresh()
 {
  if(Screen==nullptr)return;int Legends=0;TArray<int> Counts;for(int I=0;I<4;I++)Counts.Add(0);TArray<UOpusWorldScreen> Rows;
  for(int I=0;I<Nodes.Num();I++){auto N=Nodes[I];if(N.Kind=="attribute"){Show("icon-"+N.Id,Lit(I));if(Lit(I)){Counts[Choices[I]]++;UOpusTalentBridge::CopyRaster(Screen,"icon-"+N.Id,"attr-sample-"+Attributes[Choices[I]].Id);}}
   if(N.Kind=="legendary"){UOpusTalentBridge::CopyRaster(Screen,"icon-"+N.Id,"legend-sample-"+N.Id);Screen.FindSourceNode("icon-"+N.Id).SetRenderOpacity(Lit(I)||I==Selected?1:.85);if(Lit(I)){if(Legends>=SelectedRows.Num()){auto R=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"talent-selected-row","UICreatorTalents",0,false);R.OnSourceAction.AddUFunction(this,n"OnAction");SelectedRows.Add(R);}auto R=SelectedRows[Legends];Label(R,"talent-row-name",N.Name);Label(R,"talent-row-category",Kind(N));UOpusTalentBridge::CopyFrom(R,"talent-row-icon",Screen,"legend-sample-"+N.Id);UOpusUI::SetAction(R,"talent-selected-row","focus",N.Id);R.SetVisibility(ESlateVisibility::SelfHitTestInvisible);Rows.Add(R);Legends++;}}}
  Text("talent-spent-text",""+Spent());Text("talent-budget-total-text","/ "+Budget);Text("talent-legend-count-text",""+Legends+" 个传奇已点亮");Text("talent-plan-spent-text","已分配 "+Spent()+" 点");Text("talent-plan-remaining-text","可用 "+(Budget-Spent())+" 点");Text("talent-remaining-text",""+(Budget-Spent()));Text("talent-attr-count-text",""+(Spent()-Legends)+" 个过渡点");
  Show("talent-plan-fill",Spent()>0);UOpusWorldUI::Fill(Screen,"talent-plan-fill",Budget>0?float(Spent())/Budget:0);
  for(int I=0;I<4;I++){Text("talent-total-"+Attributes[I].Id+"-text","+"+(Counts[I]*Attributes[I].Value));Text("talent-live-"+Attributes[I].Id+"-text",""+Math::RoundToInt(UOpusTalentBridge::Stat(Hud.Tower.Rules,FName("attr."+Attributes[I].Id))));}
  Show("talent-empty",Legends==0);float ListHeight=Legends==0?97.390625:65.5+Legends*41;UOpusTalentBridge::Rect(Screen,"talent-selected-list",FVector4(0,421.46875,319,ListHeight));UOpusTalentBridge::Rect(Screen,"talent-selected-mount",FVector4(19,47.5,281,Math::Max(1,Legends)*41));UOpusTalentBridge::Rect(Screen,"talent-preview",FVector4(0,421.46875+ListHeight,319,149.5));UOpusWorldUI::MountRows(Screen,"talent-selected-mount",Rows);RefreshPreview();Route(Focused);Vectors();
 }
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(!Visible||Screen==nullptr)return;FitTime-=DeltaSeconds;if(FitTime<=0){FitTime=.3;UOpusTalentBridge::FitScreen(Screen,Gameplay::GetPlayerController(0));}
  if(TipTime>0){TipTime-=DeltaSeconds;if(TipTime<=0)Show("talent-tip",false);}if(PendingHover>=0){HoverDelay-=DeltaSeconds;if(HoverDelay<=0){int I=PendingHover;PendingHover=-1;Hover(I);}}
  Poll-=DeltaSeconds;if(Poll<=0){Poll=.2;FString NewQuery=UOpusUI::Input(Screen,"talent-search").TrimStartAndEnd().ToLower();if(NewQuery!=Query){Query=NewQuery;RefreshSearch();}FString Skill=UOpusTalentBridge::Option(Screen,"talent-preview-skill");if(!Skill.IsEmpty()&&Skill!=PreviewSkill){PreviewSkill=Skill;RefreshPreview();}FString Sector=UOpusTalentBridge::Option(Screen,"talent-sector");if(Sector!=LastSector){LastSector=Sector;FVector2D Center;int Count=0;for(auto N:Nodes)if(N.Kind=="legendary"&&N.Sector==Sector){Center+=N.Position;Count++;}if(Count>0){Center/=Count;Zoom=2.4;Pan=-Center*.1820786663*Zoom;Transform();HideDetails();}}if(!UOpusUI::TextFocused(Screen)){int NewBudget=UOpusUI::IntInput(Screen,"talent-budget-input",Budget);if(NewBudget!=Budget){int Old=Budget;auto Previous=Choices;Budget=NewBudget;if(!Commit(Previous,Old))UOpusTalentBridge::InputValue(Screen,"talent-budget-input",""+Budget);}}}
  auto PC=Gameplay::GetPlayerController(0);if(PC==nullptr)return;auto P=Pointer();bool CardHit=(Pinned&&(Screen.FindSourceNode("talent-card").IsHovered()||Screen.FindSourceNode("talent-attribute-menu").IsHovered()));bool Inside=P.X>25&&P.X<1257&&P.Y>209&&P.Y<789&&!CardHit;if(Pinned&&Inside&&PC.WasInputKeyJustPressed(EKeys::LeftMouseButton))HideDetails();
  if(Inside&&(PC.WasInputKeyJustPressed(EKeys::MouseScrollUp)||PC.WasInputKeyJustPressed(EKeys::MouseScrollDown))){float Old=Zoom;Zoom=Math::Clamp(Zoom*(PC.WasInputKeyJustPressed(EKeys::MouseScrollUp)?1.2:1/1.2),.7,14.0);auto C=FVector2D(641,503.5);Pan=P-C-(P-C-Pan)*(Zoom/Old);Transform();HideDetails();}
  if(PC.WasInputKeyJustPressed(EKeys::LeftMouseButton)&&Inside&&Hovered<0&&PendingHover<0){Dragging=true;LastPointer=P;HideDetails();}if(!PC.IsInputKeyDown(EKeys::LeftMouseButton))Dragging=false;if(Dragging){Pan+=P-LastPointer;LastPointer=P;Transform();}
 }
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){if(Screen!=nullptr)Screen.RemoveFromParent();}
}
