// Preserve the original rule hit sphere at feet + 60 cm independently of
// Zero's physical capsule. No rule formula, radius or damage callback changes.
class ADWOHeroCombatTarget : AActor
{
 UPROPERTY(DefaultComponent,RootComponent) USceneComponent Root;
 UPROPERTY(DefaultComponent) UDWRuleTargetComponent RuleTarget;
 default RuleTarget.ExternalId=1;
 default RuleTarget.Team=0;
 default RuleTarget.MaxHealth=400;
 default RuleTarget.RadiusCm=45;
 UPROPERTY() ADWOHero Hero;
 UFUNCTION(BlueprintOverride) void BeginPlay(){SetTickGroup(ETickingGroup::TG_PostPhysics);}
 // Attachment follows the actual CMC transform; publish velocity for DWRules'
 // existing moving-player query, which reads its target actor's velocity.
 UFUNCTION(BlueprintOverride) void Tick(float DeltaSeconds){if(IsValid(Hero))UOpusNative::Velocity(this,Hero.GetVelocity());}
}
