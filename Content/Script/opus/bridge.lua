-- UI-free command facade. All gameplay state and tower policy live in AngelScript.
local M = { contract_version = 1 }
function M.rules(context) return UE.UDWRuleWorldSubsystem.ForContext(context) end
function M.challenge(tower, floor) return tower:ChallengeFloor(floor) end
function M.retry(tower) return tower:Retry() end
function M.pause(tower, paused) return tower:SetPaused(paused) end
function M.start(tower) return tower:StartChallenge() end
function M.cast(hero, origin, target, power) return hero:CastAt(origin, target, power or 30) end
function M.skill(context, slot) return M.rules(context):SelectPlayerSkill(slot) end
function M.equip(context, slot, item) return M.rules(context):EquipPlayerItem(slot, item) end
function M.use(context, item) return M.rules(context):UsePlayerItem(item) end
function M.craft(context, uid, currency, omen, index) return M.rules(context):CraftPlayerEquipment(uid, currency, omen, index or 0) end
function M.pickup(context, key, position) return UE.UOpusNative.Pickup(M.rules(context), key, position) end
function M.merchant(context, key) return UE.UOpusNative.BuyMerchant(M.rules(context), key) end
return M
