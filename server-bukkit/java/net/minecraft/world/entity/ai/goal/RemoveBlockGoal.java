// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.phys.Vec3;

public class RemoveBlockGoal extends MoveToBlockGoal {
    private final Block blockToRemove;
    private final Mob removerMob;
    private int ticksSinceReachedGoal;

    public RemoveBlockGoal(Block blockToRemove, PathfinderMob mob, double speedModifier, int verticalSearchRange) {
        super(mob, speedModifier, 24, verticalSearchRange);
        this.blockToRemove = blockToRemove;
        this.removerMob = mob;
    }

    private org.bukkit.World world() {
        return ((org.bukkit.entity.Entity) removerMob.getBukkitEntity()).getWorld();
    }

    @Override
    public boolean canUse() {
        if (!Boolean.TRUE.equals(world().getGameRuleValue(org.bukkit.GameRule.MOB_GRIEFING))) {
            return false;
        }
        if (nextStartTick > 0) {
            nextStartTick--;
            return false;
        }
        if (findNearestBlock()) {
            nextStartTick = reducedTickDelay(20);
            return true;
        }
        nextStartTick = nextStartTick(mob);
        return false;
    }

    @Override
    public void stop() {
        super.stop();
        removerMob.fallDistance = 1.0;
    }

    @Override
    public void start() {
        super.start();
        ticksSinceReachedGoal = 0;
    }

    public void playDestroyProgressSound(LevelAccessor level, BlockPos pos) {
    }

    public void playBreakSound(Level level, BlockPos pos) {
    }

    @Override
    public void tick() {
        super.tick();
        Level level = removerMob.level();
        BlockPos eatPos = posWithBlock(removerMob.blockPosition(), level);
        RandomSource random = removerMob.getRandom();
        if (!isReachedTarget() || eatPos == null) {
            return;
        }
        if (ticksSinceReachedGoal > 0) {
            Vec3 motion = removerMob.getDeltaMovement();
            removerMob.setDeltaMovement(motion.x, 0.3, motion.z);
            world().spawnParticle(org.bukkit.Particle.ITEM, eatPos.getX() + 0.5, eatPos.getY() + 0.7, eatPos.getZ() + 0.5, 3,
                (random.nextFloat() - 0.5) * 0.08, (random.nextFloat() - 0.5) * 0.08, (random.nextFloat() - 0.5) * 0.08, 0.15,
                new org.bukkit.inventory.ItemStack(org.bukkit.Material.EGG));
        }
        if (ticksSinceReachedGoal % 2 == 0) {
            Vec3 motion = removerMob.getDeltaMovement();
            removerMob.setDeltaMovement(motion.x, -0.3, motion.z);
            if (ticksSinceReachedGoal % 6 == 0) {
                playDestroyProgressSound(level, blockPos);
            }
        }
        if (ticksSinceReachedGoal > 60) {
            level.removeBlock(eatPos, false);
            for (int i = 0; i < 20; i++) {
                world().spawnParticle(org.bukkit.Particle.POOF, eatPos.getX() + 0.5, eatPos.getY(), eatPos.getZ() + 0.5, 1,
                    random.nextGaussian() * 0.02, random.nextGaussian() * 0.02, random.nextGaussian() * 0.02, 0.15);
            }
            playBreakSound(level, eatPos);
        }
        ticksSinceReachedGoal++;
    }

    private BlockPos posWithBlock(BlockPos pos, BlockGetter level) {
        if (level.getBlockState(pos).is(blockToRemove)) {
            return pos;
        }
        BlockPos[] neighbours = {pos.below(), pos.west(), pos.east(), pos.north(), pos.south(), pos.below().below()};
        for (BlockPos neighbour : neighbours) {
            if (level.getBlockState(neighbour).is(blockToRemove)) {
                return neighbour;
            }
        }
        return null;
    }

    @Override
    protected boolean isValidTarget(LevelReader level, BlockPos pos) {
        return level.getBlockState(pos).is(blockToRemove) && level.getBlockState(pos.above()).isAir()
            && level.getBlockState(pos.above(2)).isAir();
    }
}
