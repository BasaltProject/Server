// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.util.Mth;
import net.minecraft.world.level.LevelAccessor;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record IcebergFeature(BlockState state) implements Feature {
    private static boolean isIce(BlockState s) {
        return s.is(Blocks.PACKED_ICE) || s.is(Blocks.SNOW_BLOCK) || s.is(Blocks.BLUE_ICE);
    }
    private static double ellipseDist(int xo, int zo, BlockPos centre, int a, int c, double angle) {
        double dx = xo - centre.getX();
        double dz = zo - centre.getZ();
        return Math.pow((dx * Math.cos(angle) - dz * Math.sin(angle)) / (double) a, 2.0D)
            + Math.pow((dx * Math.sin(angle) + dz * Math.cos(angle)) / (double) c, 2.0D) - 1.0D;
    }
    private static double circleDist(int xo, int zo, BlockPos centre, int radius, RandomSource random) {
        float jitter = 10.0F * Mth.clamp(random.nextFloat(), 0.2F, 0.8F) / (float) radius;
        return (double) jitter + Math.pow((double) (xo - centre.getX()), 2.0D) + Math.pow((double) (zo - centre.getZ()), 2.0D) - Math.pow((double) radius, 2.0D);
    }
    private static int radiusRound(RandomSource random, int y, int height, int width) {
        float k = 3.5F - random.nextFloat();
        float scale = (1.0F - (float) Math.pow((double) y, 2.0D) / ((float) height * k)) * (float) width;
        if (height > 15 + random.nextInt(5)) {
            int shifted = y < 3 + random.nextInt(6) ? y / 2 : y;
            scale = (1.0F - (float) shifted / ((float) height * k * 0.4F)) * (float) width;
        }
        return Mth.ceil(scale / 2.0F);
    }
    private static int radiusEllipse(int y, int height, int width) {
        float scale = (1.0F - (float) Math.pow((double) y, 2.0D) / ((float) height * 1.0F)) * (float) width;
        return Mth.ceil(scale / 2.0F);
    }
    private static int radiusSteep(RandomSource random, int y, int height, int width) {
        float k = 1.0F + random.nextFloat() / 2.0F;
        float scale = (1.0F - (float) y / ((float) height * k)) * (float) width;
        return Mth.ceil(scale / 2.0F);
    }
    private static int ellipseC(int y, int height, int c) {
        return y > 0 && height - y <= 3 ? c - (4 - (height - y)) : c;
    }
    private void putBlock(BlockPos pos, LevelAccessor level, RandomSource random, int diff, int height, boolean ellipse, boolean snowTop, BlockState main) {
        BlockState found = level.getBlockState(pos);
        if (!(found.isAir() || found.is(Blocks.SNOW_BLOCK) || found.is(Blocks.ICE) || found.is(Blocks.WATER))) {
            return;
        }
        boolean luck = !ellipse || random.nextDouble() > 0.05;
        int divisor = ellipse ? 3 : 2;
        if (snowTop && !found.is(Blocks.WATER) && (double) diff <= (double) random.nextInt(Math.max(1, height / divisor)) + (double) height * 0.6 && luck) {
            setBlock(level, pos, Blocks.SNOW_BLOCK.defaultBlockState());
        } else {
            setBlock(level, pos, main);
        }
    }
    private void icebergBlock(LevelAccessor level, RandomSource random, BlockPos origin, int height, int xo, int yOff, int zo, int radius, int a,
        boolean ellipse, int cShape, double angle, boolean snowTop, BlockState main) {
        double dist = ellipse ? ellipseDist(xo, zo, BlockPos.ZERO, a, ellipseC(yOff, height, cShape), angle) : circleDist(xo, zo, BlockPos.ZERO, radius, random);
        if (dist >= 0.0D) {
            return;
        }
        double limit = ellipse ? -0.5D : (double) (-6 - random.nextInt(3));
        if (dist > limit && random.nextDouble() > 0.9) {
            return;
        }
        putBlock(origin.offset(xo, yOff, zo), level, random, height - yOff, height, ellipse, snowTop, main);
    }
    private void carve(int radius, int yOff, BlockPos origin, LevelAccessor level, boolean wet, double angle, BlockPos local, int aShape, int cShape) {
        int a = radius + 1 + aShape / 3;
        int c = Math.min(radius - 3, 3) + cShape / 2 - 1;
        for (int xo = -a; xo < a; xo++) {
            for (int zo = -a; zo < a; zo++) {
                if (ellipseDist(xo, zo, local, a, c, angle) >= 0.0D) {
                    continue;
                }
                BlockPos pos = origin.offset(xo, yOff, zo);
                BlockState found = level.getBlockState(pos);
                if (isIce(found) || found.is(Blocks.SNOW_BLOCK)) {
                    if (wet) {
                        setBlock(level, pos, Blocks.WATER.defaultBlockState());
                    } else {
                        setBlock(level, pos, Blocks.AIR.defaultBlockState());
                        if (level.getBlockState(pos.above()).is(Blocks.SNOW)) {
                            setBlock(level, pos.above(), Blocks.AIR.defaultBlockState());
                        }
                    }
                }
            }
        }
    }
    private void cutOut(RandomSource random, LevelAccessor level, int width, int height, BlockPos origin, boolean ellipse, int aShape, double angle, int cShape) {
        int signX = random.nextBoolean() ? -1 : 1;
        int signZ = random.nextBoolean() ? -1 : 1;
        int offX = random.nextInt(Math.max(width / 2 - 2, 1));
        if (random.nextBoolean()) {
            offX = width / 2 + 1 - random.nextInt(Math.max(width - width / 2 - 1, 1));
        }
        int offZ = random.nextInt(Math.max(width / 2 - 2, 1));
        if (random.nextBoolean()) {
            offZ = width / 2 + 1 - random.nextInt(Math.max(width - width / 2 - 1, 1));
        }
        if (ellipse) {
            offX = offZ = random.nextInt(Math.max(aShape - 5, 1));
        }
        BlockPos local = new BlockPos(signX * offX, 0, signZ * offZ);
        double cutAngle = ellipse ? angle + (Math.PI / 2D) : random.nextDouble() * 2.0D * Math.PI;
        for (int y = 0; y < height - 3; y++) {
            carve(radiusRound(random, y, height, width), y, origin, level, false, cutAngle, local, aShape, cShape);
        }
        for (int y = -1; y > -height + random.nextInt(5); y--) {
            carve(radiusSteep(random, -y, height, width), y, origin, level, true, cutAngle, local, aShape, cShape);
        }
    }
    private void smoothOut(LevelAccessor level, BlockPos origin, int width, int height, boolean ellipse, int aShape) {
        int a = ellipse ? aShape : width / 2;
        for (int x = -a; x <= a; x++) {
            for (int z = -a; z <= a; z++) {
                for (int y = 0; y <= height; y++) {
                    BlockPos pos = origin.offset(x, y, z);
                    BlockState found = level.getBlockState(pos);
                    if (!isIce(found) && !found.is(Blocks.SNOW)) {
                        continue;
                    }
                    if (level.getBlockState(pos.below()).isAir()) {
                        setBlock(level, pos, Blocks.AIR.defaultBlockState());
                        setBlock(level, pos.above(), Blocks.AIR.defaultBlockState());
                    } else if (isIce(found)) {
                        int open = 0;
                        for (BlockPos side : new BlockPos[] {pos.west(), pos.east(), pos.north(), pos.south()}) {
                            if (!isIce(level.getBlockState(side))) {
                                open++;
                            }
                        }
                        if (open >= 3) {
                            setBlock(level, pos, Blocks.AIR.defaultBlockState());
                        }
                    }
                }
            }
        }
    }
    public static final MapCodec<IcebergFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.IcebergFeature.class, "CODEC");
    public MapCodec<IcebergFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockPos base = new BlockPos(origin.getX(), chunkGenerator.getSeaLevel(), origin.getZ());
        boolean snowTop = random.nextDouble() > 0.7;
        BlockState main = state;
        double angle = random.nextDouble() * 2.0D * Math.PI;
        int ellA = 11 - random.nextInt(5);
        int ellC = 3 + random.nextInt(3);
        boolean ellipse = random.nextDouble() > 0.7;
        int over = ellipse ? random.nextInt(6) + 6 : random.nextInt(15) + 3;
        if (!ellipse && random.nextDouble() > 0.9) {
            over += random.nextInt(19) + 7;
        }
        int under = Math.min(over + random.nextInt(11), 18);
        int width = Math.min(over + random.nextInt(7) - random.nextInt(5), 11);
        int a = ellipse ? ellA : 11;
        for (int xo = -a; xo < a; xo++) {
            for (int zo = -a; zo < a; zo++) {
                for (int y = 0; y < over; y++) {
                    int radius = ellipse ? radiusEllipse(y, over, width) : radiusRound(random, y, over, width);
                    if (ellipse || xo < radius) {
                        icebergBlock(level, random, base, over, xo, y, zo, radius, a, ellipse, ellC, angle, snowTop, main);
                    }
                }
            }
        }
        smoothOut(level, base, width, over, ellipse, ellA);
        for (int xo = -a; xo < a; xo++) {
            for (int zo = -a; zo < a; zo++) {
                for (int y = -1; y > -under; y--) {
                    int reach = ellipse ? Mth.ceil((float) a * (1.0F - (float) Math.pow((double) y, 2.0D) / ((float) under * 8.0F))) : a;
                    int radius = radiusSteep(random, -y, under, width);
                    if (xo < radius) {
                        icebergBlock(level, random, base, under, xo, y, zo, radius, reach, ellipse, ellC, angle, snowTop, main);
                    }
                }
            }
        }
        boolean cut = ellipse ? random.nextDouble() > 0.1 : random.nextDouble() > 0.7;
        if (cut) {
            cutOut(random, level, width, over, base, ellipse, ellA, angle, ellC);
        }
        return true;


    }
}
