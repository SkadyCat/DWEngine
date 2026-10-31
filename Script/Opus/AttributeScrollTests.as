// Real saved HUD, native scrolling viewport and state changes; transient only.
class UDWOAttributeScrollTest : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() USvgCanvasNode Root;
 UPROPERTY() UScrollBox Scroll;
 int Stage=0,Frames=0;bool Done=false;float End=0;
 void Initialize(UDWOSvgHUD H,USvgCanvasNode R,UScrollBox S){Hud=H;Root=R;Scroll=S;}
 bool Check(bool Good,FString What){if(!Good){Done=true;UOpusNative::WriteLog("ATTRIBUTE_SCROLL_FAIL "+What);UOpusNative::Quit(false);}return Good;}
 UTextBlock Label(UWidget Widget){auto Text=Cast<UTextBlock>(Widget);if(Text!=nullptr)return Text;auto Panel=Cast<UPanelWidget>(Widget);if(Panel!=nullptr)for(auto Child:Panel.GetAllChildren()){auto Found=Label(Child);if(Found!=nullptr)return Found;}return nullptr;}
 void Capture(FString Name){FString Dir=UOpusNative::StringOption("DWOAttributeScrollProof");if(!Dir.IsEmpty())System::ExecuteConsoleCommand("Shot ShowUI filename="+Dir+"/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||++Frames<18)return;Frames=0;
  if(Stage==0)
  {
   End=Scroll.GetScrollOffsetOfEnd();if(!Check(End>500,"full authored content overflows the viewport"))return;
   auto Last=Hud.Screen.FindSourceNode("battle_live_modal_page::battle-stat-6-4-key");auto Text=Label(Last);
   if(!Check(Last!=nullptr&&Text!=nullptr&&Text.GetText().ToString()==Last.SourceText&&!Last.SourceText.IsEmpty(),"clipped bottom label has real native glyphs"))return;
   auto Bar=Hud.Screen.FindSourceNode(Root.SourceId+"::native-scrollbar");if(!Check(Bar!=nullptr&&Bar.GetVisibility()==ESlateVisibility::Collapsed,"decorative scrollbar is retired"))return;
   Capture("top");
  }
  else if(Stage==1)Scroll.SetScrollOffset(End);
  else if(Stage==2)
  {
   if(!Check(Math::Abs(Scroll.GetScrollOffset()-End)<1,"native viewport reaches the last attribute"))return;
   Capture("bottom");Hud.RefreshPanels();if(!Check(Math::Abs(Scroll.GetScrollOffset()-End)<1,"data refresh preserves scroll offset"))return;
  }
  else if(Stage==3)
  {
   Hud.Screen.ApplySceneState("selected-1");Hud.Screen.ApplySceneState("character");Hud.RefreshPanels();
   if(!Check(DWOAttributeScroll::Find(Root)==Scroll,"reopening reuses one native viewport"))return;
  }
  else if(Stage==4){if(!Check(Math::Abs(Scroll.GetScrollOffset()-End)<1,"navigation preserves offset and content layout"))return;Scroll.SetScrollOffset(0);}
  else if(Stage==5)
  {
   if(!Check(Scroll.GetScrollOffset()<1,"native viewport returns to the first attribute"))return;
   Done=true;UOpusNative::WriteLog("ATTRIBUTE_SCROLL_PASS NativeViewport end="+End);UOpusNative::Quit(true);return;
  }
  Stage++;
 }
}
