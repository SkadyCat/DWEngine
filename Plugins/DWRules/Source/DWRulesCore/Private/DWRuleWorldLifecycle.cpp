#include "DWRuleWorld.h"

namespace DWRules
{
void FDWRuleWorld::RetireOwner(int32 Owner)
{
	if (Owners.IsValidIndex(Owner) && !FreeOwners.Contains(Owner))
		RetiredOwners.Add(Owner);
}
void FDWRuleWorld::ReleaseOwners()
{
	if (RetiredOwners.IsEmpty())
		return;
	TSet<int32> Busy;
	for (const auto &R : Roots)
		if (R.bActive)
			Busy.Add(R.Owner);
	for (const auto &Pair : Statuses)
		if (Pair.Value.Expire > Clock)
			Busy.Add(Pair.Value.Owner);
	for (auto It = RetiredOwners.CreateIterator(); It; ++It)
		if (!Busy.Contains(*It))
		{
			if (const auto *Keys = OwnerStateKeys.Find(*It))
				for (uint64 Key : *Keys)
					OwnerState.Remove(Key);
			OwnerStateKeys.Remove(*It);
			FreeOwners.Add(*It);
			It.RemoveCurrent();
		}
}
} // namespace DWRules
