// Generated from evaluated native foot-contact poses; cm/s at play rate 1.
namespace C3Gaits {
 double Speed(FString Model,FName State) {
  if(Model=="cloth_grunt")return State==n"walk"?95.212678748:286.078402417;
  if(Model=="goblin_archer")return State==n"walk"?72.939999126:196.289317336;
  if(Model=="goblin_dagger_scout")return State==n"walk"?73.544237214:197.101707355;
  if(Model=="goblin_hammer_chief")return State==n"walk"?81.337721982:219.148509255;
  if(Model=="goblin_laborer")return State==n"walk"?61.645582699:185.315744092;
  if(Model=="goblin_shaman")return State==n"walk"?70.944862375:196.525387268;
  if(Model=="goblin_shield_trooper")return State==n"walk"?64.801598064:189.951937162;
  if(Model=="rust_warrior")return State==n"walk"?102.324311242:307.426821224;
  if(Model=="archer")return State==n"walk"?334.428357598:334.428357598;
  if(Model=="crossbowman")return State==n"walk"?334.252602998:334.252602998;
  if(Model=="mage")return State==n"walk"?150.000000000:150.000000000;
  if(Model=="samurai")return State==n"walk"?334.255210445:334.255210445;
  if(Model=="spearman")return State==n"walk"?334.255214820:334.255214820;
  if(Model=="swordsman")return State==n"walk"?334.242585911:334.242585911;
  if(Model=="axeman")return State==n"walk"?334.242585911:334.242585911;
  return 300;
 }
 bool Striding(FString Model) {return Model!="mage";}
}
