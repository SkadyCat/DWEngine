// Connect the authored UICreator attribute ScrollBox to the existing native adapter.
namespace DWOAttributeScroll
{
 UScrollBox Find(USvgCanvasNode Root)
 {
  if(Root!=nullptr)for(auto Child:Root.GetAllChildren()){auto Scroll=Cast<UScrollBox>(Child);if(Scroll!=nullptr)return Scroll;}return nullptr;
 }
 void DrawStatic(USvgCanvasScreen Screen,UWidget Widget)
 {
  auto Node=Cast<USvgCanvasNode>(Widget);
  if(Node!=nullptr&&((Node.SourceType=="TextBlock"&&!Node.SourceId.EndsWith("-val"))||Node.SourceType=="Border"))
  {
   // Offscreen glyphs/lines have no exported raster. Keep their authored styles
   // and materialize them before they enter the native scrolling viewport.
   FString Binding="attribute-static:"+Node.SourceId;UOpusUI::BindNode(Screen,Node.SourceId,Binding);
   UOpusUI::SetText(Screen,Binding,Node.SourceType=="TextBlock"?Node.SourceText:"");
  }
  auto Panel=Cast<UPanelWidget>(Widget);if(Panel!=nullptr)for(auto Child:Panel.GetAllChildren())DrawStatic(Screen,Child);
 }
 void Ensure(UDWOSvgHUD Hud)
 {
  auto Root=Hud.Screen.FindSourceNode("battle_live_modal_page::battle-character-attributes");
  if(Root==nullptr||Find(Root)!=nullptr)return;
  UOpusUI::HidePrefix(Hud.Screen,Root.SourceId+"::native-scrollbar");
  DrawStatic(Hud.Screen,Root);UOpusTalentBridge::Scroll(Hud.Screen,Root.SourceId);
  auto Scroll=Find(Root);if(Scroll==nullptr)return;
  Scroll.SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);Scroll.SetAllowOverscroll(false);Scroll.SetAnimateWheelScrolling(true);
  if(UOpusNative::Flag("DWOAttributeScrollTest")&&UOpusNative::Flag("DWOTransient")){auto Test=UDWOAttributeScrollTest::Create(Hud.Tower);Test.Initialize(Hud,Root,Scroll);}
 }
}
