struct FC3Model
{
 FString Id,Path,LeftFoot,RightFoot,Pelvis;
 TMap<FName,FString> Clips;
 double Scale=1,Gait=350;
}
namespace C3Models
{
 TArray<FC3Model> All()
 {
  TArray<FC3Model> Out;
  {FC3Model M;M.Id="cloth_grunt";M.Path="/Game/Monsters/MeshySkeletons/cloth_grunt/Meshes/SK_cloth_grunt";M.Scale=1.0;M.Gait=278.13773932847766;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"attack1","/Game/Monsters/MeshySkeletons/cloth_grunt/Animation/FootContact/AS_attack1_FootContact");
   M.Clips.Add(n"attack2","/Game/Monsters/MeshySkeletons/cloth_grunt/Animation/FootContact/AS_attack2_FootContact");
   M.Clips.Add(n"death","/Game/Monsters/MeshySkeletons/cloth_grunt/Animation/Clips/AS_death");
   M.Clips.Add(n"hit","/Game/Monsters/MeshySkeletons/cloth_grunt/Animation/FootContact/AS_hit_FootContact");
   M.Clips.Add(n"idle","/Game/Monsters/MeshySkeletons/cloth_grunt/Animation/FootContact/AS_idle_FootContact");
   M.Clips.Add(n"run","/Game/Monsters/MeshySkeletons/cloth_grunt/Animation/FootContact/AS_run_FootContact");
   M.Clips.Add(n"walk","/Game/Monsters/MeshySkeletons/cloth_grunt/Animation/FootContact/AS_walk_FootContact");
   Out.Add(M);}
  {FC3Model M;M.Id="goblin_archer";M.Path="/Game/Monsters/MeshyGoblins/archer/Meshes/SK_archer";M.Scale=1.0;M.Gait=215.90260653342892;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"attack1","/Game/Monsters/MeshyGoblins/archer/Animation/FootContact/AS_attack1_FootContact");
   M.Clips.Add(n"attack2","/Game/Monsters/MeshyGoblins/archer/Animation/FootContact/AS_attack2_FootContact");
   M.Clips.Add(n"death","/Game/Monsters/MeshyGoblins/archer/Animation/Clips/AS_death");
   M.Clips.Add(n"hit","/Game/Monsters/MeshyGoblins/archer/Animation/FootContact/AS_hit_FootContact");
   M.Clips.Add(n"idle","/Game/Monsters/MeshyGoblins/archer/Animation/FootContact/AS_idle_FootContact");
   M.Clips.Add(n"run","/Game/Monsters/MeshyGoblins/archer/Animation/FootContact/AS_run_FootContact");
   M.Clips.Add(n"walk","/Game/Monsters/MeshyGoblins/archer/Animation/FootContact/AS_walk_FootContact");
   Out.Add(M);}
  {FC3Model M;M.Id="goblin_dagger_scout";M.Path="/Game/Monsters/MeshyGoblins/dagger_scout/Meshes/SK_dagger_scout";M.Scale=1.0;M.Gait=239.9257385741809;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"attack1","/Game/Monsters/MeshyGoblins/dagger_scout/Animation/FootContact/AS_attack1_FootContact");
   M.Clips.Add(n"attack2","/Game/Monsters/MeshyGoblins/dagger_scout/Animation/FootContact/AS_attack2_FootContact");
   M.Clips.Add(n"death","/Game/Monsters/MeshyGoblins/dagger_scout/Animation/Clips/AS_death");
   M.Clips.Add(n"hit","/Game/Monsters/MeshyGoblins/dagger_scout/Animation/FootContact/AS_hit_FootContact");
   M.Clips.Add(n"idle","/Game/Monsters/MeshyGoblins/dagger_scout/Animation/FootContact/AS_idle_FootContact");
   M.Clips.Add(n"run","/Game/Monsters/MeshyGoblins/dagger_scout/Animation/FootContact/AS_run_FootContact");
   M.Clips.Add(n"walk","/Game/Monsters/MeshyGoblins/dagger_scout/Animation/FootContact/AS_walk_FootContact");
   Out.Add(M);}
  {FC3Model M;M.Id="goblin_hammer_chief";M.Path="/Game/Monsters/MeshyGoblins/hammer_chief/Meshes/SK_hammer_chief";M.Scale=1.0;M.Gait=251.92514524720391;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"attack1","/Game/Monsters/MeshyGoblins/hammer_chief/Animation/FootContact/AS_attack1_FootContact");
   M.Clips.Add(n"attack2","/Game/Monsters/MeshyGoblins/hammer_chief/Animation/FootContact/AS_attack2_FootContact");
   M.Clips.Add(n"death","/Game/Monsters/MeshyGoblins/hammer_chief/Animation/Clips/AS_death");
   M.Clips.Add(n"hit","/Game/Monsters/MeshyGoblins/hammer_chief/Animation/FootContact/AS_hit_FootContact");
   M.Clips.Add(n"idle","/Game/Monsters/MeshyGoblins/hammer_chief/Animation/FootContact/AS_idle_FootContact");
   M.Clips.Add(n"run","/Game/Monsters/MeshyGoblins/hammer_chief/Animation/FootContact/AS_run_FootContact");
   M.Clips.Add(n"walk","/Game/Monsters/MeshyGoblins/hammer_chief/Animation/FootContact/AS_walk_FootContact");
   Out.Add(M);}
  {FC3Model M;M.Id="goblin_laborer";M.Path="/Game/Monsters/MeshyGoblins/laborer/Meshes/SK_laborer";M.Scale=1.0;M.Gait=195.530754394371;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"attack1","/Game/Monsters/MeshyGoblins/laborer/Animation/FootContact/AS_attack1_FootContact");
   M.Clips.Add(n"attack2","/Game/Monsters/MeshyGoblins/laborer/Animation/FootContact/AS_attack2_FootContact");
   M.Clips.Add(n"death","/Game/Monsters/MeshyGoblins/laborer/Animation/Clips/AS_death");
   M.Clips.Add(n"hit","/Game/Monsters/MeshyGoblins/laborer/Animation/FootContact/AS_hit_FootContact");
   M.Clips.Add(n"idle","/Game/Monsters/MeshyGoblins/laborer/Animation/FootContact/AS_idle_FootContact");
   M.Clips.Add(n"run","/Game/Monsters/MeshyGoblins/laborer/Animation/FootContact/AS_run_FootContact");
   M.Clips.Add(n"walk","/Game/Monsters/MeshyGoblins/laborer/Animation/FootContact/AS_walk_FootContact");
   Out.Add(M);}
  {FC3Model M;M.Id="goblin_shaman";M.Path="/Game/Monsters/MeshyGoblins/shaman/Meshes/SK_shaman";M.Scale=1.0;M.Gait=242.120701937069;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"attack1","/Game/Monsters/MeshyGoblins/shaman/Animation/FootContact/AS_attack1_FootContact");
   M.Clips.Add(n"attack2","/Game/Monsters/MeshyGoblins/shaman/Animation/FootContact/AS_attack2_FootContact");
   M.Clips.Add(n"death","/Game/Monsters/MeshyGoblins/shaman/Animation/Clips/AS_death");
   M.Clips.Add(n"hit","/Game/Monsters/MeshyGoblins/shaman/Animation/FootContact/AS_hit_FootContact");
   M.Clips.Add(n"idle","/Game/Monsters/MeshyGoblins/shaman/Animation/FootContact/AS_idle_FootContact");
   M.Clips.Add(n"run","/Game/Monsters/MeshyGoblins/shaman/Animation/FootContact/AS_run_FootContact");
   M.Clips.Add(n"walk","/Game/Monsters/MeshyGoblins/shaman/Animation/FootContact/AS_walk_FootContact");
   Out.Add(M);}
  {FC3Model M;M.Id="goblin_shield_trooper";M.Path="/Game/Monsters/MeshyGoblins/shield_trooper/Meshes/SK_shield_trooper";M.Scale=1.0;M.Gait=220.46294293607855;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"attack1","/Game/Monsters/MeshyGoblins/shield_trooper/Animation/FootContact/AS_attack1_FootContact");
   M.Clips.Add(n"attack2","/Game/Monsters/MeshyGoblins/shield_trooper/Animation/FootContact/AS_attack2_FootContact");
   M.Clips.Add(n"death","/Game/Monsters/MeshyGoblins/shield_trooper/Animation/Clips/AS_death");
   M.Clips.Add(n"hit","/Game/Monsters/MeshyGoblins/shield_trooper/Animation/FootContact/AS_hit_FootContact");
   M.Clips.Add(n"idle","/Game/Monsters/MeshyGoblins/shield_trooper/Animation/FootContact/AS_idle_FootContact");
   M.Clips.Add(n"run","/Game/Monsters/MeshyGoblins/shield_trooper/Animation/FootContact/AS_run_FootContact");
   M.Clips.Add(n"walk","/Game/Monsters/MeshyGoblins/shield_trooper/Animation/FootContact/AS_walk_FootContact");
   Out.Add(M);}
  {FC3Model M;M.Id="rust_warrior";M.Path="/Game/Monsters/MeshySkeletons/rust_warrior/Meshes/SK_rust_warrior";M.Scale=1.0;M.Gait=294.5339710334842;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"attack1","/Game/Monsters/MeshySkeletons/rust_warrior/Animation/FootContact/AS_attack1_FootContact");
   M.Clips.Add(n"attack2","/Game/Monsters/MeshySkeletons/rust_warrior/Animation/FootContact/AS_attack2_FootContact");
   M.Clips.Add(n"death","/Game/Monsters/MeshySkeletons/rust_warrior/Animation/Clips/AS_death");
   M.Clips.Add(n"hit","/Game/Monsters/MeshySkeletons/rust_warrior/Animation/FootContact/AS_hit_FootContact");
   M.Clips.Add(n"idle","/Game/Monsters/MeshySkeletons/rust_warrior/Animation/FootContact/AS_idle_FootContact");
   M.Clips.Add(n"run","/Game/Monsters/MeshySkeletons/rust_warrior/Animation/FootContact/AS_run_FootContact");
   M.Clips.Add(n"walk","/Game/Monsters/MeshySkeletons/rust_warrior/Animation/FootContact/AS_walk_FootContact");
   Out.Add(M);}
  {FC3Model M;M.Id="archer";M.Path="/Game/Monsters/Mass/Instanced/Meshes/SK_MassGPU_archer";M.Scale=1.0;M.Gait=350.0;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"idle","/Game/Skeleton_Collection/Skeleton_Archer/Animations/AS_idleAim");
   M.Clips.Add(n"run","/Game/Skeleton_Collection/Skeleton_Archer/Animations/AS_run_f");
   M.Clips.Add(n"attack1","/Game/Skeleton_Collection/Skeleton_Archer/Animations/AS_idleAim_shot");
   M.Clips.Add(n"hit","/Game/Skeleton_Collection/Skeleton_Archer/Animations/AS_GetDamage");
   M.Clips.Add(n"death","/Game/Skeleton_Collection/Skeleton_Archer/Animations/AS_death");
   Out.Add(M);}
  {FC3Model M;M.Id="crossbowman";M.Path="/Game/Monsters/Mass/Instanced/Meshes/SK_MassGPU_crossbowman";M.Scale=1.0;M.Gait=350.0;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"idle","/Game/Skeleton_Collection/Skeleton_Crossbowman/Animations/AS_idleAim");
   M.Clips.Add(n"run","/Game/Skeleton_Collection/Skeleton_Crossbowman/Animations/AS_run_f");
   M.Clips.Add(n"attack1","/Game/Skeleton_Collection/Skeleton_Crossbowman/Animations/AS_Shot");
   M.Clips.Add(n"hit","/Game/Skeleton_Collection/Skeleton_Crossbowman/Animations/AS_GetDamage");
   M.Clips.Add(n"death","/Game/Skeleton_Collection/Skeleton_Crossbowman/Animations/AS_death");
   Out.Add(M);}
  {FC3Model M;M.Id="mage";M.Path="/Game/Monsters/Mass/Instanced/Meshes/SK_MassGPU_mage";M.Scale=1.0;M.Gait=150.0;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"idle","/Game/Skeleton_Collection/Skeleton_Mage/Animations/AS_idle_combat");
   M.Clips.Add(n"run","/Game/Skeleton_Collection/Skeleton_Mage/Animations/AS_combat_walk_f");
   M.Clips.Add(n"attack1","/Game/Skeleton_Collection/Skeleton_Mage/Animations/AS_attack1");
   M.Clips.Add(n"hit","/Game/Skeleton_Collection/Skeleton_Mage/Animations/AS_GetDamage");
   M.Clips.Add(n"death","/Game/Skeleton_Collection/Skeleton_Mage/Animations/AS_death");
   Out.Add(M);}
  {FC3Model M;M.Id="samurai";M.Path="/Game/Monsters/Mass/Instanced/Meshes/SK_MassGPU_samurai";M.Scale=1.0;M.Gait=350.0;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"idle","/Game/Skeleton_Collection/Skeleton_Samurai/Animations/AS_idle");
   M.Clips.Add(n"run","/Game/Skeleton_Collection/Skeleton_Samurai/Animations/AS_run_f");
   M.Clips.Add(n"attack1","/Game/Skeleton_Collection/Skeleton_Samurai/Animations/AS_attack_01");
   M.Clips.Add(n"hit","/Game/Skeleton_Collection/Skeleton_Samurai/Animations/AS_GetDamage");
   M.Clips.Add(n"death","/Game/Skeleton_Collection/Skeleton_Samurai/Animations/AS_death");
   Out.Add(M);}
  {FC3Model M;M.Id="spearman";M.Path="/Game/Monsters/Mass/Instanced/Meshes/SK_MassGPU_spearman";M.Scale=1.0;M.Gait=350.0;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"idle","/Game/Skeleton_Collection/Skeleton_Spearman/Animations/AS_idle");
   M.Clips.Add(n"run","/Game/Skeleton_Collection/Skeleton_Spearman/Animations/AS_run_f");
   M.Clips.Add(n"attack1","/Game/Skeleton_Collection/Skeleton_Spearman/Animations/AS_attack_01");
   M.Clips.Add(n"hit","/Game/Skeleton_Collection/Skeleton_Spearman/Animations/AS_GetDamage");
   M.Clips.Add(n"death","/Game/Skeleton_Collection/Skeleton_Spearman/Animations/AS_death");
   Out.Add(M);}
  {FC3Model M;M.Id="swordsman";M.Path="/Game/Monsters/Mass/Instanced/Meshes/SK_MassGPU_swordsman";M.Scale=1.0;M.Gait=350.0;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"idle","/Game/Skeleton_Collection/Skeleton_swordsman/Animations/AS_idle");
   M.Clips.Add(n"run","/Game/Skeleton_Collection/Skeleton_swordsman/Animations/AS_run");
   M.Clips.Add(n"attack1","/Game/Skeleton_Collection/Skeleton_swordsman/Animations/AS_attack01_part1");
   M.Clips.Add(n"hit","/Game/Skeleton_Collection/Skeleton_swordsman/Animations/AS_GetDamage");
   M.Clips.Add(n"death","/Game/Skeleton_Collection/Skeleton_swordsman/Animations/AS_death");
   Out.Add(M);}
  {FC3Model M;M.Id="axeman";M.Path="/Game/Monsters/Mass/Instanced/Meshes/SK_MassGPU_axeman";M.Scale=1.0;M.Gait=350.0;
   M.LeftFoot="foot_l";M.RightFoot="foot_r";M.Pelvis="pelvis";
   M.Clips.Add(n"idle","/Game/Skeleton_Collection/Skeleton_with_axe/Animations/AS_idle");
   M.Clips.Add(n"run","/Game/Skeleton_Collection/Skeleton_with_axe/Animations/AS_run_f");
   M.Clips.Add(n"attack1","/Game/Skeleton_Collection/Skeleton_with_axe/Animations/AS_attack_01");
   M.Clips.Add(n"hit","/Game/Skeleton_Collection/Skeleton_with_axe/Animations/AS_GetDamage");
   M.Clips.Add(n"death","/Game/Skeleton_Collection/Skeleton_with_axe/Animations/AS_death");
   Out.Add(M);}
  return Out;
 }
}
