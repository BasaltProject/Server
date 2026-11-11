// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import java.util.Optional;
import net.minecraft.world.level.levelgen.Column;
import net.minecraft.world.phys.Vec3;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.world.level.levelgen.Heightmap;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record LargeDripstoneFeature(HolderSet<Block> replaceableBlocks, int floorToCeilingSearchRange, IntProvider columnRadius, FloatProvider heightScale, float maxColumnRadiusToCaveHeightRatio, FloatProvider stalactiteBluntness, FloatProvider stalagmiteBluntness, FloatProvider windSpeed, int minRadiusForWind, float minBluntnessForWind) implements Feature {
    private Cone cone(BlockPos root, boolean up, RandomSource random, int radius, FloatProvider bluntness) {
        return new Cone(root, up, radius, (double) bluntness.sample(random), (double) heightScale.sample(random));
    }
    private static final class Cone {
        private BlockPos root;
        private final boolean up;
        private int radius;
        private final double bluntness;
        private final double scale;

        private Cone(BlockPos root, boolean up, int radius, double bluntness, double scale) {
            this.root = root;
            this.up = up;
            this.radius = radius;
            this.bluntness = bluntness;
            this.scale = scale;
        }

        private int heightAt(float r) {
            return (int) SpeleothemUtils.getSpeleothemHeight((double) r, (double) radius, scale, bluntness);
        }

        private boolean retreat(WorldGenLevel level, Wind wind) {
            while (radius > 1) {
                BlockPos.MutableBlockPos probe = root.mutable();
                int tries = Math.min(10, heightAt(0.0F));
                for (int i = 0; i < tries; i++) {
                    if (level.getBlockState(probe).is(Blocks.LAVA)) {
                        return false;
                    }
                    if (SpeleothemUtils.isCircleMostlyEmbeddedInStone(level, wind.shift(probe), radius)) {
                        root = probe;
                        return true;
                    }
                    probe.move(up ? Direction.DOWN : Direction.UP);
                }
                radius /= 2;
            }
            return false;
        }

        private void fill(WorldGenLevel level, RandomSource random, Wind wind) {
            for (int dx = -radius; dx <= radius; dx++) {
                for (int dz = -radius; dz <= radius; dz++) {
                    float dist = Mth.sqrt((float) (dx * dx + dz * dz));
                    if (dist > (float) radius) {
                        continue;
                    }
                    int height = heightAt(dist);
                    if (height <= 0) {
                        continue;
                    }
                    if ((double) random.nextFloat() < 0.2) {
                        height = (int) ((float) height * Mth.randomBetween(random, 0.8F, 1.0F));
                    }
                    BlockPos.MutableBlockPos pos = root.offset(dx, 0, dz).mutable();
                    boolean leftStone = false;
                    int limit = up ? level.getHeight(Heightmap.Types.WORLD_SURFACE_WG, pos.getX(), pos.getZ()) : Integer.MAX_VALUE;
                    for (int i = 0; i < height && pos.getY() < limit; i++) {
                        BlockPos shifted = wind.shift(pos);
                        if (SpeleothemUtils.isEmptyOrWaterOrLava(level, shifted)) {
                            leftStone = true;
                            level.setBlock(shifted, Blocks.DRIPSTONE_BLOCK.defaultBlockState(), 2);
                        } else if (leftStone && level.getBlockState(shifted).is(BlockTags.BASE_STONE_OVERWORLD)) {
                            break;
                        }
                        pos.move(up ? Direction.UP : Direction.DOWN);
                    }
                }
            }
        }

        private boolean windy(int minRadius, float minBluntness) {
            return radius >= minRadius && bluntness >= (double) minBluntness;
        }
    }
    private static final class Wind {
        private final int originY;
        private final Vec3 speed;
        private final int maxOffset;

        private Wind(int originY, RandomSource random, FloatProvider range, int maxOffset) {
            this.originY = originY;
            this.maxOffset = maxOffset;
            float strength = range.sample(random);
            float heading = Mth.randomBetween(random, 0.0F, (float) Math.PI);
            this.speed = new Vec3((double) (Mth.cos((double) heading) * strength), 0.0D, (double) (Mth.sin((double) heading) * strength));
        }

        private Wind() {
            this.originY = 0;
            this.speed = null;
            this.maxOffset = 0;
        }

        private static Wind calm() {
            return new Wind();
        }

        private BlockPos shift(BlockPos pos) {
            if (speed == null) {
                return pos;
            }
            Vec3 drift = speed.scale((double) (originY - pos.getY()));
            return pos.offset(Mth.clamp(Mth.floor(drift.x), -maxOffset, maxOffset), 0, Mth.clamp(Mth.floor(drift.z), -maxOffset, maxOffset));
        }
    }
    public static final MapCodec<LargeDripstoneFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.LargeDripstoneFeature.class, "CODEC");
    public MapCodec<LargeDripstoneFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!SpeleothemUtils.isEmptyOrWater(level, origin)) {
            return false;
        }
        Optional<Column> scanned = Column.scan(level, origin, floorToCeilingSearchRange, SpeleothemUtils::isEmptyOrWater,
            state -> SpeleothemUtils.isBaseOrLava(state, Blocks.DRIPSTONE_BLOCK, replaceableBlocks));
        if (scanned.isEmpty() || !(scanned.get() instanceof Column.Range range) || range.height() < 4) {
            return false;
        }
        int capRadius = (int) ((float) range.height() * maxColumnRadiusToCaveHeightRatio);
        int maxRadius = Mth.clamp(capRadius, columnRadius.minInclusive(), columnRadius.maxInclusive());
        int radius = Mth.randomBetweenInclusive(random, columnRadius.minInclusive(), maxRadius);
        Cone hanging = cone(origin.atY(range.ceiling() - 1), false, random, radius, stalactiteBluntness);
        Cone rising = cone(origin.atY(range.floor() + 1), true, random, radius, stalagmiteBluntness);
        Wind wind;
        if (hanging.windy(minRadiusForWind, minBluntnessForWind) && rising.windy(minRadiusForWind, minBluntnessForWind)) {
            wind = new Wind(origin.getY(), random, windSpeed, 16 - radius);
        } else {
            wind = Wind.calm();
        }
        boolean hangingOk = hanging.retreat(level, wind);
        boolean risingOk = rising.retreat(level, wind);
        if (hangingOk) {
            hanging.fill(level, random, wind);
        }
        if (risingOk) {
            rising.fill(level, random, wind);
        }
        return true;
    }
    static final class LargeDripstone {
        private LargeDripstone(BlockPos root, boolean pointingUp, int radius, double bluntness, double scale) {
            
        }
    }
    static final class WindOffsetter {
        private WindOffsetter(int originY, RandomSource random, FloatProvider windSpeedRange, int maxOffset) {
            
        }
        private WindOffsetter() {
            
        }
    }
}
