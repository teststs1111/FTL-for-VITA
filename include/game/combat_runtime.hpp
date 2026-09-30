#pragma once
#include "game/ship_runtime.hpp"
#include <deque>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace wormhole {

enum class CombatOutcome { Ongoing, PlayerDestroyed, EnemyDestroyed, EnemyEscaped };
enum class CombatEnvironment { None, Asteroid, Sun, PlasmaStorm, PDSPlayer, PDSEnemy };

// Vanilla Rebel Fleet environment selection. Difficulty is explicit so the
// Easy-mode exit exception remains independently testable.
CombatEnvironment selectRebelFleetEnvironment(bool nebulaBeacon, bool exitBeacon, bool easyMode);

struct CombatResult {
    bool fired{false};
    int shotsFired{0};
    int shieldsAbsorbed{0};
    int evaded{0};
    int hullDamage{0};
    int systemDamage{0};
    int ionDamage{0};
    int personnelDamage{0};
    int firesStarted{0};
    int breachesStarted{0};
    int systemsStunned{0};
    bool targetDestroyed{false};
};

struct CombatShot {
    bool fromPlayer{true};
    bool fakeFlak{false};
    int weaponIndex{-1};
    RuntimeWeapon weapon;
    int targetRoom{-1};
    float elapsed{0.0f};
    float duration{0.25f};
};

class CombatRuntime {
public:
    ShipRuntime player;
    ShipRuntime enemy;
    int targetRoom{-1};
    int selectedWeapon{0};
    int enemyTargetRoom{0};
    CombatOutcome outcome{CombatOutcome::Ongoing};
    bool enemyDefeatedByCrew{false};
    std::vector<RuntimeCrew> boarders;

    bool load(ShipContent& content, const LoadedShip& enemyShip);
    bool loadFlagshipPhase(ShipContent& content, const LoadedShip& enemyShip,
                           const std::vector<RuntimeCrew>& previousCrew);
    void update(float dt);
    void setEnemyEscapeTimer(float seconds);
    bool setTargetRoom(int roomId);
    CombatResult fireSelectedWeapon();
    CombatResult fireWeapon(int weaponIndex);
    std::size_t pendingShotCount() const { return shots_.size(); }
    const std::vector<CombatShot>& pendingShots() const { return shots_; }
    const CombatResult& lastImpactResult() const { return lastImpactResult_; }
    bool consumeImpactResult(CombatResult& result);
    bool selectWeapon(int weaponIndex);
    bool setEnemyTargetRoom(int roomId);
    void setRandomSeed(std::uint32_t seed);
    void configureFlagshipPhase(int phase, int difficulty = 1);
    void setPlayerWeaponCooldownMultiplier(float multiplier) { playerWeaponCooldownMultiplier_ = multiplier; }
    void setPlayerShieldRechargeMultiplier(float multiplier) { playerShieldRechargeMultiplier_ = multiplier; }
    void setStealthWeapons(bool enabled) { stealthWeapons_ = enabled; }
    void setEnvironment(CombatEnvironment environment) {
        environment_ = environment;
        environmentTimer_ = 0.0f;
        if (environment_ == CombatEnvironment::PlasmaStorm)
            applyPlasmaStormPowerCap();
        else {
            player.setReactorPowerCap(-1);
            enemy.setReactorPowerCap(-1);
        }
    }
    CombatEnvironment environment() const { return environment_; }
    bool activateCloaking();
    void deactivateCloaking() { cloakTimer_ = 0.0f; }
    bool cloaked() const { return cloakTimer_ > 0.0f; }
    float cloakRemaining() const { return std::max(0.0f, cloakTimer_); }
    int superShieldRemaining() const { return superShield_; }
    bool flagshipSurgeWarning() const { return flagshipSurgeWarning_; }
    bool playerDeployedCombatDrone() const { return playerDeployedCombatDrone_; }
    bool enemyDefeatedByCrewDamage() const { return enemyDefeatedByCrew_; }

private:
    CombatResult resolveWeapon(ShipRuntime& attacker, ShipRuntime& target,
                               const RuntimeWeapon& weapon, int targetRoom);

    void enqueueWeapon(bool fromPlayer, int weaponIndex, const RuntimeWeapon& weapon,
                       int targetRoom);
    int flakProjectileTarget(const ShipRuntime& target, int targetRoom,
                             const RuntimeWeapon& weapon);
    std::vector<CombatShot> shots_;
    std::deque<CombatResult> impactResults_;
    CombatResult lastImpactResult_{};
    bool hasImpactResult_{false};
    float enemyFireDelay_{0.0f};
    float enemyEscapeTimer_{0.0f};
    std::uint32_t randomState_{0x6D2B79F5u};
    float boardingTimer_{0.0f};
    float boardingFightTimer_{0.0f};
    int flagshipPhase_{0};
    float droneSurgeTimer_{0.0f};
    float flagshipSurgeInterval_{0.0f};
    int superShield_{0};
    int flagshipSurgeCount_{0};
    int flagshipSurgeCombatDrones_{0};
    int flagshipSurgeBeamDrones_{0};
    bool flagshipSurgeWarning_{false};
    float playerWeaponCooldownMultiplier_{1.0f};
    float playerShieldRechargeMultiplier_{1.0f};
    bool stealthWeapons_{false};
    float cloakTimer_{0.0f};
    bool playerDeployedCombatDrone_{false};
    bool enemyDefeatedByCrew_{false};
    CombatEnvironment environment_{CombatEnvironment::None};
    float environmentTimer_{0.0f};
    std::uint32_t nextRandom();
    void updateEnvironmentHazard(float dt);
    void applyPlasmaStormPowerCap();
};

} 
