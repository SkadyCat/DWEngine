// UICreator owns the panel and row prefabs. AS only binds/filter data.
class UDWOLootHunter : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 UPROPERTY() UOpusWorldScreen Panel;
 UPROPERTY() TArray<UOpusWorldScreen> AffixRows;
 UPROPERTY() TArray<UOpusWorldScreen> CurrencyRows;
 TArray<FOpusEntry> Pool,Currencies;
 FString Query,Signature;
 int Page=0,Pages=1,Matches=0;
 double RefreshIn=0;
 void Initialize(ADWOTowerGameMode T){Tower=T;Pool=UOpusData::AffixPool(T.Rules);for(auto E:UOpusData::Listings(T.Rules))if(E.Category=="currency")Currencies.Add(E);}
 bool Visible() const{return Panel!=nullptr&&Panel.IsVisible();}
 bool TextFocused() const{return Panel!=nullptr&&UOpusUI::TextFocused(Panel);}
 void Close(){if(Panel!=nullptr)Panel.SetVisibility(ESlateVisibility::Collapsed);auto PC=Gameplay::GetPlayerController(0);if(PC!=nullptr&&Tower.Hud.Screen!=nullptr)Tower.Hud.Screen.SetFocus();}
 void Open()
 {
  if(Visible()){Close();return;}
  Tower.WorldUI.CloseDetails();
  if(Panel==nullptr)
  {
   Panel=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"hunter","UICreatorLoot",110);
   if(Panel==nullptr){Tower.Fail("Missing UICreator loot Hunter");return;}
   Panel.OnSourceAction.AddUFunction(this,n"OnAction");Panel.InputFilter=FSvgCanvasInputFilter(this,n"FilterKey");
   UOpusUI::SetInput(Panel,"field:hunterQuery",Query);
   auto Search=Panel.FindSourceNode("hunter-search");for(auto W:Search.GetAllChildren()){auto Input=Cast<UEditableTextBox>(W);if(Input!=nullptr)Input.SetHintText(FText::FromString(Search.SourceName));}
  }
  Panel.SetVisibility(ESlateVisibility::SelfHitTestInvisible);Place();Signature="";Refresh();
 }
 UFUNCTION() bool FilterKey(FKey Key){if(Key==EKeys::Escape&&Visible()){Close();return true;}return false;}
 void Place(){if(Panel==nullptr)return;auto V=UOpusHoverUI::ViewSize(Gameplay::GetPlayerController(0));double S=Math::Min(1.0,(V.Y-40)/600.0);UOpusHoverUI::Place(Panel,FVector2D(20,Math::Clamp(106.0,20.0,Math::Max(20.0,V.Y-600*S-20))),float32(S));}
 void Rows(TArray<FOpusEntry> Values,TArray<UOpusWorldScreen>& Live,FString Binding,FString Op,TArray<FString> Selected)
 {
  for(auto W:Live){W.OnSourceAction.UnbindObject(this);W.RemoveFromParent();}Live.Empty();
  for(auto E:Values)
  {
   bool Chosen=Selected.Contains(E.Id);auto Row=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),Chosen?"hunter-row-selected-template":"hunter-row-template","UICreatorLoot",110,false);
   if(Row==nullptr)continue;UOpusUI::SetText(Row,"hunter:row-name",(Chosen?"✓  ":"□  ")+E.Name);UOpusUI::SetAction(Row,"hunter:row",Op,E.Id);Row.OnSourceAction.AddUFunction(this,n"OnAction");Live.Add(Row);
  }
  UOpusWorldUI::MountRows(Panel,Binding,Live);
 }
 void Refresh()
 {
  if(!Visible())return;auto H=Tower.Hud;TArray<FOpusEntry> Filtered;
  FString NewQuery=UOpusUI::Input(Panel,"field:hunterQuery").TrimStartAndEnd().ToLower();if(NewQuery!=Query){Query=NewQuery;Page=0;}
  for(auto E:Pool)if((!H.HunterOnly||H.HunterTargets.Contains(E.Id))&&(Query.IsEmpty()||E.Name.ToLower().Contains(Query)||E.Id.ToLower().Contains(Query)))Filtered.Add(E);
  Matches=Filtered.Num();Pages=Math::Max(1,int(Math::CeilToInt(double(Matches)/12)));Page=Math::Clamp(Page,0,Pages-1);
  FString Key=Query+Page+(H.HunterOnly?"only":"all");for(auto Id:H.HunterTargets)Key+=Id+";";Key+="/";for(auto Id:H.HunterCurrencies)Key+=Id+";";
  if(Key!=Signature)
  {
   TArray<FOpusEntry> PageRows;for(int I=Page*12;I<Math::Min(Matches,(Page+1)*12);I++)PageRows.Add(Filtered[I]);
   Rows(PageRows,AffixRows,"hunter:affixes","hunter-target",H.HunterTargets);Rows(Currencies,CurrencyRows,"hunter:currencies","hunter-currency",H.HunterCurrencies);Signature=Key;
  }
  int Shown=0;for(auto E:Tower.WorldUI.Ground)if(H.HunterAllows(E))Shown++;
  UOpusUI::SetText(Panel,"hunter:count","显示 "+Shown+" / "+Tower.WorldUI.Ground.Num()+" 件");
  UOpusUI::SetText(Panel,"hunter:pages",""+(Page+1)+" / "+Pages+" · "+Matches+" 种");
  UOpusUI::BindNode(Panel,"hunter-enabled-text","hunter:enabled-text");UOpusUI::SetText(Panel,"hunter:enabled-text",H.HunterEnabled?"✓ 启用过滤":"□ 过滤已关闭");
  UOpusUI::BindNode(Panel,"hunter-only-text","hunter:only-text");UOpusUI::SetText(Panel,"hunter:only-text",H.HunterOnly?"查看全部":"仅看已选");
  FString Names;for(auto Id:H.HunterTargets){for(auto E:Pool)if(E.Id==Id){if(!Names.IsEmpty())Names+=" · ";Names+=E.Name;break;}}
  UOpusUI::SetText(Panel,"hunter:selected",Names.IsEmpty()?"尚未选择装备条件":Names);
  auto Prev=Panel.FindSourceNode("hunter-prev"),Next=Panel.FindSourceNode("hunter-next");if(Prev!=nullptr)Prev.SetInteractionEnabled(Page>0);if(Next!=nullptr)Next.SetInteractionEnabled(Page+1<Pages);
 }
 UFUNCTION() void OnAction(USvgCanvasNode Node)
 {
  auto A=UOpusUI::Action(Node);auto H=Tower.Hud;
  if(A.Op=="hunter-close"){Close();return;}
  if(A.Op=="hunter-toggle")H.HunterEnabled=!H.HunterEnabled;
  if(A.Op=="hunter-clear")H.HunterTargets.Empty();
  if(A.Op=="hunter-only"){H.HunterOnly=!H.HunterOnly;Page=0;}
  if(A.Op=="hunter-page")Page=Math::Clamp(Page+A.Index,0,Pages-1);
  if(A.Op=="hunter-target"||A.Op=="hunter-affix"){if(H.HunterTargets.Contains(A.Value))H.HunterTargets.Remove(A.Value);else H.HunterTargets.Add(A.Value);}
  if(A.Op=="hunter-currency"){if(H.HunterCurrencies.Contains(A.Value))H.HunterCurrencies.Remove(A.Value);else H.HunterCurrencies.Add(A.Value);}
  H.SaveHunter();Tower.WorldUI.RefreshIn=0;Signature="";Refresh();
 }
 UFUNCTION(BlueprintOverride) void Tick(float Dt){if(!Visible())return;RefreshIn-=Dt;if(RefreshIn<=0){RefreshIn=0.1;Refresh();}Place();}
 UFUNCTION(BlueprintOverride) void EndPlay(EEndPlayReason Reason){if(Panel!=nullptr){Panel.OnSourceAction.UnbindObject(this);Panel.RemoveFromParent();}for(auto Row:AffixRows)Row.OnSourceAction.UnbindObject(this);for(auto Row:CurrencyRows)Row.OnSourceAction.UnbindObject(this);}
}
