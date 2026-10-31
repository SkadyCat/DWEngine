// Map-generation configuration only. C++ Mass remains the sole monster AI.
struct FDWMonsterLineageMember
{
 UPROPERTY(EditAnywhere) FName Archetype;
 UPROPERTY(EditAnywhere, Meta=(ClampMin="0")) float Weight = 1;
}
struct FDWMonsterLineageDefinition
{
 UPROPERTY(EditAnywhere) FName Id;
 UPROPERTY(EditAnywhere) FString DisplayName;
 UPROPERTY(EditAnywhere) TArray<FDWMonsterLineageMember> Members;
}
struct FDWMonsterLineageChoice
{
 UPROPERTY(EditAnywhere) FName Lineage;
 UPROPERTY(EditAnywhere, Meta=(ClampMin="0")) float Weight = 1;
}
struct FDWMonsterLineageRule
{
 UPROPERTY(EditAnywhere) FName Id;
 // Weight when a map selects the reserved 'random' rule.
 UPROPERTY(EditAnywhere, Meta=(ClampMin="0")) float Weight = 1;
 // One entry = single lineage; several = a mixed population.
 UPROPERTY(EditAnywhere) TArray<FDWMonsterLineageChoice> Lineages;
}
struct FDWMonsterLineageMapPolicy
{
 UPROPERTY(EditAnywhere) FName MapId;
 UPROPERTY(EditAnywhere) FName RuleId = n"random";
 UPROPERTY(EditAnywhere) bool Enabled = true;
}
class UDWMonsterLineageSettings : UDataAsset
{
 UPROPERTY(EditAnywhere) bool Enabled = true;
 // Zero requests a fresh world seed. Positive values reproduce a population.
 UPROPERTY(EditAnywhere, Meta=(ClampMin="0")) int Seed = 0;
 UPROPERTY(EditAnywhere) FName DefaultRule = n"random";
 UPROPERTY(EditAnywhere) TArray<FDWMonsterLineageDefinition> Lineages;
 UPROPERTY(EditAnywhere) TArray<FDWMonsterLineageRule> Rules;
 UPROPERTY(EditAnywhere) TArray<FDWMonsterLineageMapPolicy> Maps;
#if EDITOR
 UFUNCTION(BlueprintCallable)
 void ClearConfiguration() { Lineages.Empty(); Rules.Empty(); Maps.Empty(); }
 UFUNCTION(BlueprintCallable)
 void AddLineage(FName Id, FString Name)
 {
  FDWMonsterLineageDefinition Row; Row.Id=Id; Row.DisplayName=Name; Lineages.Add(Row);
 }
 UFUNCTION(BlueprintCallable)
 void AddMember(FName Id, FName Archetype, float Weight)
 {
  for (FDWMonsterLineageDefinition& Row : Lineages)
  {
   if (Row.Id == Id) { FDWMonsterLineageMember Member; Member.Archetype=Archetype; Member.Weight=Weight; Row.Members.Add(Member); return; }
  }
 }
 UFUNCTION(BlueprintCallable)
 void AddRule(FName Id, float Weight)
 {
  FDWMonsterLineageRule Row; Row.Id=Id; Row.Weight=Weight; Rules.Add(Row);
 }
 UFUNCTION(BlueprintCallable)
 void AddRuleLineage(FName Id, FName Lineage, float Weight)
 {
  for (FDWMonsterLineageRule& Row : Rules)
  {
   if (Row.Id == Id) { FDWMonsterLineageChoice Choice; Choice.Lineage=Lineage; Choice.Weight=Weight; Row.Lineages.Add(Choice); return; }
  }
 }
 UFUNCTION(BlueprintCallable)
 void AddMap(FName MapId, FName RuleId, bool IsEnabled)
 {
  FDWMonsterLineageMapPolicy Row; Row.MapId=MapId; Row.RuleId=RuleId; Row.Enabled=IsEnabled; Maps.Add(Row);
 }
#endif
}
