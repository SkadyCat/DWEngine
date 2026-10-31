namespace DWOHoverTests
{
 bool Check(bool Good,FString What){if(!Good)UOpusNative::WriteLog("HOVER_TEST_FAIL "+What);return Good;}
 UOpusActionNode Find(UWidget W,FString Id)
 {
  auto N=Cast<UOpusActionNode>(W);if(N!=nullptr&&N.HoverId==Id)return N;
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Find(C,Id);if(Found!=nullptr)return Found;}return nullptr;
 }
 UButton Input(UWidget W)
 {
  auto B=Cast<UButton>(W);if(B!=nullptr)return B;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Input(C);if(Found!=nullptr)return Found;}return nullptr;
 }
 FString VisibleText(UWidget W)
 {
  if(W==nullptr||W.GetVisibility()==ESlateVisibility::Collapsed||W.GetVisibility()==ESlateVisibility::Hidden)return "";
  auto Text=Cast<UTextBlock>(W);if(Text!=nullptr)return Text.GetText().ToString();
  FString Result;auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren())Result+=VisibleText(C)+"\n";return Result;
 }
 USvgCanvasNode Action(UWidget W,FString Op,FString Value="")
 {
  auto N=Cast<USvgCanvasNode>(W);if(N!=nullptr){auto A=UOpusUI::Action(N);if(A.Op==Op&&(Value.IsEmpty()||A.Value==Value)&&N.GetVisibility()!=ESlateVisibility::Collapsed)return N;}
  auto P=Cast<UPanelWidget>(W);if(P!=nullptr)for(auto C:P.GetAllChildren()){auto Found=Action(C,Op,Value);if(Found!=nullptr)return Found;}return nullptr;
 }
 int Count(ADWOTowerGameMode T,FString Id){for(auto E:UOpusData::Inventory(T.Rules))if(E.Id==Id)return E.Count;return 0;}
 FString FixedLines(ADWOTowerGameMode T,FString Id){FString Result;for(auto L:UOpusData::HoverLines(T.Rules,Id))if(L.Style==3||L.Style==4)Result+=L.Text;return Result;}
 bool Click(UWidget W){auto B=Input(W);if(B==nullptr||!B.IsEnabled)return false;B.OnClicked.Broadcast();return true;}
 bool CraftInPlace(ADWOTowerGameMode T)
 {
  auto I=T.Hud.ItemHover;auto Before=I.Panel;FString Page=T.Hud.Screen.CurrentState,Id=I.ItemId,Fixed=FixedLines(T,Id);
  FString Snapshot=T.Rules.ExportPlayerState();FOpusEntry Low,Omen,Whole;TArray<FOpusEntry> Stones;
  for(auto E:UOpusData::Inventory(T.Rules))if(E.Category=="currency"&&E.CurrencyGrade>0)Stones.Add(E);else if(E.Category=="omen"&&Omen.Id.IsEmpty())Omen=E;else if(E.Category=="currency"&&E.CraftOp=="roll_affixes")Whole=E;
  for(auto E:Stones)if(E.CurrencyGrade==1)Low=E;
  if(!Check(!Low.Id.IsEmpty()&&!Whole.Id.IsEmpty()&&!Omen.Id.IsEmpty(),"owned native materials cover spirit stones, whole-item currency and omen"))return false;
  auto CurrencyNode=Action(Before.GetRootWidget(),"hover-currency",Low.Id);
  if(!Check(CurrencyNode!=nullptr&&Click(CurrencyNode)&&I.CurrencyId==Low.Id,"native currency icon activates inside the pinned inspector"))return false;
  CurrencyNode=Action(Before.GetRootWidget(),"hover-currency",Low.Id);Click(CurrencyNode);
  if(!Check(I.CurrencyId.IsEmpty()&&Snapshot==T.Rules.ExportPlayerState(),"activation toggles off without payment, inventory or RNG mutation"))return false;
  auto OmenNode=Action(Before.GetRootWidget(),"hover-omen",Omen.Id);if(!Check(OmenNode!=nullptr&&Click(OmenNode)&&I.OmenId==Omen.Id,"optional omen activates in place"))return false;
  OmenNode=Action(Before.GetRootWidget(),"hover-omen",Omen.Id);auto OmenInput=Input(OmenNode);OmenInput.OnHovered.Broadcast();
  if(!Check(I.DetailPanel!=nullptr&&VisibleText(I.DetailPanel.GetRootWidget()).Contains(Omen.Name),"UICreator material hover reads the actual owned material"))return false;
  I.FilterKey(EKeys::LeftControl);OmenInput.OnUnhovered.Broadcast();if(!Check(I.DetailPanel!=nullptr&&I.DetailPinned,"Ctrl pins the material explanation"))return false;
  I.FilterKey(EKeys::Escape);if(!Check(I.DetailPanel==nullptr&&I.Panel==Before,"Escape dismisses only the nested explanation"))return false;
  // Return to unrestricted native pool before the transaction assertions.
  Click(Action(Before.GetRootWidget(),"hover-omen",Omen.Id));
  int Paid=0;
  for(int Grade=1;Grade<=3;Grade++)
  {
   FOpusEntry Money;for(auto E:Stones)if(E.CurrencyGrade==Grade)Money=E;if(Money.Id.IsEmpty())continue;
   int BeforeCount=Count(T,Money.Id);if(BeforeCount<=0)continue;
   auto Control=Action(Before.GetRootWidget(),"hover-currency",Money.Id);if(!Check(Control!=nullptr&&Click(Control),"grade icon is a live exported UButton"))return false;
   if(I.CurrencyId!=Money.Id)Click(Action(Before.GetRootWidget(),"hover-currency",Money.Id));
   if(I.Craft.EligibleAffixes.Num()==0)continue;int Index=I.Craft.EligibleAffixes[0];
   auto Row=Action(Before.GetRootWidget(),"hover-affix",""+Index);if(!Check(Row!=nullptr&&Click(Row),"eligible affix row directly performs the native transaction"))return false;
   Paid++;
   if(!Check(Count(T,Money.Id)==BeforeCount-1&&I.Panel==Before&&I.Pinned&&T.Hud.Screen.CurrentState==Page,"one row click spends one material and keeps the same item, panel and page"))return false;
   bool Bound=false;for(auto L:UOpusData::HoverLines(T.Rules,Id))if(L.Style==2&&L.Index==Index)Bound=L.Grade==Grade;
   if(!Check(Bound&&Fixed==FixedLines(T,Id),"native grade binding is displayed while fixed bases and legendary lines remain intact"))return false;
  }
  if(!Check(Paid>0,"at least one actual native line reroll completed"))return false;
  UOpusNative::WriteLog("HOVER_TEST_PASS InlineGradeCraft transactions="+Paid);
  int BeforeWhole=Count(T,Whole.Id);Click(Action(Before.GetRootWidget(),"hover-currency",Whole.Id));if(I.CurrencyId!=Whole.Id)Click(Action(Before.GetRootWidget(),"hover-currency",Whole.Id));
  if(!Check(Click(Action(Before.GetRootWidget(),"hover-whole")),"whole-item currency exposes its own inline action"))return false;
  if(!Check(Count(T,Whole.Id)==BeforeWhole-1&&I.Craft.BoundCount==0&&UOpusData::EquipmentAffixes(T.Rules,Id).Num()==Whole.CraftCount&&Fixed==FixedLines(T,Id),"whole reroll uses the native count, clears grade bindings and preserves fixed properties"))return false;
  if(!Check(I.Panel==Before&&I.ItemId==Id&&T.Hud.Screen.CurrentState==Page,"whole reroll also preserves the pinned inspector and underlying page"))return false;
  UOpusNative::WriteLog("HOVER_TEST_PASS WholeCraftInPlace");return true;
 }
 bool IdentifyInPlace(ADWOTowerGameMode T)
 {
  auto H=T.Hud;auto Inspector=H.ItemHover;auto Before=Inspector.Panel;
  if(!Check(Before!=nullptr&&Inspector.Pinned,"identification starts in a pinned hover"))return false;
  FString Page=H.Screen.CurrentState;int BeforeCount=0;for(auto E:UOpusData::Inventory(T.Rules))if(E.Kind=="equipment")BeforeCount+=E.Count;
  auto Stones=UOpusNative::Stones(T.Rules);auto Action=Before.FindSourceNode("hover-"+Inspector.Rarity+"-battle-item-hover-identify");
  if(!Check(Action!=nullptr,"hover identification button exists"))return false;
  Before.HandleInvoke(Action);
  bool Ok=Check(Inspector.Panel==Before&&Inspector.Pinned,"identify updates the same pinned hover without recreating it");
  Ok=Check(H.Screen.CurrentState==Page&&Inspector.Context==Page,"identify opens no modal and preserves the underlying page")&&Ok;
  bool Identified=false;int AfterCount=0;for(auto E:UOpusData::Inventory(T.Rules)){if(E.Kind=="equipment")AfterCount+=E.Count;if(E.Id==Inspector.ItemId)Identified=E.Identified;}
  Ok=Check(Identified&&Inspector.ItemId==H.SelectedId,"hover follows the actual identified instance UID")&&Ok;
  Ok=Check(BeforeCount==AfterCount&&Stones==UOpusNative::Stones(T.Rules),"identification preserves owned equipment count and currency")&&Ok;
  FString Text=VisibleText(Before.FindSourceNode("hover-"+Inspector.Rarity+"-battle-item-hover-content"));
  Ok=Check(!Text.Contains("未鉴定")&&Action.GetVisibility()==ESlateVisibility::Collapsed&&Inspector.Craft.Instance,"identification immediately reveals inline crafting and retires the identify action")&&Ok;
  Inspector.Refresh();Inspector.Place();
  Ok=Check(Inspector.Panel==Before&&Inspector.Pinned&&H.Screen.CurrentState==Page,"identified hover survives the next refresh")&&Ok;
  if(Ok)UOpusNative::WriteLog("HOVER_TEST_PASS IdentifyInPlace uid="+Inspector.ItemId+" affixes="+UOpusData::EquipmentAffixes(T.Rules,Inspector.ItemId).Num());
  return Ok;
 }
 FOpusEntry Prepare(ADWOTowerGameMode T,FString Wanted="affixed")
 {
  FString Error;FString Profile=UOpusNative::ReadProfile();if(!Profile.IsEmpty())T.Rules.ImportPlayerState(Profile,Error);
  auto Bag=UOpusData::Inventory(T.Rules);bool Affixed=false;for(auto E:Bag)if(E.Identified&&UOpusData::EquipmentAffixes(T.Rules,E.Id).Num()>0)Affixed=true;
  if(!Affixed&&Wanted!="unidentified")for(auto E:Bag)if(!E.Identified){FString Uid;if(T.Rules.IdentifyPlayerEquipment(FName(E.Id),Uid,Error))break;}
  Bag=UOpusData::Inventory(T.Rules);FOpusEntry Selected;
  for(auto E:Bag)if(E.Kind=="equipment")
  {
   if(Selected.Id.IsEmpty())Selected=E;
   if((Wanted=="affixed"&&E.Identified&&UOpusData::EquipmentAffixes(T.Rules,E.Id).Num()>0)||(Wanted=="unidentified"&&!E.Identified)||(Wanted=="legendary"&&E.Rarity=="legendary")){Selected=E;break;}
  }
  if(Wanted=="unidentified")
  {
   int Best=-1;for(auto E:Bag)if(E.Kind=="equipment"&&!E.Identified){int Rank=E.Rarity=="legendary"?3:E.Rarity=="rare"?2:E.Rarity=="magic"?1:0;if(Rank>Best){Selected=E;Best=Rank;}}
  }
  if(Wanted=="reference")
  {
   int Best=-1;for(auto E:Bag)if(E.Kind=="equipment"&&E.Identified){int Lines=UOpusData::EquipmentAffixes(T.Rules,E.Id).Num();int Rank=Lines*10+(E.Slot=="chest"?3:0)+(E.Rarity=="rare"?2:0);if(Rank>Best){Selected=E;Best=Rank;}}
  }
  FString ItemQuery=UOpusNative::StringOption("DWOHoverItem");
  if(!ItemQuery.IsEmpty())for(auto E:Bag)if(E.Kind=="equipment"&&E.Identified&&E.Name.Contains(ItemQuery)){Selected=E;break;}
  T.Hud.Screen.ApplySceneState("character");auto Search=T.Hud.Screen.FindSourceNode("battle_live_modal_page::battle-character-search");
  if(Search!=nullptr)for(auto C:Search.GetAllChildren()){auto Edit=Cast<UEditableTextBox>(C);if(Edit!=nullptr)Edit.SetText(FText::FromString(Selected.Name));}
  T.Hud.RefreshData();return Selected;
 }
 bool Run(ADWOTowerGameMode T,int Part)
 {
  auto H=T.Hud;auto Inspector=H.ItemHover;bool Ok=true;
  if(Part==1)return CraftInPlace(T);
  if(Part==2)
  {
   Inspector.Close();auto Unknown=Prepare(T,"unidentified");auto UnknownRoot=Find(H.Screen.FindSourceNode("battle_live_modal_page::battle-character-grid"),Unknown.Id);auto UnknownButton=Input(UnknownRoot);if(UnknownButton!=nullptr)UnknownButton.OnClicked.Broadcast();
   if(!Check(!Unknown.Identified,"identification fixture is unidentified"))return false;
   Ok=Check(H.Screen.CurrentState=="character"&&Inspector.Pinned&&Inspector.ItemId==Unknown.Id,"character item click pins the hover instead of opening an identification modal")&&Ok;
   if(Inspector.Panel==nullptr)return false;Ok=IdentifyInPlace(T)&&Ok;
   H.Screen.HandleInputKey(EKeys::Escape);return Check(Inspector.Panel==nullptr&&H.Screen.CurrentState=="character","Escape closes the identified hover without leaving the page")&&Ok;
  }
  if(Part==3)
  {
   FString IdentifiedId=H.SelectedId;H.Screen.ApplySceneState("inventory");H.RefreshData();auto BagRoot=Find(H.Screen.FindSourceNode("battle_live_modal_page::battle-bag-grid"),IdentifiedId);auto BagButton=Input(BagRoot);if(BagButton!=nullptr)BagButton.OnClicked.Broadcast();
   Ok=Check(H.Screen.CurrentState=="inventory"&&H.SelectedId==IdentifiedId,"inventory item click selects the cell without opening a modal")&&Ok;
   auto Item=Prepare(T);auto Root=Find(H.Screen.FindSourceNode("battle_live_modal_page::battle-character-grid"),Item.Id);auto Button=Input(Root);if(Button!=nullptr)Button.OnHovered.Broadcast();Inspector.Pin();H.Screen.ApplySceneState("inventory");Inspector.Refresh();
   return Check(Inspector.Panel==nullptr,"navigation clears the pinned portal")&&Ok;
  }
  auto Item=Prepare(T);
  auto Root=Find(H.Screen.FindSourceNode("battle_live_modal_page::battle-character-grid"),Item.Id);
  Ok=Check(Root!=nullptr,"live inventory cell carries actual item identity")&&Ok;if(Root==nullptr)return false;
  auto Button=Input(Root);Ok=Check(Button!=nullptr,"imported cell input exists")&&Ok;if(Button==nullptr)return false;
  FString Before=T.Rules.ExportPlayerState();Button.OnHovered.Broadcast();
  Ok=Check(Inspector.Panel!=nullptr&&Inspector.ItemId==Item.Id&&!Inspector.Pinned,"native UButton hover opens the full item panel")&&Ok;if(Inspector.Panel==nullptr)return false;
  FString ExpectedHash=UOpusNative::StringOption("DWOExpectedHoverHash");if(ExpectedHash.IsEmpty())ExpectedHash="fae51032a46d7f429aa32ff3d5ac1f1f0f3eb872d1d4092ed8a70f7b61fbd53a";
  Ok=Check(UOpusUI::SourceHash(Inspector.Panel)==ExpectedHash,"saved UICreator inspector export matches the current source")&&Ok;
  Ok=Check(Inspector.Panel.GetVisibility()==ESlateVisibility::HitTestInvisible,"unfixed panel passes mouse input")&&Ok;
  Ok=Check(Before==T.Rules.ExportPlayerState(),"hover is read-only")&&Ok;
  Button.OnUnhovered.Broadcast();Ok=Check(Inspector.Panel==nullptr,"leaving closes an unfixed panel")&&Ok;
  Button.OnHovered.Broadcast();H.Screen.HandleInputKey(EKeys::LeftControl);Button.OnUnhovered.Broadcast();
  Ok=Check(Inspector.Panel!=nullptr&&Inspector.Pinned&&Inspector.Panel.GetVisibility()==ESlateVisibility::SelfHitTestInvisible,"pin survives mouse leave and enables actions")&&Ok;
  if(Inspector.Panel==nullptr)return false;
  H.Screen.HandleInputKey(EKeys::Escape);Ok=Check(Inspector.Panel==nullptr&&H.Screen.CurrentState=="character","Escape closes only the pinned hover panel")&&Ok;
  Button.OnHovered.Broadcast();H.Screen.HandleInputKey(EKeys::LeftControl);if(Inspector.Panel==nullptr)return false;
  FString Prefix="hover-"+Inspector.Rarity+"-battle-item-hover-";
  auto Equip=Inspector.Panel.FindSourceNode(Prefix+"equip");Ok=Check(Equip!=nullptr,"source equip button exists")&&Ok;
  if(Equip!=nullptr&&Item.Identified){Inspector.Panel.HandleInvoke(Equip);bool Changed=false;for(auto E:UOpusData::Inventory(T.Rules))if(E.Id==Item.Id)Changed=E.Equipped!=Item.Equipped;Ok=Check(Changed,"pinned equip action updates the native loadout")&&Ok;Inspector.Panel.HandleInvoke(Equip);}
  Ok=Check(Inspector.Panel.FindSourceNode(Prefix+"details")==nullptr&&Inspector.Panel.FindSourceNode(Prefix+"craft")==nullptr,"legacy details and navigation-to-craft controls are removed from the exported source")&&Ok;
  return Ok;
 }
 bool View(ADWOTowerGameMode T)
 {
  FString Wanted=UOpusNative::StringOption("DWOHoverView");auto Item=Prepare(T,Wanted=="identified"?"unidentified":Wanted);auto Root=Find(T.Hud.Screen.FindSourceNode("battle_live_modal_page::battle-character-grid"),Item.Id);auto Button=Input(Root);
  if(Wanted=="unidentified"&&!Check(!Item.Identified,"visual fixture contains an unidentified item"))return false;
  if(Wanted=="affixed"&&!Check(UOpusData::EquipmentAffixes(T.Rules,Item.Id).Num()>0,"visual fixture contains real random affixes"))return false;
  if(!Check(Button!=nullptr,"visual hover cell exists"))return false;T.Hud.Screen.ForceLayoutPrepass();Button.OnHovered.Broadcast();if(UOpusNative::Flag("DWOHoverPinned")||Wanted=="identified")T.Hud.ItemHover.Pin();
  if(Wanted=="identified"&&!IdentifyInPlace(T))return false;
  FString Material=UOpusNative::StringOption("DWOHoverCurrency"),ChosenMaterial;
  if(!Material.IsEmpty())for(auto E:T.Hud.ItemHover.Materials)if(E.Category=="currency"&&(E.Id==Material||(Material=="low"&&E.CurrencyGrade==1))){ChosenMaterial=E.Id;break;}
  if(!ChosenMaterial.IsEmpty()){auto Choice=Action(T.Hud.ItemHover.Panel.GetRootWidget(),"hover-currency",ChosenMaterial);if(Choice!=nullptr)Click(Choice);}
  if(UOpusNative::Flag("DWOHoverProbe"))
  {
   auto I=T.Hud.ItemHover;
   auto Owned=I.Materials;
   for(auto M:Owned){auto Choice=Action(I.Panel.GetRootWidget(),M.Category=="currency"?"hover-currency":"hover-omen",M.Id);auto B=Input(Choice);if(B!=nullptr){B.OnHovered.Broadcast();B.OnUnhovered.Broadcast();}}
   auto First=Input(Action(I.Panel.GetRootWidget(),"hover-affix","0"));if(First!=nullptr){First.OnHovered.Broadcast();First.OnUnhovered.Broadcast();}
   auto Next=Input(Action(I.Panel.GetRootWidget(),"hover-affix","1"));if(Next!=nullptr)Next.OnHovered.Broadcast();
  }
  for(auto CurrentItem:UOpusData::Inventory(T.Rules))if(CurrentItem.Id==T.Hud.ItemHover.ItemId){Item=CurrentItem;break;}
  UOpusNative::WriteLog("HOVER_VIEW item="+Item.Name+" identified="+(Item.Identified?1:0)+" random="+UOpusData::EquipmentAffixes(T.Rules,Item.Id).Num());return T.Hud.ItemHover.Panel!=nullptr;
 }
}
// Real user operations happen on separate frames. Keep the integration probe
// out of BeginPlay and do not batch all UI transitions into one script call.
class UDWOHoverContractTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0,Frames=0;
 bool Done=false;
 void Initialize(ADWOTowerGameMode T){Tower=T;SetTickGroup(ETickingGroup::TG_PostUpdateWork);}
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds)
 {
  if(Done||Tower==nullptr||++Frames<4)return;
  if(!DWOHoverTests::Run(Tower,Stage)){Done=true;UOpusNative::Quit(false);return;}
  Stage++;Frames=0;
  if(Stage==4){Done=true;UOpusNative::WriteLog("HOVER_TEST_PASS RuntimeInspector");UOpusNative::Quit(true);}
 }
}
