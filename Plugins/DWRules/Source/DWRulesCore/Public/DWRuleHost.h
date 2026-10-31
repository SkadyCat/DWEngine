#pragma once

#include "DWRuleLibrary.h"

namespace DWRules
{
struct FDWHeldGrant
{
	int32 Id = INDEX_NONE;
	uint32 Target = InvalidId;
	TArray<FDWTraitSource> Sources;
	float Expire = 0.f;
	FString Label;
	FString Key;
};

// Engine-independent host: library composition, plan cache and timed held traits.
// UObject/Mass/file adapters delegate here; the VM never knows about inventories or JSON.
class DWRULESCORE_API FDWRuleHost
{
  public:
	explicit FDWRuleHost(const FDWWorldConfig &Config = FDWWorldConfig());
	FDWRuleWorld World;
	FDWRuleLibrary Library;
	bool SetLibrary(FDWRuleLibrary InLibrary, FString &Error);
	int32 CompileSource(const FDWBuildSource &Source, FString &Error);
	int32 CompileTraits(FName Active, const TArray<FName> &Ids, FString &Error);
	int32 CompileNamedBuild(FName Build, const TArray<FName> &Extras, FString &Error);
	int32 PlanWithSources(int32 Plan, const TArray<FDWTraitSource> &Held, FString &Error);
	int32 PlanWithGrantedTraits(int32 Plan, int32 Owner, FString &Error);
	int32 GrantTraits(uint32 Target, const TArray<FDWTraitEntry> &Entries, float Duration, const FString &Label,
					  FString &Error);
	int32 GrantSources(uint32 Target, const TArray<FDWTraitSource> &Sources, float Duration,
					   const FString &Label = FString());
	void RevokeGrant(int32 Id);
	void HeldSources(uint32 Target, TArray<FDWTraitSource> &Out) const;
	const TArray<FDWHeldGrant> &Grants() const
	{
		return Held;
	}
	bool SetTargetDefenseTraits(uint32 Target, const TArray<FName> &Ids, FString &Error);
	bool SetTargetDefenseSources(uint32 Target, const TArray<FDWTraitSource>& Sources, FString& Error);
	void RefreshGrantedDefense();
	void ForgetTarget(uint32 Target);
	void Step(float DeltaSeconds);
	const FDWBuildSource *PlanSource(int32 Plan) const
	{
		return PlanSources.Find(Plan);
	}

  private:
	void GrantFromRule(uint32 Target, int32 Owner, float Duration, const FDWGrantDef &Grant);
	bool BindDefense(uint32 Target, const TArray<FDWTraitSource> &Sources, FString &Error);
	TMap<uint64, int32> PlanByHash;
	TMap<int32, FDWBuildSource> PlanSources;
	TMap<FString, int32> GrantedPlans;
	TMap<uint32, TArray<FDWTraitSource>> BaseDefense;
	TMap<uint32, FString> AppliedDefense;
	TArray<FDWHeldGrant> Held;
	int32 NextGrant = 1;
};
} // namespace DWRules
