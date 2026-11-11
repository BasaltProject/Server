// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.function.Supplier;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.particles.BlockParticleOption;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;

public class ParticleUtils {
    public static void spawnParticlesOnBlockFaces(Level level, BlockPos pos, ParticleOptions particle, IntProvider particlesPerFaceRange) {
        RandomSource random = level.getRandom();
        for (Direction face : Direction.values()) {
            spawnParticlesOnBlockFace(level, pos, particle, particlesPerFaceRange, face, () -> randomSpeed(random), 0.55D);
        }
    }

    public static void spawnParticlesOnBlockFace(Level level, BlockPos pos, ParticleOptions particle, IntProvider particlesPerFaceRange,
            Direction face, Supplier<Vec3> speedSupplier, double stepFactor) {
        int count = particlesPerFaceRange.sample(level.getRandom());
        for (int i = 0; i < count; i++) {
            spawnParticleOnFace(level, pos, face, particle, speedSupplier.get(), stepFactor);
        }
    }

    private static Vec3 randomSpeed(RandomSource random) {
        return new Vec3(Mth.nextDouble(random, -0.5D, 0.5D), Mth.nextDouble(random, -0.5D, 0.5D), Mth.nextDouble(random, -0.5D, 0.5D));
    }

    public static void spawnParticlesAlongAxis(Direction.Axis attachedAxis, Level level, BlockPos pos, double radius, ParticleOptions particle,
            UniformInt sparkCount) {
        Vec3 center = Vec3.atCenterOf(pos);
        boolean alongX = attachedAxis == Direction.Axis.X;
        boolean alongY = attachedAxis == Direction.Axis.Y;
        boolean alongZ = attachedAxis == Direction.Axis.Z;
        RandomSource random = level.getRandom();
        int count = sparkCount.sample(random);
        for (int i = 0; i < count; i++) {
            double x = center.x + Mth.nextDouble(random, -1.0D, 1.0D) * (alongX ? 0.5D : radius);
            double y = center.y + Mth.nextDouble(random, -1.0D, 1.0D) * (alongY ? 0.5D : radius);
            double z = center.z + Mth.nextDouble(random, -1.0D, 1.0D) * (alongZ ? 0.5D : radius);
            double vx = alongX ? Mth.nextDouble(random, -1.0D, 1.0D) : 0.0D;
            double vy = alongY ? Mth.nextDouble(random, -1.0D, 1.0D) : 0.0D;
            double vz = alongZ ? Mth.nextDouble(random, -1.0D, 1.0D) : 0.0D;
            level.addParticle(particle, x, y, z, vx, vy, vz);
        }
    }

    public static void spawnParticleOnFace(Level level, BlockPos pos, Direction face, ParticleOptions particle, Vec3 speed, double stepFactor) {
        Vec3 center = Vec3.atCenterOf(pos);
        int sx = face.getStepX();
        int sy = face.getStepY();
        int sz = face.getStepZ();
        RandomSource random = level.getRandom();
        double x = center.x + (sx == 0 ? Mth.nextDouble(random, -0.5D, 0.5D) : sx * stepFactor);
        double y = center.y + (sy == 0 ? Mth.nextDouble(random, -0.5D, 0.5D) : sy * stepFactor);
        double z = center.z + (sz == 0 ? Mth.nextDouble(random, -0.5D, 0.5D) : sz * stepFactor);
        level.addParticle(particle, x, y, z, sx == 0 ? speed.x() : 0.0D, sy == 0 ? speed.y() : 0.0D, sz == 0 ? speed.z() : 0.0D);
    }

    public static void spawnParticleBelow(Level level, BlockPos pos, RandomSource random, ParticleOptions particle) {
        level.addParticle(particle, pos.getX() + random.nextDouble(), pos.getY() - 0.05D, pos.getZ() + random.nextDouble(), 0.0D, 0.0D, 0.0D);
    }

    public static void spawnParticleInBlock(LevelAccessor level, BlockPos pos, int count, ParticleOptions particle) {
        BlockState state = level.getBlockState(pos);
        double height = state.isAir() ? 1.0D : state.getShape(level, pos).max(Direction.Axis.Y);
        spawnParticles(level, pos, count, 0.5D, height, true, particle);
    }

    public static void spawnParticles(LevelAccessor level, BlockPos pos, int count, double spreadWidth, double spreadHeight,
            boolean allowFloatingParticles, ParticleOptions particle) {
        RandomSource random = level.getRandom();
        for (int i = 0; i < count; i++) {
            double vx = random.nextGaussian() * 0.02D;
            double vy = random.nextGaussian() * 0.02D;
            double vz = random.nextGaussian() * 0.02D;
            double offset = 0.5D - spreadWidth;
            double x = pos.getX() + offset + random.nextDouble() * spreadWidth * 2.0D;
            double y = pos.getY() + random.nextDouble() * spreadHeight;
            double z = pos.getZ() + offset + random.nextDouble() * spreadWidth * 2.0D;
            if (allowFloatingParticles || !level.getBlockState(BlockPos.containing(x, y, z).below()).isAir()) {
                level.addParticle(particle, x, y, z, vx, vy, vz);
            }
        }
    }

    public static void spawnSmashAttackParticles(LevelAccessor level, BlockPos pos, int count) {
        Vec3 center = Vec3.atCenterOf(pos).add(0.0D, 0.5D, 0.0D);
        BlockParticleOption particle = new BlockParticleOption(ParticleTypes.DUST_PILLAR, level.getBlockState(pos));
        RandomSource random = level.getRandom();
        for (int i = 0; i < count / 3.0F; i++) {
            level.addParticle(particle, center.x + random.nextGaussian() / 2.0D, center.y, center.z + random.nextGaussian() / 2.0D,
                random.nextGaussian() * 0.2D, random.nextGaussian() * 0.2D, random.nextGaussian() * 0.2D);
        }
        for (int i = 0; i < count / 1.5F; i++) {
            level.addParticle(particle, center.x + 3.5D * Math.cos(i) + random.nextGaussian() / 2.0D, center.y,
                center.z + 3.5D * Math.sin(i) + random.nextGaussian() / 2.0D,
                random.nextGaussian() * 0.05D, random.nextGaussian() * 0.05D, random.nextGaussian() * 0.05D);
        }
    }
}
