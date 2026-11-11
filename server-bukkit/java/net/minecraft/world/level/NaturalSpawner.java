// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.MobCategory;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.material.FluidState;
import it.unimi.dsi.fastutil.objects.Object2IntMaps;
import net.minecraft.world.attribute.EnvironmentAttributes;
import net.minecraft.world.level.levelgen.structure.structures.NetherFortressStructure;
import net.minecraft.world.level.biome.MobSpawnSettings;
import net.minecraft.world.entity.SpawnPlacements;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.tags.BiomeTags;
import net.minecraft.tags.BlockTags;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.level.levelgen.structure.BuiltinStructures;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.util.random.WeightedList;
import java.util.Optional;
import java.util.ArrayList;
import net.minecraft.world.entity.Entity;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.Vec3;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.attribute.EnvironmentAttributeReader;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.level.storage.LevelData;
import java.util.Objects;
import net.minecraft.resources.ResourceKey;
import net.minecraft.SharedConstants;
import net.minecraft.world.entity.SpawnGroupData;
import java.util.stream.Stream;
public final class NaturalSpawner {
    private static final org.slf4j.Logger kLog = org.slf4j.LoggerFactory.getLogger(NaturalSpawner.class);
    public static final int SPAWN_DISTANCE_CHUNK = 8;
    public static final int SPAWN_DISTANCE_BLOCK = 128;
    public static final int INSCRIBED_SQUARE_SPAWN_DISTANCE_CHUNK = Mth.floor(8.0F / Mth.SQRT_OF_TWO);
    private static final int kChunkAreaCap = 17 * 17;
    private static final MobCategory[] kSpawning = Stream.of(MobCategory.values()).filter(category -> category != MobCategory.MISC).toArray(MobCategory[]::new);
    private NaturalSpawner() {
    }
    public static SpawnState createState(int spawnableChunkCount, ServerLevel level, ChunkGetter chunkGetter, LocalMobCapCalculator localMobCapCalculator) {
        PotentialCalculator potential = new PotentialCalculator();
        Object2IntOpenHashMap<MobCategory> counts = new Object2IntOpenHashMap<>();
        for (Entity entity : level.getAllEntities()) {
            if (entity instanceof Mob mob && (mob.isPersistenceRequired() || mob.requiresCustomPersistence())) {
                continue;
            }
            final MobCategory category = entity.getType().getCategory();
            if (category == MobCategory.MISC) {
                continue;
            }
            BlockPos at = entity.blockPosition();
            chunkGetter.query(ChunkPos.pack(at), chunk -> {
                MobSpawnSettings.MobSpawnCost cost = level.environmentAttributes().getValue(EnvironmentAttributes.NATURAL_MOB_SPAWNS, at).getMobSpawnCost(entity.getType());
                if (cost != null) {
                    potential.addCharge(entity.blockPosition(), cost.charge());
                }
                if (entity instanceof Mob) {
                    localMobCapCalculator.addMob(chunk.getPos(), category);
                }
                counts.addTo(category, 1);
            });
        }
        return new SpawnState(spawnableChunkCount, counts, potential, localMobCapCalculator);
    }
    public static List<MobCategory> getFilteredSpawningCategories(SpawnState state, boolean spawnEnemies, boolean spawnPersistent) {
        List<MobCategory> out = new ArrayList<>(kSpawning.length);
        for (MobCategory category : kSpawning) {
            boolean allowed = (spawnEnemies || category.isFriendly()) && (spawnPersistent || !category.isPersistent());
            if (allowed && state.canSpawnForCategoryGlobal(category)) {
                out.add(category);
            }
        }
        return out;
    }
    public static void spawnForChunk(ServerLevel level, LevelChunk chunk, SpawnState state, List<MobCategory> spawningCategories) {
        for (MobCategory category : spawningCategories) {
            if (state.canSpawnForCategoryLocal(category, chunk.getPos())) {
                spawnCategoryForChunk(category, level, chunk, state::canSpawn, state::afterSpawn);
            }
        }
    }
    public static void spawnCategoryForChunk(MobCategory mobCategory, ServerLevel level, LevelChunk chunk, SpawnPredicate extraTest, AfterSpawnCallback spawnCallback) {
        BlockPos start = randomPosIn(level, chunk);
        if (start.getY() > level.getMinY()) {
            spawnCategoryForPosition(mobCategory, level, chunk, start, extraTest, spawnCallback);
        }
    }
    public static void spawnCategoryForPosition(MobCategory mobCategory, ServerLevel level, BlockPos start) {
        spawnCategoryForPosition(mobCategory, level, level.getChunk(start), start, (type, world, pos, chunk) -> true, (mob, chunk) -> {
        });
    }
    public static void spawnCategoryForPosition(MobCategory mobCategory, ServerLevel level, ChunkAccess chunk, BlockPos start, SpawnPredicate extraTest, AfterSpawnCallback spawnCallback) {
        StructureManager structures = level.structureManager();
        final ChunkGenerator generator = level.getChunkSource().getGenerator();
        int y = start.getY();
        if (chunk.getBlockState(start).isRedstoneConductor(chunk, start)) {
            return;
        }
        final BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        int spawned = 0;
        for (int attempt = 0; attempt < 3; attempt++) {
            int x = start.getX();
            int z = start.getZ();
            MobSpawnSettings.SpawnerData chosen = null;
            SpawnGroupData group = null;
            int limit = Mth.ceil(level.getRandom().nextFloat() * 4.0F);
            int inGroup = 0;
            for (int step = 0; step < limit; step++) {
                x += level.getRandom().nextInt(6) - level.getRandom().nextInt(6);
                z += level.getRandom().nextInt(6) - level.getRandom().nextInt(6);
                pos.set(x, y, z);
                double cx = x + 0.5;
                double cz = z + 0.5;
                Player nearest = level.getNearestPlayer(cx, y, cz, -1.0, false);
                if (nearest == null) {
                    continue;
                }
                double distance = nearest.distanceToSqr(cx, y, cz);
                if (!farEnoughFromPlayer(level, chunk, pos, distance)) {
                    continue;
                }
                if (chosen == null) {
                    Optional<MobSpawnSettings.SpawnerData> next = pickSpawn(level, structures, generator, mobCategory, level.getRandom(), pos);
                    if (next.isEmpty()) {
                        break;
                    }
                    chosen = next.get();
                    limit = chosen.count().sample(level.getRandom());
                }
                if (!positionFits(level, mobCategory, structures, generator, chosen, pos, distance) || !extraTest.test(chosen.type(), level, pos, chunk)) {
                    continue;
                }
                Mob mob = create(level, chosen.type());
                if (mob == null) {
                    return;
                }
                mob.snapTo(cx, y, cz, level.getRandom().nextFloat() * 360.0F, 0.0F);
                if (!mobFits(level, mob, distance)) {
                    continue;
                }
                group = mob.finalizeSpawn(level, level.getCurrentDifficultyAt(mob.blockPosition()), EntitySpawnReason.NATURAL, group);
                spawned++;
                inGroup++;
                level.addFreshEntityWithPassengers(mob);
                spawnCallback.run(mob, chunk);
                if (spawned >= mob.getMaxSpawnClusterSize()) {
                    return;
                }
                if (mob.isMaxGroupSizeReached(inGroup)) {
                    break;
                }
            }
        }
    }
    private static boolean farEnoughFromPlayer(ServerLevel level, ChunkAccess chunk, BlockPos.MutableBlockPos pos, double distanceSqr) {
        if (distanceSqr <= 576.0) {
            return false;
        }
        LevelData.RespawnData respawn = level.getRespawnData();
        Vec3 here = new Vec3(pos.getX() + 0.5, pos.getY(), pos.getZ() + 0.5);
        if (respawn.dimension() == level.dimension() && respawn.pos().closerToCenterThan(here, 24.0)) {
            return false;
        }
        ChunkPos target = ChunkPos.containing(pos);
        return Objects.equals(target, chunk.getPos()) || level.canSpawnEntitiesInChunk(target);
    }
    private static boolean positionFits(ServerLevel level, MobCategory category, StructureManager structures, ChunkGenerator generator, MobSpawnSettings.SpawnerData data, BlockPos.MutableBlockPos pos, double distanceSqr) {
        EntityType<?> type = data.type();
        if (MobCategory.MISC == type.getCategory()) {
            return false;
        }
        int despawn = type.getCategory().getDespawnDistance();
        if (!type.canSpawnFarFromPlayer() && distanceSqr > (double) despawn * despawn) {
            return false;
        }
        if (!type.canSummon() || !spawnsAt(level, structures, generator, category, pos).contains(data)) {
            return false;
        }
        if (!SpawnPlacements.isSpawnPositionOk(type, level, pos) || !SpawnPlacements.checkSpawnRules(type, level, EntitySpawnReason.NATURAL, pos, level.getRandom())) {
            return false;
        }
        return level.noCollision(type.getSpawnAABB(pos.getX() + 0.5, pos.getY(), pos.getZ() + 0.5));
    }
    private static Mob create(ServerLevel level, EntityType<?> type) {
        try {
            if (type.create(level, EntitySpawnReason.NATURAL) instanceof Mob mob) {
                return mob;
            }
            kLog.warn("Can't spawn entity of type: {}", BuiltInRegistries.ENTITY_TYPE.getKey(type));
        } catch (Exception failure) {
            kLog.warn("Failed to create mob", failure);
        }
        return null;
    }
    private static boolean mobFits(ServerLevel level, Mob mob, double distanceSqr) {
        int despawn = mob.getType().getCategory().getDespawnDistance();
        if (distanceSqr > (double) despawn * despawn && mob.removeWhenFarAway(distanceSqr)) {
            return false;
        }
        boolean rulesOk = mob.checkSpawnRules(level, EntitySpawnReason.NATURAL);
        return rulesOk && mob.checkSpawnObstruction(level);
    }
    private static Optional<MobSpawnSettings.SpawnerData> pickSpawn(ServerLevel level, StructureManager structures, ChunkGenerator generator, MobCategory category, RandomSource random, BlockPos pos) {
        boolean sparse = category == MobCategory.WATER_AMBIENT && level.getBiome(pos).is(BiomeTags.REDUCED_WATER_AMBIENT_SPAWNS);
        if (sparse && random.nextFloat() < 0.98F) {
            return Optional.empty();
        }
        return spawnsAt(level, structures, generator, category, pos).getRandom(random);
    }
    private static WeightedList<MobSpawnSettings.SpawnerData> spawnsAt(ServerLevel level, StructureManager structures, ChunkGenerator generator, MobCategory category, BlockPos pos) {
        return isInNetherFortressBounds(pos, level, category, structures) ? NetherFortressStructure.FORTRESS_ENEMIES : generator.getMobsAt(level, structures, category, pos);
    }
    public static boolean isInNetherFortressBounds(BlockPos pos, ServerLevel level, MobCategory category, StructureManager structureManager) {
        if (category != MobCategory.MONSTER || !level.getBlockState(pos.below()).is(Blocks.NETHER_BRICKS)) {
            return false;
        }
        Structure fortress = fortressOf(structureManager);
        return fortress != null && structureManager.getStructureAt(pos, fortress).isValid();
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Structure fortressOf(StructureManager structures) {
        return (Structure) structures.registryAccess().lookupOrThrow((ResourceKey) Registries.STRUCTURE).getValue((ResourceKey) BuiltinStructures.FORTRESS);
    }
    private static BlockPos randomPosIn(Level level, LevelChunk chunk) {
        ChunkPos chunkPos = chunk.getPos();
        int x = chunkPos.getMinBlockX() + level.getRandom().nextInt(16);
        int z = chunkPos.getMinBlockZ() + level.getRandom().nextInt(16);
        int top = chunk.getHeight(Heightmap.Types.WORLD_SURFACE, x, z) + 1;
        return new BlockPos(x, Mth.randomBetweenInclusive(level.getRandom(), level.getMinY(), top), z);
    }
    public static boolean isValidEmptySpawnBlock(BlockGetter level, BlockPos pos, BlockState blockState, FluidState fluidState, EntityType<?> type) {
        return !blockState.isCollisionShapeFullBlock(level, pos) && !blockState.isSignalSource() && fluidState.isEmpty()
            && !blockState.is(BlockTags.PREVENT_MOB_SPAWNING_INSIDE) && !type.isBlockDangerous(blockState);
    }
    public static void spawnMobsForChunkGeneration(ServerLevelAccessor level, BlockPos sourcePos, ChunkPos chunkPos, RandomSource random) {
        if (!level.getLevel().getGameRules().get(GameRules.SPAWN_MOBS)) {
            return;
        }
        final EnvironmentAttributeReader attributes = level.environmentAttributes();
        WeightedList<MobSpawnSettings.SpawnerData> mobs = attributes.getValue(EnvironmentAttributes.NATURAL_MOB_SPAWNS, sourcePos).getMobsToSpawn(MobCategory.CREATURE);
        if (mobs.isEmpty()) {
            return;
        }
        float probability = attributes.getValue(EnvironmentAttributes.CREATURE_WORLD_GEN_SPAWN_PROBABILITY, sourcePos);
        int minX = chunkPos.getMinBlockX();
        int minZ = chunkPos.getMinBlockZ();
        while (random.nextFloat() < probability) {
            Optional<MobSpawnSettings.SpawnerData> pick = mobs.getRandom(random);
            if (pick.isEmpty()) {
                continue;
            }
            MobSpawnSettings.SpawnerData data = pick.get();
            int count = data.count().sample(random);
            SpawnGroupData group = null;
            int x = minX + random.nextInt(16);
            int z = minZ + random.nextInt(16);
            int homeX = x;
            int homeZ = z;
            for (int each = 0; each < count; each++) {
                boolean placed = false;
                for (int tries = 0; !placed && tries < 4; tries++) {
                    BlockPos pos = topFreePos(level, data.type(), x, z);
                    if (data.type().canSummon() && SpawnPlacements.isSpawnPositionOk(data.type(), level, pos)) {
                        float width = data.type().getWidth();
                        double fx = Mth.clamp((double) x, minX + (double) width, minX + 16.0 - width);
                        double fz = Mth.clamp((double) z, minZ + (double) width, minZ + 16.0 - width);
                        placed = tryPlaceForGeneration(level, data.type(), pos, fx, fz, random, group);
                        if (placed) {
                            group = lastGroup;
                        }
                    }
                    x += random.nextInt(5);
                    x -= random.nextInt(5);
                    z += random.nextInt(5) - random.nextInt(5);
                    while (x < minX || x >= minX + 16 || z < minZ || z >= minZ + 16) {
                        x = homeX + random.nextInt(5) - random.nextInt(5);
                        z = homeZ + random.nextInt(5) - random.nextInt(5);
                    }
                }
            }
        }
    }
    private static SpawnGroupData lastGroup;
    private static boolean tryPlaceForGeneration(ServerLevelAccessor level, EntityType<?> type, BlockPos pos, double fx, double fz, RandomSource random, SpawnGroupData group) {
        if (!level.noCollision(type.getSpawnAABB(fx, pos.getY(), fz))
            || !SpawnPlacements.checkSpawnRules(type, level, EntitySpawnReason.CHUNK_GENERATION, BlockPos.containing(fx, pos.getY(), fz), level.getRandom())) {
            return false;
        }
        Entity entity;
        try {
            entity = type.create(level.getLevel(), EntitySpawnReason.NATURAL);
        } catch (Exception failure) {
            kLog.warn("Failed to create mob", failure);
            return false;
        }
        if (!(entity instanceof Mob mob)) {
            return false;
        }
        mob.snapTo(fx, pos.getY(), fz, random.nextFloat() * 360.0F, 0.0F);
        if (!mob.checkSpawnRules(level, EntitySpawnReason.CHUNK_GENERATION) || !mob.checkSpawnObstruction(level)) {
            return false;
        }
        lastGroup = mob.finalizeSpawn(level, level.getCurrentDifficultyAt(mob.blockPosition()), EntitySpawnReason.CHUNK_GENERATION, group);
        level.addFreshEntityWithPassengers(mob);
        return true;
    }
    private static BlockPos topFreePos(LevelReader level, EntityType<?> type, int x, int z) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos(x, level.getHeight(SpawnPlacements.getHeightmapType(type), x, z), z);
        if (level.dimensionType().hasCeiling()) {
            do {
                pos.move(Direction.DOWN);
            } while (!level.getBlockState(pos).isAir());
            do {
                pos.move(Direction.DOWN);
            } while (level.getBlockState(pos).isAir() && pos.getY() > level.getMinY());
        }
        BlockPos fixed = pos.immutable();
        return SpawnPlacements.getPlacementType(type).adjustSpawnPosition(level, fixed);
    }
    public static class SpawnState {
        private final int spawnableChunkCount;
        private final Object2IntOpenHashMap<MobCategory> categoryCounts;
        private final PotentialCalculator potential;
        private final Object2IntMap<MobCategory> readOnlyCounts;
        private final LocalMobCapCalculator localCaps;
        private BlockPos lastPos;
        private EntityType<?> lastType;
        private double lastCharge;
        private SpawnState(int spawnableChunkCount, Object2IntOpenHashMap<MobCategory> mobCategoryCounts, PotentialCalculator spawnPotential, LocalMobCapCalculator localMobCapCalculator) {
            this.spawnableChunkCount = spawnableChunkCount;
            this.categoryCounts = mobCategoryCounts;
            this.potential = spawnPotential;
            this.localCaps = localMobCapCalculator;
            this.readOnlyCounts = Object2IntMaps.unmodifiable(mobCategoryCounts);
        }
        private boolean canSpawn(EntityType<?> type, ServerLevel level, BlockPos testPos, ChunkAccess chunk) {
            lastPos = testPos;
            lastType = type;
            MobSpawnSettings.MobSpawnCost cost = level.environmentAttributes().getValue(EnvironmentAttributes.NATURAL_MOB_SPAWNS, testPos).getMobSpawnCost(type);
            if (cost == null) {
                lastCharge = 0.0;
                return true;
            }
            lastCharge = cost.charge();
            return potential.getPotentialEnergyChange(testPos, lastCharge) <= cost.energyBudget();
        }
        private void afterSpawn(Mob mob, ChunkAccess chunk) {
            EntityType<?> type = mob.getType();
            BlockPos pos = mob.blockPosition();
            double charge = 0.0;
            if (pos.equals(lastPos) && type == lastType) {
                charge = lastCharge;
            } else {
                MobSpawnSettings.MobSpawnCost cost = mob.level().environmentAttributes().getValue(EnvironmentAttributes.NATURAL_MOB_SPAWNS, pos).getMobSpawnCost(type);
                if (cost != null) {
                    charge = cost.charge();
                }
            }
            potential.addCharge(pos, charge);
            categoryCounts.addTo(type.getCategory(), 1);
            localCaps.addMob(ChunkPos.containing(pos), type.getCategory());
        }
        public int getSpawnableChunkCount() {
            return spawnableChunkCount;
        }
        public Object2IntMap<MobCategory> getMobCategoryCounts() {
            return readOnlyCounts;
        }
        private boolean canSpawnForCategoryGlobal(MobCategory category) {
            return categoryCounts.getInt(category) < category.getMaxInstancesPerChunk() * spawnableChunkCount / kChunkAreaCap;
        }
        private boolean canSpawnForCategoryLocal(MobCategory category, ChunkPos chunkPos) {
            return localCaps.canSpawn(category, chunkPos) || SharedConstants.DEBUG_IGNORE_LOCAL_MOB_CAP;
        }
    }
    @FunctionalInterface
    public interface AfterSpawnCallback {
        void run(Mob mob, ChunkAccess levelChunk);
    }
    @FunctionalInterface
    public interface ChunkGetter {
        void query(long chunkKey, Consumer<LevelChunk> output);
    }
    @FunctionalInterface
    public interface SpawnPredicate {
        boolean test(EntityType<?> type, ServerLevel level, BlockPos blockPos, ChunkAccess levelChunk);
    }
}
