class UDWOMerchantWidgetTest : UActorComponent
{
 UPROPERTY() ADWOTowerGameMode Tower;
 int Stage=0;double Age=0,Clock=0;bool Done=false;
 FVector Original;FVector4 Before;
 void Initialize(ADWOTowerGameMode T){Tower=T;}
 bool Check(bool Good,FString Detail){UOpusNative::WriteLog((Good?"MERCHANT_WIDGET_PASS ":"MERCHANT_WIDGET_FAIL ")+Detail);if(!Good){Done=true;UOpusNative::Quit(false);}return Good;}
 void Next(int N){Stage=N;Age=0;}
 void Shot(FString Name){System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/MerchantWidget/"+Name+".png -nosuffix");}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Tower==nullptr)return;Clock+=Dt;Age+=Dt;if(Clock>100){Check(false,"timeout stage="+Stage);return;}
  auto G=Tower.Studio;auto UI=G.UI;auto PC=Gameplay::GetPlayerController(0);
  if(!G.Ready||!G.MerchantReady||!IsValid(G.Merchant)||UI.MerchantTag==nullptr||!Tower.Dungeon.Ready||Age<.6)return;
  auto M=G.Merchant;
  if(Stage==0)
  {
   Tower.Hero.ControlProbe=true;
   if(!UOpusWorldUI::VisualReady(UI.MerchantTag))return;
   if(!Check(M.Bubble.GetOwner()==M&&M.Bubble.GetAttachParent()==M.RootComponent,"WidgetComponent belongs and attaches to merchant actor"))return;
   if(!Check(M.Bubble.GetWidget()==UI.MerchantTag&&!UI.MerchantTag.IsInViewport(),"saved UICreator widget is hosted only by component"))return;
   if(!Check(M.Bubble.GetWidgetSpace()==EWidgetSpace::Screen&&M.Bubble.GetDrawSize()==UI.MerchantTag.TemplateSize,"component preserves authored screen-space size"))return;
   Original=M.ActorLocation;Before=UOpusHoverUI::AnchorRect(UI.MerchantTag.GetRootWidget(),PC);Shot("attached");Next(1);return;
  }
  if(Stage==1){M.SetActorLocation(Original+FVector(280,80,0));Next(2);return;}
  if(Stage==2)
  {
   auto After=UOpusHoverUI::AnchorRect(UI.MerchantTag.GetRootWidget(),PC);
   if(!Check((M.Bubble.WorldLocation-M.ActorLocation-FVector(0,0,340)).Size()<.1,"component keeps its local anchor when actor moves"))return;
   if(!Check(Math::Abs(After.X-Before.X)+Math::Abs(After.Y-Before.Y)>8&&G.MerchantPosition==Original+FVector(0,0,60),"bubble follows actor without updating projected gameplay position"))return;
   Shot("moved");Next(3);return;
  }
  if(Stage==3){M.SetActorLocation(Original);Tower.SetPaused(true);Next(4);return;}
  if(Stage==4)
  {
   if(!Check(!M.Bubble.IsVisible()&&!UI.MerchantTag.IsVisible(),"pause hides component and disables bubble hit testing"))return;
   Tower.SetPaused(false);Next(5);return;
  }
  if(Stage==5)
  {
   if(!Check(M.Bubble.IsVisible()&&UI.MerchantTag.IsVisible(),"unpause restores component"))return;
   auto Input=DWOHoverTests::Input(UI.MerchantTag.FindSourceNode("merchant-launcher-merchant-open"));
   if(!Check(Input!=nullptr&&UOpusHoverUI::TestPointerButton(Input,EKeys::LeftMouseButton,true),"component bubble receives real Slate pointer down"))return;
   UOpusHoverUI::TestPointerButton(Input,EKeys::LeftMouseButton,false);
   if(!Check(UI.Panel!=nullptr&&UI.View=="merchant"&&Tower.Paused,"component click opens original merchant shop"))return;
   Next(6);return;
  }
  if(Stage==6)
  {
   if(!Check(!M.Bubble.IsVisible(),"shop hides component bubble"))return;
   Shot("shop");UI.Close();Next(7);return;
  }
  if(Stage==7)
  {
   if(!Check(UI.Panel==nullptr&&!Tower.Paused&&M.Bubble.IsVisible(),"closing shop restores component bubble"))return;
   Done=true;UOpusNative::WriteLog("MERCHANT_WIDGET_DONE");UOpusNative::Quit(true);
  }
 }
}
