// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.trialspawner;

import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.Level;

public enum TrialSpawnerState implements StringRepresentable {
    INACTIVE(0, -1.0, false),
    WAITING_FOR_PLAYERS(4, 200.0, true),
    ACTIVE(8, 1000.0, true),
    WAITING_FOR_REWARD_EJECTION(8, -1.0, false),
    EJECTING_REWARD(8, -1.0, false),
    COOLDOWN(0, -1.0, false);

    private static final float kTimeBetweenEjections = 30.0F;
    private final int lightLevel;
    private final double spinningMobSpeed;
    private final boolean isCapableOfSpawning;

    TrialSpawnerState(int lightLevel, double spinningMobSpeed, boolean isCapableOfSpawning) {
        this.lightLevel = lightLevel;
        this.spinningMobSpeed = spinningMobSpeed;
        this.isCapableOfSpawning = isCapableOfSpawning;
    }

    TrialSpawnerState tickAndGetNext(BlockPos spawnerPos, TrialSpawner trialSpawner, ServerLevel serverLevel) {
        TrialSpawnerStateData book = trialSpawner.getStateData();
        TrialSpawnerConfig rules = trialSpawner.activeConfig();
        RandomSource rng = serverLevel.getRandom();
        switch (this) {
            case INACTIVE:
                return book.getOrCreateDisplayEntity(trialSpawner, serverLevel, WAITING_FOR_PLAYERS) == null ? this : WAITING_FOR_PLAYERS;
            case WAITING_FOR_PLAYERS:
                if (!trialSpawner.canSpawnInLevel(serverLevel)) {
                    book.resetStatistics();
                    return this;
                }
                if (!book.hasMobToSpawn(trialSpawner, rng)) {
                    return INACTIVE;
                }
                book.tryDetectPlayers(serverLevel, spawnerPos, trialSpawner);
                return book.detectedPlayers.isEmpty() ? this : ACTIVE;
            case ACTIVE:
                return tickActive(spawnerPos, trialSpawner, serverLevel, book, rules, rng);
            case WAITING_FOR_REWARD_EJECTION:
                if (!book.isReadyToOpenShutter(serverLevel, 40.0F, trialSpawner.getTargetCooldownLength())) {
                    return this;
                }
                serverLevel.playSound((Entity) null, spawnerPos, SoundEvents.TRIAL_SPAWNER_OPEN_SHUTTER, SoundSource.BLOCKS, 1.0F, 1.0F);
                return EJECTING_REWARD;
            case EJECTING_REWARD:
                if (!book.isReadyToEjectItems(serverLevel, kTimeBetweenEjections, trialSpawner.getTargetCooldownLength())) {
                    return this;
                }
                if (book.detectedPlayers.isEmpty()) {
                    serverLevel.playSound((Entity) null, spawnerPos, SoundEvents.TRIAL_SPAWNER_CLOSE_SHUTTER, SoundSource.BLOCKS, 1.0F, 1.0F);
                    book.ejectingLootTable = Optional.empty();
                    return COOLDOWN;
                }
                if (book.ejectingLootTable.isEmpty()) {
                    book.ejectingLootTable = rules.lootTablesToEject().getRandom(rng);
                }
                book.ejectingLootTable.ifPresent(table -> trialSpawner.ejectReward(serverLevel, spawnerPos, table));
                book.detectedPlayers.remove(book.detectedPlayers.iterator().next());
                return this;
            default:
                book.tryDetectPlayers(serverLevel, spawnerPos, trialSpawner);
                if (!book.detectedPlayers.isEmpty()) {
                    book.totalMobsSpawned = 0;
                    book.nextMobSpawnsAt = 0L;
                    return ACTIVE;
                }
                if (book.isCooldownFinished(serverLevel)) {
                    trialSpawner.removeOminous(serverLevel, spawnerPos);
                    book.reset();
                    return WAITING_FOR_PLAYERS;
                }
                return this;
        }
    }

    private TrialSpawnerState tickActive(BlockPos spawnerPos, TrialSpawner trialSpawner, ServerLevel serverLevel,
                                         TrialSpawnerStateData book, TrialSpawnerConfig rules, RandomSource rng) {
        if (!trialSpawner.canSpawnInLevel(serverLevel)) {
            book.resetStatistics();
            return WAITING_FOR_PLAYERS;
        }
        if (!book.hasMobToSpawn(trialSpawner, rng)) {
            return INACTIVE;
        }
        int extraPlayers = book.countAdditionalPlayers(spawnerPos);
        book.tryDetectPlayers(serverLevel, spawnerPos, trialSpawner);
        if (book.hasFinishedSpawningAllMobs(rules, extraPlayers)) {
            if (book.haveAllCurrentMobsDied()) {
                book.cooldownEndsAt = serverLevel.getGameTime() + trialSpawner.getTargetCooldownLength();
                book.totalMobsSpawned = 0;
                book.nextMobSpawnsAt = 0L;
                return WAITING_FOR_REWARD_EJECTION;
            }
        } else if (book.isReadyToSpawnNextMob(serverLevel, rules, extraPlayers)) {
            trialSpawner.spawnMob(serverLevel, spawnerPos).ifPresent(id -> {
                book.currentMobs.add(id);
                book.totalMobsSpawned++;
                book.nextMobSpawnsAt = serverLevel.getGameTime() + rules.ticksBetweenSpawn();
                rules.spawnPotentialsDefinition().getRandom(rng).ifPresent(entry -> {
                    book.nextSpawnData = Optional.of(entry);
                    trialSpawner.markUpdated();
                });
            });
        }
        return this;
    }

    public int lightLevel() {
        return lightLevel;
    }

    public double spinningMobSpeed() {
        return spinningMobSpeed;
    }

    public boolean hasSpinningMob() {
        return spinningMobSpeed >= 0.0;
    }

    public boolean isCapableOfSpawning() {
        return isCapableOfSpawning;
    }

    public void emitParticles(Level level, BlockPos blockPos, boolean isOminous) {
    }

    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
