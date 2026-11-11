// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.entity.boss.enderdragon.EndCrystal;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.util.Mth;
import net.minecraft.world.level.ServerLevelAccessor;
import com.google.common.cache.CacheLoader;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.phys.AABB;
public record EndSpikeFeature(List<EndSpike> spikes, boolean crystalInvulnerable, Optional<BlockPos> crystalBeamTarget) implements Feature {
    private static final java.util.Map<Long, List<EndSpike>> kCache = new java.util.concurrent.ConcurrentHashMap<>();
    private void raise(ServerLevelAccessor level, RandomSource random, EndSpike spike) {
        int radius = spike.getRadius();
        BlockPos low = new BlockPos(spike.getCenterX() - radius, level.getMinY(), spike.getCenterZ() - radius);
        BlockPos high = new BlockPos(spike.getCenterX() + radius, spike.getHeight() + 10, spike.getCenterZ() + radius);
        for (BlockPos spot : BlockPos.betweenClosed(low, high)) {
            if (spot.distToLowCornerSqr((double) spike.getCenterX(), (double) spot.getY(), (double) spike.getCenterZ()) <= (double) (radius * radius + 1)
                && spot.getY() < spike.getHeight()) {
                setBlock(level, spot, Blocks.OBSIDIAN.defaultBlockState());
            } else if (spot.getY() > 65) {
                setBlock(level, spot, Blocks.AIR.defaultBlockState());
            }
        }
        if (spike.isGuarded()) {
            BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
            for (int dx = -2; dx <= 2; dx++) {
                for (int dz = -2; dz <= 2; dz++) {
                    for (int dy = 0; dy <= 3; dy++) {
                        boolean sideX = Mth.abs(dx) == 2;
                        boolean sideZ = Mth.abs(dz) == 2;
                        boolean roof = dy == 3;
                        if (!sideX && !sideZ && !roof) {
                            continue;
                        }
                        boolean spanX = dx == -2 || dx == 2 || roof;
                        boolean spanZ = dz == -2 || dz == 2 || roof;
                        BlockState bars = Blocks.IRON_BARS.defaultBlockState();
                        bars = org.basaltmc.bukkit.facade.StateProps.with(bars, "north", spanX && dz != -2);
                        bars = org.basaltmc.bukkit.facade.StateProps.with(bars, "south", spanX && dz != 2);
                        bars = org.basaltmc.bukkit.facade.StateProps.with(bars, "west", spanZ && dx != -2);
                        bars = org.basaltmc.bukkit.facade.StateProps.with(bars, "east", spanZ && dx != 2);
                        setBlock(level, cursor.set(spike.getCenterX() + dx, spike.getHeight() + dy, spike.getCenterZ() + dz), bars);
                    }
                }
            }
        }
        if (net.minecraft.world.entity.EntityTypes.END_CRYSTAL.create(level.getLevel(), net.minecraft.world.entity.EntitySpawnReason.STRUCTURE) instanceof EndCrystal crystal) {
            crystal.setBeamTarget(crystalBeamTarget.orElse(null));
            crystal.setPermanentlyInvulnerable(crystalInvulnerable);
            crystal.snapTo((double) spike.getCenterX() + 0.5D, (double) (spike.getHeight() + 1), (double) spike.getCenterZ() + 0.5D, random.nextFloat() * 360.0F, 0.0F);
            level.addFreshEntity(crystal);
            BlockPos at = crystal.blockPosition();
            setBlock(level, at.below(), Blocks.BEDROCK.defaultBlockState());
            setBlock(level, at, net.minecraft.world.level.block.FireBlock.getState(level, at));
        }
    }
    public static final MapCodec<EndSpikeFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.EndSpikeFeature.class, "CODEC");
    public static List<EndSpike> getSpikesForLevel(WorldGenLevel level) {
        RandomSource random = RandomSource.createThreadLocalInstance(level.getSeed());
        long key = random.nextLong() & 65535L;
        return kCache.computeIfAbsent(key, k -> new SpikeCacheLoader().load(k));
    }
    public MapCodec<EndSpikeFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        List<EndSpike> list = spikes;
        if (list.isEmpty()) {
            list = getSpikesForLevel(level);
        }
        for (EndSpike spike : list) {
            if (spike.isCenterWithinChunk(origin)) {
                raise(level, random, spike);
            }
        }
        return true;
    }
    public static class EndSpike {
        private final int centerX;
        private final int centerZ;
        private final int radius;
        private final int height;
        private final boolean guarded;
        private final AABB topBoundingBox;
        public static final Codec<EndSpike> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.EndSpikeFeature.EndSpike.class, "CODEC");
        public EndSpike(int centerX, int centerZ, int radius, int height, boolean guarded) {
            this.centerX = centerX;
            this.centerZ = centerZ;
            this.radius = radius;
            this.height = height;
            this.guarded = guarded;
            this.topBoundingBox = new net.minecraft.world.phys.AABB((double) (centerX - radius), (double) net.minecraft.world.level.dimension.DimensionType.MIN_Y, (double) (centerZ - radius),
                (double) (centerX + radius), (double) net.minecraft.world.level.dimension.DimensionType.MAX_Y, (double) (centerZ + radius));
        }
        public boolean isCenterWithinChunk(BlockPos chunkOrigin) {
            return net.minecraft.core.SectionPos.blockToSectionCoord(chunkOrigin.getX()) == net.minecraft.core.SectionPos.blockToSectionCoord(centerX)
                && net.minecraft.core.SectionPos.blockToSectionCoord(chunkOrigin.getZ()) == net.minecraft.core.SectionPos.blockToSectionCoord(centerZ);
        }
        public int getCenterX() {
            return centerX;
        }
        public int getCenterZ() {
            return centerZ;
        }
        public int getRadius() {
            return radius;
        }
        public int getHeight() {
            return height;
        }
        public boolean isGuarded() {
            return guarded;
        }
        public AABB getTopBoundingBox() {
            return topBoundingBox;
        }
    }
    static class SpikeCacheLoader extends CacheLoader<Long, List<EndSpike>> {
        public List<EndSpike> load(Long seed) {
            List<Integer> order = new java.util.ArrayList<>();
            for (int i = 0; i < 10; i++) {
                order.add(i);
            }
            net.minecraft.util.Util.shuffle(order, RandomSource.createThreadLocalInstance(seed));
            List<EndSpike> out = new java.util.ArrayList<>();
            for (int i = 0; i < 10; i++) {
                double angle = 2.0D * (-Math.PI + (Math.PI / 10.0D) * (double) i);
                int x = Mth.floor(42.0D * Math.cos(angle));
                int z = Mth.floor(42.0D * Math.sin(angle));
                int size = order.get(i);
                out.add(new EndSpike(x, z, 2 + size / 3, 76 + size * 3, size == 1 || size == 2));
            }
            return out;


        }
    }
}
