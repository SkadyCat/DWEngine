# 风之息 / Wind's Breath

Skill ID: `wind_breath`. Physical aura; granted by `wind_breath_ring` or `wind_breath_amulet` and assigned/toggled through the existing Q/E/F/G aura UI. Uses the existing 50% mana reservation contract.

On each landed non-DOT attack, snapshot the attacker's damage **before the player's resistance, armour, block, shield or minion transfer**. A wind pulse expands in all directions from the hit location to 5 metres over 0.5 seconds. Each hostile target can be hit once per pulse for 80% of that original damage. This reflected physical damage cannot crit, receive the player's attack bonuses or recursively generate attack effects. Target defenses still apply. Evades, immunity, DOT, disabled/unequipped auras and a dead player do not create a pulse. Independent hits create independent pulses; no additional cooldown.

Composition: `wind_breath` declares the aura/reservation; `hit_retaliation_nova` declares percent/radius/expansion speed. AngelScript owns activation, expanding-front timing and per-pulse hit sets. Native adapters expose original hit metadata and deliver reflected hits through the existing rule world. Existing damage formulas remain unchanged for ordinary attacks.

Range, speed and reservation follow current neighboring skill conventions; the human explicitly selected original attacker damage and omnidirectional surrounding targets.
