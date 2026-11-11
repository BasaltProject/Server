// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.carver;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.CarverOutput;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.heightproviders.HeightProvider;
public record CaveWorldCarver(float probability, HeightProvider y, IntProvider count, FloatProvider thickness, boolean weirdThicknessBias, FloatProvider roomVerticalRadiusMultiplier, FloatProvider horizontalRadiusMultiplier, FloatProvider verticalRadiusMultiplier, FloatProvider startVerticalRadiusMultiplier, FloatProvider floorLevel) implements WorldCarver {
    public static final MapCodec<CaveWorldCarver> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.carver.CaveWorldCarver.class, "MAP_CODEC");
    public boolean isStartChunk(RandomSource random) {
        return random.nextFloat() <= this.probability;
    }
    public boolean carve(WorldGenerationContext context, RandomSource random, ChunkPos chunkPos, ChunkPos sourceChunkPos, CarverOutput output) {
        int maxDistance = (getRange() * 2 - 1) * 16;
        int caveCount = this.count.sample(random);
        for (int cave = 0; cave < caveCount; cave++) {
            double x = sourceChunkPos.getBlockX(random.nextInt(16));
            double y = this.y.sample(random, context);
            double z = sourceChunkPos.getBlockZ(random.nextInt(16));
            double hMul = this.horizontalRadiusMultiplier.sample(random);
            double vMul = this.verticalRadiusMultiplier.sample(random);
            double startMul = this.startVerticalRadiusMultiplier.sample(random);
            double floor = this.floorLevel.sample(random);
            WorldCarver.CarveSkipChecker skip = (xd, yd, zd, wy) -> belowFloorOrOutside(xd, yd, zd, floor);
            int tunnels = 1;
            if (random.nextInt(4) == 0) {
                double yScale = this.roomVerticalRadiusMultiplier.sample(random);
                float size = 1.0F + random.nextFloat() * 6.0F;
                room(chunkPos, x, y, z, size, yScale, output, skip);
                tunnels += random.nextInt(4);
            }
            for (int i = 0; i < tunnels; i++) {
                float yaw = random.nextFloat() * ((float) Math.PI * 2F);
                float pitch = (random.nextFloat() - 0.5F) / 4.0F;
                float size = tunnelThickness(random);
                int length = maxDistance - random.nextInt(maxDistance / 4);
                tunnel(chunkPos, random.nextLong(), x, y, z, hMul, vMul, size, yaw, pitch, 0, length, startMul, output, skip);
            }
        }
        return true;
    }
    private float tunnelThickness(RandomSource random) {
        float size = this.thickness.sample(random);
        if (this.weirdThicknessBias && random.nextInt(10) == 0) {
            size *= random.nextFloat() * random.nextFloat() * 3.0F + 1.0F;
        }
        return size;
    }

    private void room(ChunkPos chunkPos, double x, double y, double z, float size, double yScale, CarverOutput output,
        WorldCarver.CarveSkipChecker skip) {
        double radius = 1.5 + Mth.sin((float) Math.PI / 2F) * size;
        WorldCarver.carveEllipsoid(chunkPos, x + 1.0, y, z, radius, radius * yScale, output, skip);
    }

    private void tunnel(ChunkPos chunkPos, long seed, double x, double y, double z, double hMul, double vMul, float size,
        float yaw, float pitch, int step, int length, double yScale, CarverOutput output, WorldCarver.CarveSkipChecker skip) {
        RandomSource random = RandomSource.createThreadLocalInstance(seed);
        int splitAt = random.nextInt(length / 2) + length / 4;
        boolean steep = random.nextInt(6) == 0;
        float yawDelta = 0.0F;
        float pitchDelta = 0.0F;
        for (int at = step; at < length; at++) {
            double hRadius = 1.5 + Mth.sin((float) Math.PI * at / length) * size;
            double vRadius = hRadius * yScale;
            float cosPitch = Mth.cos(pitch);
            x += Mth.cos(yaw) * cosPitch;
            y += Mth.sin(pitch);
            z += Mth.sin(yaw) * cosPitch;
            pitch *= steep ? 0.92F : 0.7F;
            pitch += pitchDelta * 0.1F;
            yaw += yawDelta * 0.1F;
            pitchDelta *= 0.9F;
            yawDelta *= 0.75F;
            pitchDelta += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0F;
            yawDelta += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0F;
            if (at == splitAt && size > 1.0F) {
                for (int sign = -1; sign <= 1; sign += 2) {
                    long branchSeed = random.nextLong();
                    float branchSize = random.nextFloat() * 0.5F + 0.5F;
                    tunnel(chunkPos, branchSeed, x, y, z, hMul, vMul, branchSize, yaw + sign * ((float) Math.PI / 2F), pitch / 3.0F, at, length, 1.0, output, skip);
                }
                return;
            }
            if (random.nextInt(4) != 0) {
                if (!WorldCarver.canReach(chunkPos, x, z, at, length, size)) {
                    return;
                }
                WorldCarver.carveEllipsoid(chunkPos, x, y, z, hRadius * hMul, vRadius * vMul, output, skip);
            }
        }
    }

    private static boolean belowFloorOrOutside(double xd, double yd, double zd, double floorLevel) {
        return yd <= floorLevel || xd * xd + yd * yd + zd * zd >= 1.0;
    }

    public MapCodec<CaveWorldCarver> codec() {
        return MAP_CODEC;
    }
}
