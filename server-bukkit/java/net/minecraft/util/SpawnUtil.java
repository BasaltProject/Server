// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.LeavesBlock;
import net.minecraft.world.level.block.StainedGlassBlock;
import net.minecraft.world.level.block.StainedGlassPaneBlock;
import net.minecraft.world.level.block.state.BlockState;

public class SpawnUtil {
    public static <T extends Mob> Optional<T> trySpawnMob(EntityType<T> entityType, EntitySpawnReason spawnReason, ServerLevel level, BlockPos start,
            int spawnAttempts, int spawnRangeXZ, int spawnRangeY, Strategy strategy, boolean checkCollisions) {
        BlockPos.MutableBlockPos search = start.mutable();
        RandomSource random = level.getRandom();
        for (int attempt = 0; attempt < spawnAttempts; attempt++) {
            int dx = Mth.randomBetweenInclusive(random, -spawnRangeXZ, spawnRangeXZ);
            int dz = Mth.randomBetweenInclusive(random, -spawnRangeXZ, spawnRangeXZ);
            search.setWithOffset(start, dx, spawnRangeY, dz);
            if (!level.getWorldBorder().isWithinBounds(search) || !settle(level, spawnRangeY, search, strategy)) {
                continue;
            }
            if (checkCollisions && !level.noCollision(entityType.getSpawnAABB(search.getX() + 0.5D, search.getY(), search.getZ() + 0.5D))) {
                continue;
            }
            T mob = entityType.tryCast(entityType.create(level, null, search, spawnReason, false, false));
            if (mob == null) {
                continue;
            }
            if (mob.checkSpawnRules(level, spawnReason) && mob.checkSpawnObstruction(level)) {
                level.addFreshEntityWithPassengers(mob);
                mob.playAmbientSound();
                return Optional.of(mob);
            }
            mob.discard();
        }
        return Optional.empty();
    }

    private static boolean settle(ServerLevel level, int spawnRangeY, BlockPos.MutableBlockPos search, Strategy strategy) {
        BlockPos.MutableBlockPos above = new BlockPos.MutableBlockPos().set(search);
        BlockState aboveState = level.getBlockState(above);
        for (int y = spawnRangeY; y >= -spawnRangeY; y--) {
            search.move(Direction.DOWN);
            above.setWithOffset(search, Direction.UP);
            BlockState current = level.getBlockState(search);
            if (strategy.canSpawnOn(level, search, current, above, aboveState)) {
                search.move(Direction.UP);
                return true;
            }
            aboveState = current;
        }
        return false;
    }

    public interface Strategy {
        @Deprecated
        Strategy LEGACY_IRON_GOLEM = (level, pos, state, abovePos, aboveState) -> {
            if (state.is(Blocks.COBWEB) || state.is(Blocks.CACTUS) || state.is(Blocks.GLASS_PANE) || state.getBlock() instanceof StainedGlassPaneBlock
                    || state.getBlock() instanceof StainedGlassBlock || state.getBlock() instanceof LeavesBlock || state.is(Blocks.CONDUIT)
                    || state.is(Blocks.ICE) || state.is(Blocks.TNT) || state.is(Blocks.GLOWSTONE) || state.is(Blocks.BEACON)
                    || state.is(Blocks.SEA_LANTERN) || state.is(Blocks.FROSTED_ICE) || state.is(Blocks.TINTED_GLASS) || state.is(Blocks.GLASS)) {
                return false;
            }
            return (aboveState.isAir() || aboveState.liquid()) && (state.isSolid() || state.is(Blocks.POWDER_SNOW));
        };
        Strategy ON_TOP_OF_COLLIDER = (level, pos, state, abovePos, aboveState) ->
            aboveState.getCollisionShape(level, abovePos).isEmpty() && Block.isFaceFull(state.getCollisionShape(level, pos), Direction.UP);
        Strategy ON_TOP_OF_COLLIDER_NO_LEAVES = (level, pos, state, abovePos, aboveState) ->
            aboveState.getCollisionShape(level, abovePos).isEmpty() && !state.is(BlockTags.LEAVES)
                && Block.isFaceFull(state.getCollisionShape(level, pos), Direction.UP);

        boolean canSpawnOn(ServerLevel level, BlockPos pos, BlockState blockState, BlockPos abovePos, BlockState aboveState);
    }
}
