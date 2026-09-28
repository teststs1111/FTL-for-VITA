#include "game/ship_runtime.hpp"
#include <cassert>

using namespace wormhole;

int main() {
    // Zoltan source redistribution: subsystems cannot receive free power,
    // and a Zoltan entering a full main system displaces Battery power first.
    {
        ShipRuntime sourceTest;
        sourceTest.valid = true;
        sourceTest.reactor = 10;
        RuntimeSystem mainSystem;
        mainSystem.type = "weapons";
        mainSystem.room = 1;
        mainSystem.power = 4;
        mainSystem.maxPower = 4;
        mainSystem.powered = true;
        mainSystem.batteryPower = 2;
        sourceTest.systems.push_back(mainSystem);
        RuntimeSystem subSystem;
        subSystem.type = "pilot";
        subSystem.room = 1;
        subSystem.power = 1;
        subSystem.maxPower = 1;
        subSystem.powered = true;
        sourceTest.systems.push_back(subSystem);
        RuntimeCrew z;
        z.race = "zoltan";
        z.room = 1;
        z.alive = true;
        sourceTest.crew.push_back(z);
        assert(sourceTest.availableZoltanPowerForSystem(sourceTest.systems[1]) == 0);
        sourceTest.rebalanceZoltanPowerSources();
        assert(sourceTest.systems[0].power == 4);
        assert(sourceTest.systems[0].zoltanPower == 1);
        assert(sourceTest.systems[0].batteryPower == 1);
        sourceTest.crew[0].room = 2;
        sourceTest.rebalanceZoltanPowerSources();
        assert(sourceTest.systems[0].power == 3);
        assert(sourceTest.systems[0].zoltanPower == 0);
        assert(sourceTest.systems[0].batteryPower == 1);
    }

    ShipRuntime ship;
    ship.valid = true;
    ship.reactor = 10;

    RuntimeSystem weapons;
    weapons.type = "weapons";
    weapons.room = 1;
    weapons.power = 3;
    weapons.maxPower = 4;
    weapons.powered = true;
    ship.systems.push_back(weapons);

    RuntimeSystem pilot;
    pilot.type = "pilot";
    pilot.room = 1;
    pilot.power = 1;
    pilot.maxPower = 1;
    pilot.powered = true;
    ship.systems.push_back(pilot);

    RuntimeCrew z1;
    z1.race = "zoltan";
    z1.room = 1;
    z1.alive = true;
    ship.crew.push_back(z1);

    RuntimeCrew z2 = z1;
    ship.crew.push_back(z2);

    // Two Zoltans are initially assigned to the existing weapon power bars.
    ship.systems[0].zoltanPower = 2;
    assert(ship.zoltanPowerForSystem(ship.systems[0]) == 2);
    assert(ship.reactorFundedPowerForSystem(ship.systems[0]) == 1);

    // A Zoltan leaving the room removes only its free bar; returning restores it.
    ship.crew[0].room = 2;
    ship.rebalanceZoltanPowerSources();
    assert(ship.systems[0].zoltanPower == 1);
    assert(ship.systems[0].power == 2);
    ship.crew[0].room = 1;
    ship.rebalanceZoltanPowerSources();
    assert(ship.systems[0].zoltanPower == 2);
    assert(ship.systems[0].power == 3);

    // A dead Zoltan likewise loses its supplied bar.
    ship.crew[1].health = 0;
    ship.crew[1].alive = false;
    ship.rebalanceZoltanPowerSources();
    assert(ship.systems[0].zoltanPower == 1);
    assert(ship.systems[0].power == 2);
    ship.crew[1].alive = true;
    ship.crew[1].health = ship.crew[1].maxHealth;
    ship.rebalanceZoltanPowerSources();
    assert(ship.systems[0].zoltanPower == 2);
    assert(ship.systems[0].power == 3);

    // Subsystems do not receive Zoltan power.
    assert(ship.zoltanPowerForSystem(ship.systems[1]) == 0);
    assert(ship.reactorFundedPowerForSystem(ship.systems[1]) == 1);

    // Total reactor consumption excludes the two free Zoltan bars.
    assert(ship.usedReactorPower() == 2);
    assert(ship.availableReactorPower() == 8);

    // Backup Battery I adds +2 temporary power without consuming reactor power.
    RuntimeSystem battery;
    battery.type = "battery";
    battery.room = 0;
    battery.level = 1;
    battery.power = 1;
    battery.maxPower = 2;
    battery.powered = true;
    ship.systems.push_back(battery);
    assert(ship.usedReactorPower() == 2);
    assert(ship.availableReactorPower() == 8);
    assert(ship.activateBackupBattery());
    assert(ship.backupBatteryPower() == 2);
    assert(ship.backupBatteryRemaining() > 29.9f);
    assert(ship.availableReactorPower() == 10);

    // Battery bars are allocated only after regular reactor power is exhausted.
    ship.setReactorPowerCap(2);
    assert(ship.setSystemPower(0, 4));
    assert(ship.systems[0].batteryPower == 1);
    assert(ship.systems[0].zoltanPower == 2);
    assert(ship.reactorFundedPowerForSystem(ship.systems[0]) == 1);
    ship.updateBackupBattery(30.0f);
    assert(ship.backupBatteryPower() == 0);
    assert(ship.systems[0].power == 3);
    assert(ship.systems[0].batteryPower == 0);
    assert(ship.backupBatteryCooldownRemaining() > 19.9f);
    assert(ship.availableReactorPower() == 8);

    // Level II supplies four temporary bars after the normal cooldown.
    ship.updateBackupBattery(20.0f);
    assert(ship.backupBatteryCooldownRemaining() == 0.0f);
    ship.systems[2].level = 2;
    ship.systems[2].powered = true;
    assert(ship.activateBackupBattery());
    assert(ship.backupBatteryPower() == 4);
    ship.updateBackupBattery(30.0f);
    ship.updateBackupBattery(20.0f);

    ship.setReactorPowerCap(5);
    assert(ship.availableReactorPower() == 3);

    // A Zoltan can supply a new power bar even when the reactor is capped.
    ship.systems[0].power = 0;
    ship.systems[0].powered = false;
    ship.setReactorPowerCap(0);
    assert(ship.setSystemPower(0, 1));
    assert(ship.systems[0].power == 1);
    assert(ship.systems[0].powered);
    assert(ship.zoltanPowerForSystem(ship.systems[0]) == 1);
    assert(ship.reactorFundedPowerForSystem(ship.systems[0]) == 0);
    assert(ship.availableReactorPower() == 0);

    // Turning on a preallocated system can likewise use a Zoltan bar.
    ship.systems[0].powered = false;
    assert(ship.setSystemPowered(0, true));

    // Drone Control and Artillery are main systems, not subsystems:
    // Zoltan power can supply them as well.
    RuntimeSystem drones;
    drones.type = "drones";
    drones.room = 2;
    drones.power = 1;
    drones.maxPower = 2;
    drones.powered = true;
    ship.systems.push_back(drones);

    RuntimeSystem artillery;
    artillery.type = "artillery";
    artillery.room = 3;
    artillery.power = 1;
    artillery.maxPower = 4;
    artillery.powered = true;
    ship.systems.push_back(artillery);

    RuntimeCrew z3 = z1;
    z3.room = 2;
    ship.crew.push_back(z3);
    assert(ship.zoltanPowerForSystem(ship.systems[3]) == 1);
    assert(ship.zoltanPowerForSystem(ship.systems[4]) == 0);

    RuntimeCrew z4 = z3;
    z4.room = 3;
    ship.crew.push_back(z4);
    assert(ship.zoltanPowerForSystem(ship.systems[4]) == 1);

    // Ion damage removes normal power but leaves Zoltan power intact, locks
    // manual allocation, and restores the lost power after the 5-second lock.
    {
        ShipRuntime ionShip;
        ionShip.valid = true;
        ionShip.reactor = 1;
        ionShip.setReactorPowerCap(1);

        RuntimeSystem ionSystem;
        ionSystem.type = "shields";
        ionSystem.room = 10;
        ionSystem.power = 4;
        ionSystem.maxPower = 4;
        ionSystem.powered = true;
        ionSystem.zoltanPower = 1;
        ionSystem.batteryPower = 2;
        ionShip.systems.push_back(ionSystem);
        ionShip.backupBatteryActivePower = 2;
        ionShip.backupBatteryTimer = 20.0f;

        assert(ionShip.usedReactorPower() == 1);
        assert(ionShip.ionizeSystemInRoom(10, 2) == 2);
        assert(ionShip.systems[0].ionDamage == 2);
        assert(ionShip.systems[0].ionRemovedPower == 2);
        assert(ionShip.systems[0].power == 2);
        assert(ionShip.systems[0].zoltanPower == 1);
        assert(ionShip.systems[0].batteryPower == 0);
        assert(!ionShip.setSystemPower(0, 1));
        assert(ionShip.systems[0].power == 2);

        ionShip.updateEnvironment(5.0f);
        assert(ionShip.systems[0].ionDamage == 0);
        assert(ionShip.systems[0].ionRemovedPower == 0);
        assert(ionShip.systems[0].power == 4);
        assert(ionShip.systems[0].zoltanPower == 1);
        // The lost bars can return as Battery power while the Battery remains active.
        assert(ionShip.systems[0].batteryPower == 2);
        assert(ionShip.reactorFundedPowerForSystem(ionShip.systems[0]) == 1);
    }

    // Ion damage stacks: each additional point adds five seconds, capped at
    // five ion points / twenty-five seconds.
    {
        ShipRuntime ionStack;
        ionStack.valid = true;
        RuntimeSystem system;
        system.type = "shields";
        system.room = 3;
        system.power = 5;
        system.maxPower = 5;
        system.powered = true;
        ionStack.systems.push_back(system);

        assert(ionStack.ionizeSystemInRoom(3, 2) == 2);
        assert(ionStack.systems[0].ionDamage == 2);
        assert(ionStack.systems[0].ionTimer == 10.0f);
        assert(ionStack.ionizeSystemInRoom(3, 2) == 2);
        assert(ionStack.systems[0].ionDamage == 4);
        assert(ionStack.systems[0].ionTimer == 20.0f);
        assert(ionStack.ionizeSystemInRoom(3, 2) == 1);
        assert(ionStack.systems[0].ionDamage == 5);
        assert(ionStack.systems[0].ionTimer == 25.0f);
        assert(ionStack.ionizeSystemInRoom(3, 1) == 0);
    }


    // Weapons have special Ion behavior: each Ion point shuts down one whole
    // active weapon from right to left, rather than removing one generic bar.
    // Zoltan-funded power remains available to the weapon.
    {
        ShipRuntime weaponIon;
        weaponIon.valid = true;
        weaponIon.reactor = 3;

        RuntimeSystem weaponSystem;
        weaponSystem.type = "weapons";
        weaponSystem.room = 20;
        weaponSystem.power = 4;
        weaponSystem.maxPower = 4;
        weaponSystem.powered = true;
        weaponSystem.zoltanPower = 1;
        weaponIon.systems.push_back(weaponSystem);

        RuntimeWeapon glaive;
        glaive.name = "Glaive Beam";
        glaive.power = 4;
        glaive.charge = 7.0f;
        glaive.ready = true;
        weaponIon.weapons.push_back(glaive);
        weaponIon.weaponIonDisabled.assign(1, false);

        assert(weaponIon.ionizeSystemInRoom(20, 1) == 1);
        assert(weaponIon.systems[0].ionDamage == 1);
        assert(weaponIon.systems[0].power == 1);
        assert(weaponIon.systems[0].zoltanPower == 1);
        assert(weaponIon.systems[0].ionRemovedPower == 3);
        assert(weaponIon.weaponIonDisabled[0]);
        assert(weaponIon.weapons[0].charge == 0.0f);
        assert(!weaponIon.weapons[0].ready);
        assert(!weaponIon.fireWeapon(0));

        weaponIon.updateEnvironment(5.0f);
        assert(weaponIon.systems[0].power == 4);
        assert(weaponIon.systems[0].zoltanPower == 1);
        assert(weaponIon.systems[0].ionDamage == 0);
        assert(!weaponIon.weaponIonDisabled[0]);

        // A 2-ion hit can take two 1-power weapons offline in one volley.
        ShipRuntime multiWeapon;
        multiWeapon.valid = true;
        multiWeapon.reactor = 2;

        RuntimeSystem multiSystem;
        multiSystem.type = "weapons";
        multiSystem.room = 21;
        multiSystem.power = 2;
        multiSystem.maxPower = 2;
        multiSystem.powered = true;
        multiWeapon.systems.push_back(multiSystem);

        RuntimeWeapon first;
        first.power = 1;
        first.ready = true;
        RuntimeWeapon second;
        second.power = 1;
        second.ready = true;
        multiWeapon.weapons.push_back(first);
        multiWeapon.weapons.push_back(second);
        multiWeapon.weaponIonDisabled.assign(2, false);

        assert(multiWeapon.ionizeSystemInRoom(21, 2) == 2);
        assert(multiWeapon.systems[0].power == 0);
        assert(multiWeapon.systems[0].ionRemovedPower == 2);
        assert(multiWeapon.weaponIonDisabled[0]);
        assert(multiWeapon.weaponIonDisabled[1]);
        assert(!multiWeapon.weapons[0].ready);
        assert(!multiWeapon.weapons[1].ready);
    }


    // Weapons consume system power cumulatively by slot order. A 3-power
    // Weapons system can run two 1-power weapons, but not a later 2-power gun.
    {
        ShipRuntime weaponPower;
        weaponPower.valid = true;
        RuntimeSystem system;
        system.type = "weapons";
        system.room = 30;
        system.power = 3;
        system.maxPower = 4;
        system.powered = true;
        weaponPower.systems.push_back(system);

        RuntimeWeapon first;
        first.power = 1;
        RuntimeWeapon second;
        second.power = 1;
        RuntimeWeapon heavy;
        heavy.power = 2;
        weaponPower.weapons = {first, second, heavy};
        weaponPower.weaponIonDisabled.assign(3, false);

        weaponPower.updateWeapons(1.0f);
        assert(weaponPower.weapons[0].allocatedPower == 1);
        assert(weaponPower.weapons[1].allocatedPower == 1);
        assert(weaponPower.weapons[2].allocatedPower == 0);
        assert(weaponPower.weapons[0].charge > 0.0f);
        assert(weaponPower.weapons[1].charge > 0.0f);
        assert(weaponPower.weapons[2].charge == 0.0f);

        // Losing one Weapons-system power removes the rightmost allocated slot.
        assert(weaponPower.damageSystemInRoom(30, 1) == 1);
        assert(weaponPower.systems[0].power == 2);
        assert(weaponPower.weapons[0].allocatedPower == 1);
        assert(weaponPower.weapons[1].allocatedPower == 1);
        assert(weaponPower.weapons[2].allocatedPower == 0);

        // With only one power remaining, the second slot is the one that drops.
        assert(weaponPower.damageSystemInRoom(30, 1) == 1);
        assert(weaponPower.systems[0].power == 1);
        assert(weaponPower.weapons[0].allocatedPower == 1);
        assert(weaponPower.weapons[1].allocatedPower == 0);
        assert(weaponPower.weapons[1].charge == 0.0f);
        assert(weaponPower.weapons[2].allocatedPower == 0);
    }

    // Weapon firing uses the per-slot allocation, consumes one missile cost
    // per volley, and resets the volley charge. A multi-shot weapon does not
    // multiply its ammunition cost by the number of projectiles.
    {
        ShipRuntime firing;
        firing.valid = true;
        RuntimeSystem system;
        system.type = "weapons";
        system.room = 40;
        system.power = 2;
        system.maxPower = 2;
        system.powered = true;
        firing.systems.push_back(system);
        firing.missiles = 5;

        RuntimeWeapon multi;
        multi.power = 2;
        multi.shots = 3;
        multi.missilesUsed = 1;
        multi.cooldown = 4.0f;
        multi.charge = 4.0f;
        multi.ready = true;
        firing.weapons.push_back(multi);
        firing.weaponIonDisabled.assign(1, false);

        // Without allocation, total system power alone is not enough to fire.
        assert(!firing.fireWeapon(0));

        firing.updateWeapons(0.1f);
        assert(firing.weapons[0].allocatedPower == 2);
        assert(firing.fireWeapon(0));
        assert(firing.missiles == 4);
        assert(firing.weapons[0].charge == 0.0f);
        assert(!firing.weapons[0].ready);

        // Not enough missiles blocks the entire volley without consuming ammo.
        firing.missiles = 0;
        firing.weapons[0].charge = firing.weapons[0].cooldown;
        firing.weapons[0].ready = true;
        assert(!firing.fireWeapon(0));
        assert(firing.missiles == 0);
        assert(firing.weapons[0].ready);
    }

    // Volley resolution: a fired multi-shot projectile volley consumes shields
    // one projectile at a time, while a shield-bypassing missile applies room
    // damage directly. Resolution occurs after fireWeapon() via volleyPending.
    {
        ShipRuntime combat;
        combat.valid = true;
        combat.missiles = 2;

        RuntimeSystem weaponsSystem;
        weaponsSystem.type = "weapons";
        weaponsSystem.room = 50;
        weaponsSystem.power = 2;
        weaponsSystem.maxPower = 2;
        weaponsSystem.powered = true;
        combat.systems.push_back(weaponsSystem);

        RuntimeSystem enemyShields;
        enemyShields.type = "shields";
        enemyShields.room = 51;
        enemyShields.power = 2;
        enemyShields.maxPower = 2;
        enemyShields.powered = true;
        combat.systems.push_back(enemyShields);
        combat.shieldLayers = 2;
        combat.maxShieldLayers = 2;

        RuntimeWeapon laser;
        laser.type = "laser";
        laser.power = 2;
        laser.shots = 2;
        laser.damage = 1;
        laser.systemDamage = 1;
        laser.cooldown = 1.0f;
        laser.charge = 1.0f;
        laser.ready = true;
        combat.weapons.push_back(laser);
        combat.weaponIonDisabled.assign(1, false);

        combat.updateWeapons(0.1f);
        assert(combat.fireWeapon(0));
        assert(combat.weapons[0].volleyPending);
        assert(combat.resolveWeaponVolley(0, 51) == 2);
        assert(combat.shieldLayers == 0);
        assert(!combat.weapons[0].volleyPending);
        assert(combat.hull == 0);

        RuntimeWeapon missile;
        missile.type = "missile";
        missile.power = 2;
        missile.shots = 1;
        missile.damage = 2;
        missile.systemDamage = 1;
        missile.missilesUsed = 1;
        missile.cooldown = 1.0f;
        missile.charge = 1.0f;
        missile.ready = true;
        combat.weapons.push_back(missile);
        combat.weaponIonDisabled.assign(2, false);
        combat.updateWeapons(0.1f);
        assert(combat.fireWeapon(1));
        assert(combat.missiles == 1);
        assert(combat.resolveWeaponVolley(1, 51) == 1);
        assert(combat.hull == 2);
        assert(combat.systems[1].damage == 1);
    }

    // Projectile evasion: each projectile is rolled independently. A 100% evasion
    // target causes a projectile to miss, while a 0% evasion target is guaranteed hit.
    {
        ShipRuntime evasion;
        evasion.valid = true;
        evasion.hull = 10;
        evasion.maxHull = 10;
        RuntimeSystem weaponsSystem;
        weaponsSystem.type = "weapons";
        weaponsSystem.room = 60;
        weaponsSystem.power = 1;
        weaponsSystem.maxPower = 1;
        weaponsSystem.powered = true;
        evasion.systems.push_back(weaponsSystem);

        RuntimeWeapon laser;
        laser.type = "laser";
        laser.power = 1;
        laser.shots = 1;
        laser.damage = 2;
        laser.cooldown = 1.0f;
        laser.charge = 1.0f;
        laser.ready = true;
        evasion.weapons.push_back(laser);
        evasion.weaponIonDisabled.assign(1, false);
        evasion.updateWeapons(0.1f);

        assert(evasion.fireWeapon(0));
        assert(evasion.resolveWeaponVolley(0, 61, 100) == 1);
        assert(evasion.hull == 0);

        evasion.weapons[0].charge = evasion.weapons[0].cooldown;
        evasion.weapons[0].ready = true;
        evasion.updateWeapons(0.1f);
        assert(evasion.fireWeapon(0));
        assert(evasion.resolveWeaponVolley(0, 61, 0) == 1);
        assert(evasion.hull == 2);
    }

    // Flak projectiles resolve independently against their individual target rooms.
    // One shield layer absorbs only the projectile that encounters it; later projectiles
    // can hit different rooms in the same volley.
    {
        ShipRuntime flak;
        flak.valid = true;
        flak.hull = 10;
        flak.maxHull = 10;
        flak.roomDamage.assign(4, 0);
        flak.shieldLayers = 1;
        flak.maxShieldLayers = 1;

        RuntimeSystem weaponsSystem;
        weaponsSystem.type = "weapons";
        weaponsSystem.room = 80;
        weaponsSystem.power = 3;
        weaponsSystem.maxPower = 3;
        weaponsSystem.powered = true;
        flak.systems.push_back(weaponsSystem);

        RuntimeWeapon weapon;
        weapon.type = "flak";
        weapon.power = 3;
        weapon.shots = 3;
        weapon.damage = 1;
        weapon.cooldown = 1.0f;
        weapon.allocatedPower = 3;
        weapon.volleyPending = true;
        flak.weapons.push_back(weapon);
        flak.weaponIonDisabled.assign(1, false);

        const std::vector<int> targets{1, 2, 3};
        assert(flak.resolveWeaponVolley(0, targets, 0) == 3);
        assert(flak.shieldLayers == 0);
        assert(flak.hull == 8);
        assert(flak.roomDamage[1] == 0);
        assert(flak.roomDamage[2] == 1);
        assert(flak.roomDamage[3] == 1);
        assert(!flak.weapons[0].volleyPending);
    }

    // Defense Drones intercept individual eligible projectiles before evasion.
    // Bombs bypass them, while Mark I cannot intercept lasers/ions.
    {
        ShipRuntime defense;
        defense.valid = true;
        defense.hull = 10;
        defense.maxHull = 10;
        RuntimeSystem weaponsSystem;
        weaponsSystem.type = "weapons";
        weaponsSystem.room = 70;
        weaponsSystem.power = 1;
        weaponsSystem.maxPower = 1;
        weaponsSystem.powered = true;
        defense.systems.push_back(weaponsSystem);

        RuntimeWeapon missile;
        missile.type = "missile";
        missile.power = 1;
        missile.damage = 2;
        missile.cooldown = 1.0f;
        missile.charge = 1.0f;
        missile.ready = true;
        defense.weapons.push_back(missile);
        defense.weaponIonDisabled.assign(1, false);

        RuntimeDrone dd1;
        dd1.name = "Defense Drone Mark I";
        dd1.powered = true;
        dd1.active = true;
        defense.drones.push_back(dd1);
        assert(defense.interceptWeaponWithDefenseDrone(0));
        assert(!defense.drones[0].active);

        RuntimeWeapon bomb;
        bomb.type = "bomb";
        bomb.power = 1;
        bomb.damage = 2;
        bomb.cooldown = 1.0f;
        bomb.charge = 1.0f;
        bomb.ready = true;
        defense.weapons.push_back(bomb);
        defense.weaponIonDisabled.assign(2, false);
        defense.drones[0].active = true;
        assert(!defense.interceptWeaponWithDefenseDrone(1));

        RuntimeWeapon laser;
        laser.type = "laser";
        laser.power = 1;
        laser.damage = 1;
        laser.cooldown = 1.0f;
        laser.charge = 1.0f;
        laser.ready = true;
        defense.weapons.push_back(laser);
        defense.weaponIonDisabled.assign(3, false);
        defense.drones[0].active = true;
        assert(!defense.interceptWeaponWithDefenseDrone(2));

        RuntimeDrone dd2;
        dd2.name = "Defense Drone Mark II";
        dd2.powered = true;
        dd2.active = true;
        defense.drones.push_back(dd2);
        assert(defense.interceptWeaponWithDefenseDrone(2));
        assert(!defense.drones[1].active);
    }

    return 0;
}
