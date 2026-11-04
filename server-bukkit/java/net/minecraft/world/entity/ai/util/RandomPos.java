// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.util;

import java.util.function.Predicate;
import java.util.function.Supplier;
import java.util.function.ToDoubleFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.phys.Vec3;

public class RandomPos {
    private static final int kAttempts = 10;

    public static BlockPos generateRandomDirection(RandomSource random, int horizontalDist, int verticalDist) {
        int x = random.nextInt(2 * horizontalDist + 1) - horizontalDist;
        int y = random.nextInt(2 * verticalDist + 1) - verticalDist;
        int z = random.nextInt(2 * horizontalDist + 1) - horizontalDist;
        return new BlockPos(x, y, z);
    }

    public static BlockPos generateRandomDirectionWithinRadians(RandomSource random, double minHorizontalDist, double maxHorizontalDist,
        int verticalDist, int flyingHeight, double xDir, double zDir, double maxXzRadiansFromDir) {
        double center = Mth.atan2(zDir, xDir) - (float) Math.PI / 2.0F;
        double radians = center + (2.0F * random.nextFloat() - 1.0F) * maxXzRadiansFromDir;
        double dist = Mth.lerp(Math.sqrt(random.nextDouble()), minHorizontalDist, maxHorizontalDist) * Mth.SQRT_OF_TWO;
        double x = -dist * Math.sin(radians);
        double z = dist * Math.cos(radians);
        if (Math.abs(x) > maxHorizontalDist || Math.abs(z) > maxHorizontalDist) {
            return null;
        }
        int y = random.nextInt(2 * verticalDist + 1) - verticalDist + flyingHeight;
        return BlockPos.containing(x, y, z);
    }

    public static BlockPos moveUpOutOfSolid(BlockPos pos, int maxY, Predicate<BlockPos> solidityTester) {
        if (!solidityTester.test(pos)) {
            return pos;
        }
        BlockPos.MutableBlockPos cursor = pos.mutable().move(Direction.UP);
        while (cursor.getY() <= maxY && solidityTester.test(cursor)) {
            cursor.move(Direction.UP);
        }
        return cursor.immutable();
    }

    public static BlockPos moveUpToAboveSolid(BlockPos pos, int aboveSolidAmount, int maxY, Predicate<BlockPos> solidityTester) {
        if (aboveSolidAmount < 0) {
            throw new IllegalArgumentException("aboveSolidAmount was " + aboveSolidAmount + ", expected >= 0");
        }
        if (!solidityTester.test(pos)) {
            return pos;
        }
        BlockPos.MutableBlockPos cursor = pos.mutable().move(Direction.UP);
        while (cursor.getY() <= maxY && solidityTester.test(cursor)) {
            cursor.move(Direction.UP);
        }
        int firstFreeY = cursor.getY();
        while (cursor.getY() <= maxY && cursor.getY() - firstFreeY < aboveSolidAmount) {
            cursor.move(Direction.UP);
            if (solidityTester.test(cursor)) {
                cursor.move(Direction.DOWN);
                break;
            }
        }
        return cursor.immutable();
    }

    public static Vec3 generateRandomPos(PathfinderMob mob, Supplier<BlockPos> posSupplier) {
        return generateRandomPos(posSupplier, mob::getWalkTargetValue);
    }

    public static Vec3 generateRandomPos(Supplier<BlockPos> posSupplier, ToDoubleFunction<BlockPos> positionWeightFunction) {
        double bestWeight = Double.NEGATIVE_INFINITY;
        BlockPos best = null;
        for (int i = 0; i < kAttempts; i++) {
            BlockPos pos = posSupplier.get();
            if (pos != null) {
                double weight = positionWeightFunction.applyAsDouble(pos);
                if (weight > bestWeight) {
                    bestWeight = weight;
                    best = pos;
                }
            }
        }
        return best != null ? Vec3.atBottomCenterOf(best) : null;
    }

    public static BlockPos generateRandomPosTowardDirection(PathfinderMob mob, double xzDist, RandomSource random, BlockPos direction) {
        double x = direction.getX();
        double z = direction.getZ();
        if (mob.hasHome() && xzDist > 1.0) {
            BlockPos center = mob.getHomePosition();
            x += (mob.getX() > center.getX() ? -1.0 : 1.0) * random.nextDouble() * xzDist / 2.0;
            z += (mob.getZ() > center.getZ() ? -1.0 : 1.0) * random.nextDouble() * xzDist / 2.0;
        }
        return BlockPos.containing(x + mob.getX(), direction.getY() + mob.getY(), z + mob.getZ());
    }
}
