"""Apply only narrowly anchored pending changes after the shared verification window."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
pending={}
def edit(file,pairs):
 p=r/file;s=p.read_text(encoding='utf-8')
 for old,new in pairs:
  assert s.count(old)==1,(file,old[:90],s.count(old))
  s=s.replace(old,new)
 pending[p]=s
base='Plugins/DWRules/Source/DWRulesCore/'
edit(base+'Public/DWRuleTypes.h', [('SpawnMonster = 17','SpawnMonster = 17,\n        MultiplyDamage = 18')])
edit(base+'Public/DWRuleSession.h',[('int32 Selected = 0;','int32 Selected = 0;\n    uint64 LoadoutRevision = 0;')])
edit(base+'Private/DWRuleSession.cpp',[('return Select(Selected, Error);','const bool Ok=Select(Selected,Error);if(Ok)++LoadoutRevision;return Ok;')])
edit(base+'Public/DWRuleWorld.h', [('uint64 Identity = 0;','uint64 Identity = 0;\n        float DamageMultiplier = 1.f;')])
edit(base+'Private/DWRuleWorld.cpp', [
 ('Root.Identity = NextRootIdentity++;','Root.Identity = NextRootIdentity++;\n        Root.DamageMultiplier = 1.f;'),
 ('const float Effective = EffectiveResistance(Target, Element, float(Stats.Get(ElementStatId(Element, EElementStat::Penetration)) + Mod.Penetration));','Raw *= Root.DamageMultiplier;\n        const float Effective = EffectiveResistance(Target, Element, float(Stats.Get(ElementStatId(Element, EElementStat::Penetration)) + Mod.Penetration));'),
 ('\t\treturn Hash;','        for(const auto& Root:Roots)if(Root.bActive&&Root.DamageMultiplier!=1.f)Hash=Mix64(Hash^Root.Seed^uint64(uint32(FMath::RoundToInt32(Root.DamageMultiplier*1000.f))));\n\t\treturn Hash;')])
edit('Plugins/DWRules/Source/DWRules/Private/DWRuleProgramReader.cpp', [(' { TEXT("spawn_monster"), EOp::SpawnMonster }',' { TEXT("spawn_monster"), EOp::SpawnMonster },\n                { TEXT("multiply_damage"), EOp::MultiplyDamage }')])
edit(base+'Private/DWRuleVM.cpp', [
 ('case EOp::ConsumeStatus:return RunConsumeStatus(Action,Context,Depth);','case EOp::MultiplyDamage:\n        {const float Factor=SlotOr(Action,ESlot::Value,Context,1);if(!FMath::IsFinite(Factor)||Factor<0)return false;Context.Root.DamageMultiplier=FMath::Clamp(Context.Root.DamageMultiplier*Factor,0.f,1.e6f);return true;}\n        case EOp::ConsumeStatus:return RunConsumeStatus(Action,Context,Depth);'),
 ('const bool Arc = (Action.Flags & FDWActionDef::Parabolic) != 0;','const FVector3f OffsetAim=Action.Origin==EOrigin::Owner?Owner.Aim:FlatDirection(Event.Direction,Owner.Aim);\n        Origin+=OffsetAim*SlotOr(Action,ESlot::OffsetForward,Context,0)+FVector3f::UpVector*SlotOr(Action,ESlot::OffsetUp,Context,0);\n        const bool Arc = (Action.Flags & FDWActionDef::Parabolic) != 0;'),
 ('            ShotOrigin+=Forward*SlotOr(Action,ESlot::OffsetForward,Context,0)+FVector3f::UpVector*SlotOr(Action,ESlot::OffsetUp,Context,0);\n',''),
 ('FVector2f(Origin.X, Origin.Y))','FVector2f(ShotOrigin.X, ShotOrigin.Y))'),
 ('(EndZ - Origin.Z + 4 * Height) / Duration','(EndZ - ShotOrigin.Z + 4 * Height) / Duration')])
edit(base+'Private/DWRuleCombatSkills.cpp', [('auto* From=RootInfo(Source);auto* To=RootInfo(Root,true);','World.Roots[Root].DamageMultiplier=World.Roots[Source].DamageMultiplier;\n    auto* From=RootInfo(Source);auto* To=RootInfo(Root,true);')])
edit(base+'Private/DWRuleModernCombat.cpp', [
 ('if(W.GetPlan(W.GetRoot(C.Root[I]).Plan).KindNames[C.Kind[I]]!=TEXT("treasury_weapon"))return;','const FName Kind=W.GetPlan(W.GetRoot(C.Root[I]).Plan).KindNames[C.Kind[I]];if(Kind!=TEXT("treasury_weapon")&&Kind!=TEXT("treasury_ea"))return;'),
 ('E.Kind=TEXT("__treasury_gate");','E.Kind=Kind==TEXT("treasury_ea")?TEXT("__treasury_ea_gate"):TEXT("__treasury_gate");')])
edit(base+'Private/DWRuleCombat.cpp', [('\treturn Projectiles->OnEnd(Index, Reason);','''    const bool Keep=Projectiles->OnEnd(Index,Reason);
    if(!Keep){const auto& C=World.Carriers;if(World.GetPlan(World.GetRoot(C.Root[Index]).Plan).KindNames[C.Kind[Index]]==TEXT("treasury_ea"))
    {if(Effects.Num()>=2048)Effects.RemoveAt(0);auto& E=Effects.AddDefaulted_GetRef();E.Kind=TEXT("__treasury_ea_impact");E.Position=FVector3d(C.Position[Index]);E.Position.Z=FMath::Max(.05,E.Position.Z);E.Element=3;E.Radius=3.5;}}
    return Keep;''')])
edit('Source/DungeonWorldOpus/Public/OpusRecentGameplay.h', [('UFUNCTION(BlueprintPure) static bool SelfTarget(UDWRuleWorldSubsystem* Rules);','UFUNCTION(BlueprintPure) static bool SelfTarget(UDWRuleWorldSubsystem* Rules);\n UFUNCTION(BlueprintPure) static bool PointTarget(UDWRuleWorldSubsystem* Rules);')])
edit('Plugins/DWRules/Source/DWRules/Public/DWWebVFXComponent.h',[('Clock = -1, Fade = -1;','Clock = -1, Fade = -1, BillboardScale = -1;')])
edit('Plugins/DWRules/Source/DWRules/Private/WebVFX/DWWebVFXComponent.cpp',[('Scale*Placement.GetScale3D()).ToMatrixWithScale();','Scale*(Parameters.BillboardScale>=0?FVector(Parameters.BillboardScale):Placement.GetScale3D())).ToMatrixWithScale();')])
edit('Source/DungeonWorldOpus/Private/OpusRecentGameplay.cpp', [(''' const auto* Session=Rules->PlayerSession();TArray<FString> GearKeys;
 for(const auto& E:Session->Loadout.Equipment)GearKeys.Add(E.Key.ToString()+TEXT("=")+E.Value.ToString());GearKeys.Sort();
 FString Key=FString::Join(GearKeys,TEXT(";"));for(const auto& S:Session->Skills)Key+=TEXT("|")+FString::FromInt(S.Plan);''',''' const auto* Session=Rules->PlayerSession();const FString Key=FString::Printf(TEXT("%llu"),static_cast<unsigned long long>(Session->LoadoutRevision));'''),(''' const auto* S=Rules?Rules->PlayerSession():nullptr;if(!S||S->Combat.Hero.Plan<0)return false;
 const auto& P=S->Host.World.GetPlan(S->Combat.Hero.Plan);const int32 Cast=P.FindEvent(TEXT("cast"));
 for(const auto& R:P.Rules)if(R.Event==Cast)for(int32 I=R.ActionFirst;I<R.ActionFirst+R.ActionCount;++I)
 {const auto& A=P.Actions[I];if(A.Op==EOp::MaintainEmitter||(A.Op==EOp::ApplyStatus&&A.Scope==EScope::Owner))return true;}return false;''',''' const auto* S=Rules?Rules->PlayerSession():nullptr;if(!S||S->Combat.Hero.Plan<0)return false;
 const auto& P=S->Host.World.GetPlan(S->Combat.Hero.Plan);const int32 Cast=P.FindEvent(TEXT("cast"));
 FString Active=P.ActiveId.ToString();int32 Suffix;if(Active.FindChar('~',Suffix))Active.LeftInline(Suffix);
 for(const auto& R:P.Rules)if(R.Event==Cast)
 {
  bool Match=true;for(int32 I=R.ConditionFirst;I<R.ConditionFirst+R.ConditionCount;++I){const auto& C=P.Conditions[I];if(C.Kind==ECond::Active&&(C.Compare==ECompare::Ne?FName(*Active)==C.Active:FName(*Active)!=C.Active))Match=false;}if(!Match)continue;
  for(int32 I=R.ActionFirst;I<R.ActionFirst+R.ActionCount;++I){const auto& A=P.Actions[I];if(A.Op==EOp::MaintainEmitter||(A.Op==EOp::ApplyStatus&&A.Origin==EOrigin::Owner))return true;}
 }return false;'''),('double UOpusRecentGameplay::SkillCooldown','''bool UOpusRecentGameplay::PointTarget(UDWRuleWorldSubsystem* Rules)
{const auto* S=Rules?Rules->PlayerSession():nullptr;if(!S||S->Combat.Hero.Plan<0)return false;for(const auto& A:S->Host.World.GetPlan(S->Combat.Hero.Plan).Actions)if(A.Op==EOp::Emit&&(A.Flags&FDWActionDef::Parabolic))return true;return false;}
double UOpusRecentGameplay::SkillCooldown''')])
if __name__=='__main__':
 import sys
 if '--check' not in sys.argv:
  for p,s in pending.items():p.write_text(s,encoding='utf-8')
 print('Narrow core continuation '+('checked' if '--check' in sys.argv else 'applied')+' '+str(len(pending))+' files')
