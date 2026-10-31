// Transient visual check of the production PNG loader and saved UICreator rows.
class UDWOIconCompletionTest : UActorComponent
{
 UPROPERTY() UDWOSvgHUD Hud;
 UPROPERTY() TArray<UOpusWorldScreen> Rows;
 TArray<FString> Ids,Kinds,Names;
 double Clock=0,Age=0;int Stage=0;bool Done=false;
 void Initialize(UDWOSvgHUD H)
 {
  Hud=H;
  Ids.Add("whirlwind");Kinds.Add("skill");Names.Add("旋风");
  Ids.Add("ember_hound");Kinds.Add("skill");Names.Add("召唤赤焰战犬");
  Ids.Add("fire_turret");Kinds.Add("skill");Names.Add("召唤火球炮台");
  Ids.Add("iron_guard");Kinds.Add("skill");Names.Add("召唤铁刃守卫");
  Ids.Add("lightning_nova");Kinds.Add("skill");Names.Add("雷霆新星");
  Ids.Add("storm_totem");Kinds.Add("skill");Names.Add("召唤引雷图腾");
  Ids.Add("aura_armour");Kinds.Add("skill");Names.Add("护甲光环");
  Ids.Add("aura_evasion");Kinds.Add("skill");Names.Add("闪避光环");
  Ids.Add("aura_fire");Kinds.Add("skill");Names.Add("火焰光环");
  Ids.Add("aura_frost");Kinds.Add("skill");Names.Add("冰霜光环");
  Ids.Add("aura_lightning");Kinds.Add("skill");Names.Add("闪电光环");
  Ids.Add("aura_physical");Kinds.Add("skill");Names.Add("物理光环");
  Ids.Add("aura_poison");Kinds.Add("skill");Names.Add("毒素光环");
  Ids.Add("aura_resist_all");Kinds.Add("skill");Names.Add("全抗光环");
  Ids.Add("aura_resist_fire");Kinds.Add("skill");Names.Add("火焰抗性光环");
  Ids.Add("aura_resist_frost");Kinds.Add("skill");Names.Add("冰霜抗性光环");
  Ids.Add("aura_resist_lightning");Kinds.Add("skill");Names.Add("闪电抗性光环");
  Ids.Add("aura_resist_poison");Kinds.Add("skill");Names.Add("毒素抗性光环");
  Ids.Add("aura_shield");Kinds.Add("skill");Names.Add("护盾光环");
  Ids.Add("aura_void");Kinds.Add("skill");Names.Add("虚空光环");
  Ids.Add("echo");Kinds.Add("skill");Names.Add("回响");
  Ids.Add("monster_twisted_eye_bolt");Kinds.Add("skill");Names.Add("扭曲之眼飞弹");
  Ids.Add("royal_treasury");Kinds.Add("skill");Names.Add("王之宝库");
  Ids.Add("sacrifice");Kinds.Add("skill");Names.Add("献祭");
  Ids.Add("summon_twisted_eyes");Kinds.Add("skill");Names.Add("触发－召唤扭曲之眼");
  Ids.Add("treasury_enuma_elish");Kinds.Add("skill");Names.Add("天地乖离剑");
  Ids.Add("vector_control");Kinds.Add("skill");Names.Add("矢量操控");
  Ids.Add("spirit_stone_mid");Kinds.Add("item");Names.Add("中品灵石");
  Ids.Add("spirit_stone_high");Kinds.Add("item");Names.Add("上品灵石");
  Ids.Add("annulment_crystal");Kinds.Add("item");Names.Add("无效结晶");
  Ids.Add("wish_crystal");Kinds.Add("item");Names.Add("祈愿结晶");
  Ids.Add("map");Kinds.Add("item");Names.Add("地牢地图");
 }
 bool Check(bool Good,FString What)
 {if(Good)return true;Done=true;UOpusNative::WriteLog("ICON_NATIVE_FAIL "+What);UOpusNative::Quit(false);return false;}
 UFUNCTION(BlueprintOverride) void Tick(float Dt)
 {
  if(Done||Hud==nullptr||Hud.Tower==nullptr)return;Clock+=Dt;Age+=Dt;
  if(Clock>90){Check(false,"timeout");return;}if(Age<1||Hud.Screen==nullptr)return;
  if(Stage==0)
  {
   Hud.Tower.Hero.ControlProbe=true;
   for(int I=0;I<Ids.Num();I++)
   {
    auto W=UOpusWorldUI::CreateTemplate(Gameplay::GetPlayerController(0),"recent-item-row","UICreatorRecent",180);
    if(!Check(W!=nullptr,"saved UICreator row "+Ids[I]))return;
    UOpusUI::SetImage(W,"row:icon",FPaths::ProjectContentDir()+"OpusIcons/"+Kinds[I]+"/"+Ids[I]+".png");
    UOpusUI::SetText(W,"row:item-name",Names[I]);UOpusUI::SetText(W,"row:item-meta",Ids[I]);
    UOpusHoverUI::Place(W,FVector2D(34+(I%4)*385,95+(I/4)*88),.6f);Rows.Add(W);
    auto N=W.FindSourceNode("recent-item-icon");bool Loaded=false;
    if(N!=nullptr)for(auto Child:N.GetAllChildren()){auto Image=Cast<UImage>(Child);if(Image!=nullptr&&Image.IsVisible())Loaded=true;}
    if(!Check(Loaded,"production PNG import "+Ids[I]))return;
    UOpusNative::WriteLog("ICON_NATIVE_PASS "+Kinds[I]+":"+Ids[I]);
   }
   Stage=1;Age=0;return;
  }
  if(Stage==1)
  {
   for(auto R:Rows)if(!UOpusWorldUI::VisualReady(R))return;
   System::ExecuteConsoleCommand("Shot ShowUI filename="+FPaths::ProjectSavedDir()+"Verification/IconCompletion/native-gallery.png -nosuffix");
   Stage=2;Age=0;return;
  }
  Done=true;UOpusNative::WriteLog("ICON_NATIVE_DONE count="+Rows.Num());UOpusNative::Quit(true);
 }
}
