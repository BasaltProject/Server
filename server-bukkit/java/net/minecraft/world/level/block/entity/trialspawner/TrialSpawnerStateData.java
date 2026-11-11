// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.trialspawner;
import java.util.HashSet;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.effect.MobEffects;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.player.Player;
import net.minecraft.nbt.NbtOps;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.SpawnData;
import net.minecraft.world.level.storage.loot.LootTable;
public class TrialSpawnerStateData {
    private static Holder<MobEffect> ominousEffectOf(Player player) {
        return player.hasEffect(MobEffects.TRIAL_OMEN) ? MobEffects.TRIAL_OMEN : player.hasEffect(MobEffects.BAD_OMEN) ? MobEffects.BAD_OMEN : null;
    }
    final Set<UUID> detectedPlayers = new HashSet<>();
    final Set<UUID> currentMobs = new HashSet<>();
    long cooldownEndsAt;
    long nextMobSpawnsAt;
    int totalMobsSpawned;
    Optional<SpawnData> nextSpawnData = Optional.<SpawnData>empty();
    Optional<ResourceKey<LootTable>> ejectingLootTable = Optional.<ResourceKey<LootTable>>empty();
    double spin;
    double oSpin;
    public Packed pack() {
        return new Packed(Set.copyOf(detectedPlayers), Set.copyOf(currentMobs), cooldownEndsAt, nextMobSpawnsAt, totalMobsSpawned, nextSpawnData, ejectingLootTable);
    }
    public void apply(Packed packed) {
        detectedPlayers.clear();
        detectedPlayers.addAll(packed.detectedPlayers());
        currentMobs.clear();
        currentMobs.addAll(packed.currentMobs());
        cooldownEndsAt = packed.cooldownEndsAt();
        nextMobSpawnsAt = packed.nextMobSpawnsAt();
        totalMobsSpawned = packed.totalMobsSpawned();
        nextSpawnData = packed.nextSpawnData();
        ejectingLootTable = packed.ejectingLootTable();
    }
    public void reset() {
        currentMobs.clear();
        nextSpawnData = Optional.empty();
        resetStatistics();
    }
    public void resetStatistics() {
        detectedPlayers.clear();
        totalMobsSpawned = 0;
        nextMobSpawnsAt = 0L;
        cooldownEndsAt = 0L;
    }
    public boolean hasMobToSpawn(TrialSpawner trialSpawner, RandomSource random) {
        boolean named = getOrCreateNextSpawnData(trialSpawner, random).getEntityToSpawn().getString("id").isPresent();
        return named || !trialSpawner.activeConfig().spawnPotentialsDefinition().isEmpty();
    }
    public boolean hasFinishedSpawningAllMobs(TrialSpawnerConfig config, int additionalPlayers) {
        return totalMobsSpawned >= config.calculateTargetTotalMobs(additionalPlayers);
    }
    public boolean haveAllCurrentMobsDied() {
        return currentMobs.isEmpty();
    }
    public boolean isReadyToSpawnNextMob(ServerLevel serverLevel, TrialSpawnerConfig config, int additionalPlayers) {
        return serverLevel.getGameTime() >= nextMobSpawnsAt && currentMobs.size() < config.calculateTargetSimultaneousMobs(additionalPlayers);
    }
    public int countAdditionalPlayers(BlockPos pos) {
        return Math.max(0, detectedPlayers.size() - 1);
    }
    public void tryDetectPlayers(ServerLevel level, BlockPos pos, TrialSpawner trialSpawner) {
        if ((pos.asLong() + level.getGameTime()) % 20L != 0L) {
            return;
        }
        if (trialSpawner.getState().equals(TrialSpawnerState.COOLDOWN) && trialSpawner.isOminous()) {
            return;
        }
        List<UUID> visible = trialSpawner.getPlayerDetector().detect(level, trialSpawner.getEntitySelector(), pos, trialSpawner.getRequiredPlayerRange(), true);
        boolean becameOminous = false;
        if (!trialSpawner.isOminous() && !visible.isEmpty()) {
            Player bearer = null;
            Holder<MobEffect> effect = null;
            for (UUID id : visible) {
                Player player = level.getPlayerByUUID(id);
                Holder<MobEffect> found = player == null ? null : ominousEffectOf(player);
                if (found == MobEffects.TRIAL_OMEN) {
                    bearer = player;
                    effect = found;
                    break;
                }
                if (found != null) {
                    bearer = player;
                    effect = found;
                }
            }
            if (bearer != null) {
                if (effect == MobEffects.BAD_OMEN) {
                    MobEffectInstance omen = bearer.getEffect(MobEffects.BAD_OMEN);
                    int level2 = omen.getAmplifier() + 1;
                    bearer.removeEffect(MobEffects.BAD_OMEN);
                    bearer.addEffect(new MobEffectInstance(MobEffects.TRIAL_OMEN, 18000 * level2, 0));
                }
                level.levelEvent(3020, BlockPos.containing(bearer.getEyePosition()), 0);
                trialSpawner.applyOminous(level, pos);
                becameOminous = true;
            }
        }
        if (trialSpawner.getState().equals(TrialSpawnerState.COOLDOWN) && !becameOminous) {
            return;
        }
        boolean first = trialSpawner.getStateData().detectedPlayers.isEmpty();
        List<UUID> found = first ? visible : trialSpawner.getPlayerDetector().detect(level, trialSpawner.getEntitySelector(), pos, trialSpawner.getRequiredPlayerRange(), false);
        if (detectedPlayers.addAll(found)) {
            nextMobSpawnsAt = Math.max(level.getGameTime() + 40L, nextMobSpawnsAt);
            if (!becameOminous) {
                level.levelEvent(trialSpawner.isOminous() ? 3019 : 3013, pos, detectedPlayers.size());
            }
        }
    }
    public void resetAfterBecomingOminous(TrialSpawner trialSpawner, ServerLevel level) {
        for (UUID id : currentMobs) {
            Entity entity = level.getEntity(id);
            if (entity == null) {
                continue;
            }
            level.levelEvent(3012, entity.blockPosition(), TrialSpawner.FlameParticle.NORMAL.encode());
            if (entity instanceof Mob mob) {
                mob.dropPreservedEquipment(level);
            }
            entity.remove(Entity.RemovalReason.DISCARDED);
        }
        if (!trialSpawner.ominousConfig().spawnPotentialsDefinition().isEmpty()) {
            nextSpawnData = Optional.empty();
        }
        totalMobsSpawned = 0;
        currentMobs.clear();
        nextMobSpawnsAt = level.getGameTime() + trialSpawner.ominousConfig().ticksBetweenSpawn();
        trialSpawner.markUpdated();
        cooldownEndsAt = level.getGameTime() + trialSpawner.ominousConfig().ticksBetweenItemSpawners();
    }
    public boolean isReadyToOpenShutter(ServerLevel serverLevel, float delayBeforeOpen, int targetCooldownLength) {
        long started = cooldownEndsAt - targetCooldownLength;
        return serverLevel.getGameTime() >= started + delayBeforeOpen;
    }
    public boolean isReadyToEjectItems(ServerLevel serverLevel, float timeBetweenEjections, int targetCooldownLength) {
        long started = cooldownEndsAt - targetCooldownLength;
        return (serverLevel.getGameTime() - started) % timeBetweenEjections == 0.0F;
    }
    public boolean isCooldownFinished(ServerLevel serverLevel) {
        return serverLevel.getGameTime() >= cooldownEndsAt;
    }
    protected SpawnData getOrCreateNextSpawnData(TrialSpawner trialSpawner, RandomSource random) {
        if (nextSpawnData.isEmpty()) {
            WeightedList<SpawnData> potentials = trialSpawner.activeConfig().spawnPotentialsDefinition();
            Optional<SpawnData> picked = potentials.isEmpty() ? nextSpawnData : potentials.getRandom(random);
            nextSpawnData = Optional.of(picked.orElseGet(SpawnData::new));
            trialSpawner.markUpdated();
        }
        return nextSpawnData.get();
    }
    public Entity getOrCreateDisplayEntity(TrialSpawner trialSpawner, Level level, TrialSpawnerState state) {
        return null;
    }
    public CompoundTag getUpdateTag(TrialSpawnerState state) {
        CompoundTag tag = new CompoundTag();
        if (state == TrialSpawnerState.ACTIVE) {
            tag.putLong("next_mob_spawns_at", nextMobSpawnsAt);
        }
        nextSpawnData.flatMap(spawn -> SpawnData.CODEC.encodeStart(NbtOps.INSTANCE, spawn).result()).ifPresent(encoded -> tag.put("spawn_data", encoded));
        return tag;
    }
    public double getSpin() {
        return spin;
    }
    public double getOSpin() {
        return oSpin;
    }
    public WeightedList<ItemStack> getDispensingItems(ServerLevel level, TrialSpawnerConfig config, BlockPos pos) {
        return WeightedList.of();


    }
    public static record Packed(Set<UUID> detectedPlayers, Set<UUID> currentMobs, long cooldownEndsAt, long nextMobSpawnsAt, int totalMobsSpawned, Optional<SpawnData> nextSpawnData, Optional<ResourceKey<LootTable>> ejectingLootTable) {
        public static final MapCodec<Packed> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.block.entity.trialspawner.TrialSpawnerStateData.Packed.class, "MAP_CODEC");
    }
}
