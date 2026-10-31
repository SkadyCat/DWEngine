// Shared source notifies dispatch to per-character action state. Only the
// authoritative character may commit one DWRules cast at Attack Release.
class UCleanAttackReleaseNotify : UAnimNotify
{
 UFUNCTION(BlueprintOverride) FString GetNotifyName() const {return "Attack Release";}
 UFUNCTION(BlueprintOverride) bool Notify(USkeletalMeshComponent MeshComp,UAnimSequenceBase Animation,const FAnimNotifyEventReference& EventReference) const
 {
  if(MeshComp==nullptr)return false;auto Hero=Cast<ADWOHero>(MeshComp.GetOwner());
  if(Hero!=nullptr&&MeshComp==Hero.Mesh)Hero.Visual.HandleAttackRelease(Animation);
  return true;
 }
}
class UCleanActionEndNotify : UAnimNotify
{
 UFUNCTION(BlueprintOverride) FString GetNotifyName() const {return "Action End";}
 UFUNCTION(BlueprintOverride) bool Notify(USkeletalMeshComponent MeshComp,UAnimSequenceBase Animation,const FAnimNotifyEventReference& EventReference) const
 {
  if(MeshComp==nullptr)return false;auto Hero=Cast<ADWOHero>(MeshComp.GetOwner());
  if(Hero!=nullptr&&MeshComp==Hero.Mesh)Hero.Visual.HandleActionEnd(Animation);return true;
 }
}
