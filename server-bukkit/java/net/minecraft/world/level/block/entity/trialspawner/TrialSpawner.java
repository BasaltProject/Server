// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.trialspawner;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.block.TrialSpawnerBlock;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.core.particles.SimpleParticleType;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.component.TypedEntityData;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.level.storage.loot.LootTable;
public final class TrialSpawner {
    private final TrialSpawnerStateData data = new TrialSpawnerStateData();
    private FullConfig config;
    private final StateAccessor stateAccessor;
    private PlayerDetector playerDetector;
    private final PlayerDetector.EntitySelector entitySelector;
    private boolean overridePeacefulAndMobSpawnRule;
    private boolean isOminous;
    public static final int DETECT_PLAYER_SPAWN_BUFFER = 40;
    public TrialSpawner(FullConfig config, StateAccessor stateAccessor, PlayerDetector playerDetector, PlayerDetector.EntitySelector entitySelector) {
        this.config = config;
        this.stateAccessor = stateAccessor;
        this.playerDetector = playerDetector;
        this.entitySelector = entitySelector;
    }
    public TrialSpawnerConfig activeConfig() {
        return isOminous ? config.ominous().value() : config.normal().value();
    }
    public TrialSpawnerConfig normalConfig() {
        return config.normal().value();
    }
    public TrialSpawnerConfig ominousConfig() {
        return config.ominous().value();
    }
    public void load(ValueInput input) {
        input.read(TrialSpawnerStateData.Packed.MAP_CODEC).ifPresent(data::apply);
        config = input.read(FullConfig.MAP_CODEC).orElse(FullConfig.DEFAULT);
    }
    public void store(ValueOutput output) {
        output.store(TrialSpawnerStateData.Packed.MAP_CODEC, data.pack());
        output.store(FullConfig.MAP_CODEC, config);
    }
    public void applyOminous(ServerLevel level, BlockPos spawnerPos) {
        level.setBlockAndUpdate(spawnerPos, level.getBlockState(spawnerPos).setValue(TrialSpawnerBlock.OMINOUS, true));
        level.levelEvent(3020, spawnerPos, 1);
        isOminous = true;
        data.resetAfterBecomingOminous(this, level);
    }
    public void removeOminous(ServerLevel level, BlockPos spawnerPos) {
        level.setBlockAndUpdate(spawnerPos, level.getBlockState(spawnerPos).setValue(TrialSpawnerBlock.OMINOUS, false));
        isOminous = false;
    }
    public boolean isOminous() {
        return isOminous;
    }
    public int getTargetCooldownLength() {
        return config.targetCooldownLength();
    }
    public int getRequiredPlayerRange() {
        return config.requiredPlayerRange();
    }
    public TrialSpawnerState getState() {
        return stateAccessor.getState();
    }
    public TrialSpawnerStateData getStateData() {
        return data;
    }
    public void setState(Level level, TrialSpawnerState state) {
        stateAccessor.setState(level, state);
    }
    public void markUpdated() {
        stateAccessor.markUpdated();
    }
    public PlayerDetector getPlayerDetector() {
        return playerDetector;
    }
    public PlayerDetector.EntitySelector getEntitySelector() {
        return entitySelector;
    }
    public boolean canSpawnInLevel(ServerLevel level) {
        if (!level.getGameRules().get(GameRules.SPAWNER_BLOCKS_WORK)) {
            return false;
        }
        if (overridePeacefulAndMobSpawnRule) {
            return true;
        }
        return level.getDifficulty() != Difficulty.PEACEFUL && level.getGameRules().get(GameRules.SPAWN_MOBS);
    }
    public Optional<UUID> spawnMob(ServerLevel level, BlockPos spawnerPos) {
        return Optional.empty();
    }
    public void ejectReward(ServerLevel level, BlockPos pos, ResourceKey<LootTable> ejectingLootTable) {
        return;
    }
    public void tickClient(Level level, BlockPos spawnerPos, boolean isOminous) {
        TrialSpawnerState current = getState();
        if (current.hasSpinningMob()) {
            double delay = Math.max(0L, data.nextMobSpawnsAt - level.getGameTime());
            data.oSpin = data.spin;
            data.spin = (data.spin + current.spinningMobSpeed() / (delay + 200.0)) % 360.0;
        }
    }
    public void tickServer(ServerLevel serverLevel, BlockPos spawnerPos, boolean isOminous) {
        this.isOminous = isOminous;
        TrialSpawnerState current = getState();
        TrialSpawnerState next = current.tickAndGetNext(spawnerPos, this, serverLevel);
        if (next != current) {
            setState(serverLevel, next);
        }
    }
    public static void addSpawnParticles(Level level, BlockPos pos, RandomSource random, SimpleParticleType particleType) {
        return;
    }
    public static void addBecomeOminousParticles(Level level, BlockPos pos, RandomSource random) {
        return;
    }
    public static void addDetectPlayerParticles(Level level, BlockPos pos, RandomSource random, int data, ParticleOptions type) {
        return;
    }
    public static void addEjectItemParticles(Level level, BlockPos pos, RandomSource random) {
        return;
    }
    public void overrideEntityToSpawn(EntityType<?> type, Level level) {
        data.reset();
        config = config.overrideEntity(type);
        setState(level, TrialSpawnerState.INACTIVE);
    }
    public void overrideEntityToSpawn(TypedEntityData<EntityType<?>> entityData, Level level) {
        data.reset();
        config = config.overrideEntityData(entityData);
        setState(level, TrialSpawnerState.INACTIVE);
    }
    public void setPlayerDetector(PlayerDetector playerDetector) {
        this.playerDetector = playerDetector;
    }
    public void overridePeacefulAndMobSpawnRule() {
        overridePeacefulAndMobSpawnRule = true;
    }
    public static enum FlameParticle {
        NORMAL, OMINOUS;
        public final SimpleParticleType particleType = null;
        public static FlameParticle decode(int data) {
            FlameParticle[] all = values();
            return data >= 0 && data < all.length ? all[data] : NORMAL;
        }
        public int encode() {
            return ordinal();
        }
    }
    public static record FullConfig(Holder<TrialSpawnerConfig> normal, Holder<TrialSpawnerConfig> ominous, int targetCooldownLength, int requiredPlayerRange) {
        public static final MapCodec<FullConfig> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.block.entity.trialspawner.TrialSpawner.FullConfig.class, "MAP_CODEC");
        public static final FullConfig DEFAULT = new FullConfig(Holder.direct(TrialSpawnerConfig.DEFAULT), Holder.direct(TrialSpawnerConfig.DEFAULT), 36000, 14);
        public FullConfig overrideEntity(EntityType<?> type) {
            return new FullConfig(Holder.direct(normal.value().withSpawning(type)), Holder.direct(ominous.value().withSpawning(type)), targetCooldownLength, requiredPlayerRange);
        }
        public FullConfig overrideEntityData(TypedEntityData<EntityType<?>> entityData) {
            return new FullConfig(Holder.direct(normal.value().withSpawning(entityData)), Holder.direct(ominous.value().withSpawning(entityData)), targetCooldownLength, requiredPlayerRange);
        }
    }
    public interface StateAccessor {
        void setState(Level level, TrialSpawnerState state);
        TrialSpawnerState getState();
        void markUpdated();
    }
}
