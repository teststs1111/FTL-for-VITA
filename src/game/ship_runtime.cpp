    const int fallbackTargetRoom = projectileTargets.front();
    RuntimeWeapon& weapon = weapons[weaponIndex];
    if (!weapon.volleyPending || weapon.allocatedPower < weapon.power) return 0;
    if (weaponIndex < static_cast<int>(weaponIonDisabled.size()) && weaponIonDisabled[weaponIndex])
        return 0;

    // A volley contains the configured number of projectiles. Missile/bomb
    // ammunition was already consumed by fireWeapon(), so resolving the volley
    // never charges ammo again.
    const int shots = std::max(1, weapon.shots);
    const std::string type = weapon.type;
    int resolved = 0;

    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };
    const std::string kind = lower(type);
    const bool missileLike = weapon.missilesUsed > 0 ||
                             kind.find("missile") != std::string::npos ||
                             kind.find("bomb") != std::string::npos;
    const bool ionLike = kind.find("ion") != std::string::npos;
    const bool beamLike = kind.find("beam") != std::string::npos;
    const bool bypassShields = missileLike;
    const int evasion = std::clamp(targetEvasion, 0, 100);
    // FTL resolves each projectile independently against the target's evasion.
    // Keep the RNG local to volley resolution so the result is not tied to frame timing.
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> roll(0, 99);

    for (int shot = 0; shot < shots; ++shot) {
        const int targetRoom = shot < static_cast<int>(projectileTargets.size())
            ? projectileTargets[shot]
            : fallbackTargetRoom;
        if (targetRoom < 0) {
            ++resolved;
            continue;
        }

        // Defense Drones get the first interception attempt against each
        // eligible projectile. Bombs bypass them inside interceptWeaponWithDefenseDrone().
        if (!beamLike && interceptWeaponWithDefenseDrone(weaponIndex)) {
            ++resolved;
            continue;
        }

        // Beams do not use the normal projectile hit/miss roll; they always connect.
        // Other weapon projectiles have a per-shot chance to miss equal to target evasion.
        if (!beamLike && roll(rng) < evasion) {
            ++resolved;
            continue;
        }
        if (ionLike) {
            // Ion shots that meet a normal shield apply their ion damage to the
            // shield system itself; otherwise they ionize the selected room.
            int shieldIndex = -1;
            for (int i = 0; i < static_cast<int>(systems.size()); ++i) {
                if (systems[i].type == "shields" && systems[i].maxPower > 0) {
                    shieldIndex = i;
                    break;
                }
            }
            if (shieldLayers > weapon.shieldPiercing && shieldIndex >= 0) {
                ionizeSystemInRoom(systems[shieldIndex].room, std::max(1, weapon.ionDamage));
                shieldCharge = 0.0f;
            } else if (weapon.ionDamage > 0) {
                ionizeSystemInRoom(targetRoom, weapon.ionDamage);
            }
            ++resolved;
            continue;
        }

        if (!beamLike && !bypassShields && shieldLayers > weapon.shieldPiercing) {
            // Crystal/piercing projectiles pass through the configured number
            // of shield layers. Any remaining layer still absorbs the shot.
            --shieldLayers;
            shieldCharge = 0.0f;
            ++resolved;
            continue;
        }

        if (beamLike) {
            // Beams never consume shield layers. Their damage is reduced by one
            // for every regular shield layer currently present.
            int effectiveDamage = std::max(0, weapon.damage - shieldLayers);
            if (effectiveDamage > 0 && weapon.hullBust > 0) {
                const bool hasSystem = std::any_of(systems.begin(), systems.end(),
                    [targetRoom](const RuntimeSystem& system) {
                        return system.room == targetRoom;
                    });
                if (!hasSystem)
                    effectiveDamage += weapon.hullBust;
            }
            if (effectiveDamage > 0) {
                damageRoom(targetRoom, effectiveDamage);
                if (weapon.systemDamage > 0)
                    damageSystemInRoom(targetRoom, std::min(weapon.systemDamage, effectiveDamage));
                if (weapon.personnelDamage > 0)
                    damageCrewInRoom(targetRoom, weapon.personnelDamage);
                if (weapon.fireChance > 0 && roomOxygen[targetRoom] > 0 &&
                    (roll(rng) < std::clamp(weapon.fireChance, 0, 100))) {
                    setRoomFire(targetRoom, true);
                }
                if (weapon.breachChance > 0 &&
                    (roll(rng) < std::clamp(weapon.breachChance, 0, 100))) {
                    setRoomBreach(targetRoom, true);
                }
                if (weapon.stunChance > 0 && weapon.stunDuration > 0 &&
                    (roll(rng) < std::clamp(weapon.stunChance, 0, 100))) {
                    stunSystemsInRoom(targetRoom, static_cast<float>(weapon.stunDuration));
                }
            }
            ++resolved;
            continue;
        }

        // Shield-bypassing missiles/bombs and unblocked projectiles apply both
        // hull and system effects to the targeted room. A weapon's damage and
        // systemDamage are separate effects in the blueprint data.
        int hullDamage = std::max(0, weapon.damage);
        // Hull-buster weapons gain their bonus against rooms without a system,
        // matching the combat runtime's canonical room-resolution rule.
        if (hullDamage > 0 && weapon.hullBust > 0) {
            const bool hasSystem = std::any_of(systems.begin(), systems.end(),
                [targetRoom](const RuntimeSystem& system) {
                    return system.room == targetRoom;
                });
            if (!hasSystem)
                hullDamage += weapon.hullBust;
        }
        if (hullDamage > 0)
            damageRoom(targetRoom, hullDamage);
        if (weapon.systemDamage > 0)
            damageSystemInRoom(targetRoom, weapon.systemDamage);
        if (weapon.personnelDamage > 0)
            damageCrewInRoom(targetRoom, weapon.personnelDamage);
        if (weapon.ionDamage > 0)
            ionizeSystemInRoom(targetRoom, weapon.ionDamage);
        if (weapon.stunChance > 0 && weapon.stunDuration > 0 &&
            (roll(rng) < std::clamp(weapon.stunChance, 0, 100)))
            stunSystemsInRoom(targetRoom, static_cast<float>(weapon.stunDuration));

        ++resolved;
    }

    // The charge was spent by fireWeapon(); this consumes the pending volley
    // when its projectiles have resolved in the combat runtime.
    weapon.volleyPending = false;