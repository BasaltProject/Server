// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.carver;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.CarverOutput;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.heightproviders.HeightProvider;
public record CanyonWorldCarver(float probability, HeightProvider y, FloatProvider verticalRotation, Shape shape) implements WorldCarver {
    public static final MapCodec<CanyonWorldCarver> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.carver.CanyonWorldCarver.class, "MAP_CODEC");
    public boolean isStartChunk(RandomSource random) {
        return random.nextFloat() <= this.probability;
    }
    public boolean carve(WorldGenerationContext context, RandomSource random, ChunkPos chunkPos, ChunkPos sourceChunkPos, CarverOutput output) {
        int maxDistance = (getRange() * 2 - 1) * 16;
        double x = sourceChunkPos.getBlockX(random.nextInt(16));
        int y = this.y.sample(random, context);
        double z = sourceChunkPos.getBlockZ(random.nextInt(16));
        float yaw = random.nextFloat() * ((float) Math.PI * 2F);
        float pitch = this.verticalRotation.sample(random);
        double yScale = this.shape.yScale().sample(random);
        float size = this.shape.thickness().sample(random);
        int length = (int) (maxDistance * this.shape.distanceFactor().sample(random));
        walk(context, chunkPos, random.nextLong(), x, y, z, size, yaw, pitch, 0, length, yScale, output);
        return true;
    }
    private void walk(WorldGenerationContext context, ChunkPos chunkPos, long seed, double x, double y, double z,
        float size, float yaw, float pitch, int step, int length, double yScale, CarverOutput output) {
        RandomSource random = RandomSource.createThreadLocalInstance(seed);
        float[] widths = widthFactors(context, random);
        float yawDelta = 0.0F;
        float pitchDelta = 0.0F;
        for (int at = step; at < length; at++) {
            double hRadius = 1.5 + Mth.sin(at * (float) Math.PI / length) * size;
            double vRadius = hRadius * yScale;
            hRadius *= this.shape.horizontalRadiusFactor().sample(random);
            vRadius = scaleVertical(random, vRadius, length, at);
            float cosPitch = Mth.cos(pitch);
            float sinPitch = Mth.sin(pitch);
            x += Mth.cos(yaw) * cosPitch;
            y += sinPitch;
            z += Mth.sin(yaw) * cosPitch;
            pitch *= 0.7F;
            pitch += pitchDelta * 0.05F;
            yaw += yawDelta * 0.05F;
            pitchDelta *= 0.8F;
            yawDelta *= 0.5F;
            pitchDelta += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0F;
            yawDelta += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0F;
            if (random.nextInt(4) != 0) {
                if (!WorldCarver.canReach(chunkPos, x, z, at, length, size)) {
                    return;
                }
                WorldCarver.carveEllipsoid(chunkPos, x, y, z, hRadius, vRadius, output,
                    (xd, yd, zd, wy) -> outside(context, widths, xd, yd, zd, wy));
            }
        }
    }

    private float[] widthFactors(WorldGenerationContext context, RandomSource random) {
        float[] widths = new float[context.getGenDepth()];
        float factor = 1.0F;
        for (int i = 0; i < widths.length; i++) {
            if (i == 0 || random.nextInt(this.shape.widthSmoothness()) == 0) {
                factor = 1.0F + random.nextFloat() * random.nextFloat();
            }
            widths[i] = factor * factor;
        }
        return widths;
    }

    private double scaleVertical(RandomSource random, double radius, float length, float at) {
        float middle = 1.0F - Mth.abs(0.5F - at / length) * 2.0F;
        float factor = this.shape.verticalRadiusDefaultFactor() + this.shape.verticalRadiusCenterFactor() * middle;
        return factor * radius * Mth.randomBetween(random, 0.75F, 1.0F);
    }

    private boolean outside(WorldGenerationContext context, float[] widths, double xd, double yd, double zd, int y) {
        int index = y - context.getMinGenY();
        return (xd * xd + zd * zd) * widths[index - 1] + yd * yd / 6.0 >= 1.0;
    }

    public MapCodec<CanyonWorldCarver> codec() {
        return MAP_CODEC;
    }
    public static record Shape(FloatProvider distanceFactor, FloatProvider thickness, int widthSmoothness, FloatProvider horizontalRadiusFactor, float verticalRadiusDefaultFactor, float verticalRadiusCenterFactor, FloatProvider yScale) {
        public static final Codec<Shape> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.carver.CanyonWorldCarver.Shape.class, "CODEC");
    }
}
