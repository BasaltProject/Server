// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.Direction;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.boss.wither.WitherBoss;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;
import net.minecraft.world.level.block.state.pattern.BlockPattern;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.entity.SkullBlockEntity;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class WitherSkullBlock extends SkullBlock {
    private static BlockPattern witherFull;
    private static BlockPattern witherBase;
    @SuppressWarnings("unchecked")
    private static BlockPattern witherPattern(boolean withSkulls) {
        Predicate<BlockInWorld> anything = block -> true;
        Predicate<BlockInWorld> base = block -> block.getState().is(BlockTags.WITHER_SUMMON_BASE_BLOCKS);
        Predicate<BlockInWorld> air = block -> block.getState().isAir();
        Predicate<BlockInWorld> skull = withSkulls
            ? BlockInWorld.hasState(state -> state.is(Blocks.WITHER_SKELETON_SKULL) || state.is(Blocks.WITHER_SKELETON_WALL_SKULL))
            : anything;
        Predicate<BlockInWorld>[][][] grid = new Predicate[1][3][3];
        grid[0][0] = new Predicate[] {skull, skull, skull};
        grid[0][1] = new Predicate[] {base, base, base};
        grid[0][2] = new Predicate[] {air, base, air};
        return new BlockPattern(grid);
    }
    private static BlockPattern fullPattern() {
        if (witherFull == null) {
            witherFull = witherPattern(true);
        }
        return witherFull;
    }
    private static BlockPattern basePattern() {
        if (witherBase == null) {
            witherBase = witherPattern(false);
        }
        return witherBase;
    }
    protected WitherSkullBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public void setPlacedBy(Level level, BlockPos pos, BlockState state, LivingEntity by, ItemStack itemStack) {
        super.setPlacedBy(level, pos, state, by, itemStack);
    }
    public static void checkSpawn(Level level, BlockPos pos) {
        if (level.getBlockEntity(pos) instanceof SkullBlockEntity skull) {
            checkSpawn(level, pos, skull);
        }
    }
    public static void checkSpawn(Level level, BlockPos pos, SkullBlockEntity placedSkull) {
        if (level.isClientSide()) {
            return;
        }
        BlockState placed = placedSkull.getBlockState();
        boolean witherSkull = placed.is(Blocks.WITHER_SKELETON_SKULL) || placed.is(Blocks.WITHER_SKELETON_WALL_SKULL);
        if (!witherSkull || pos.getY() < level.getMinY() || level.getDifficulty() == Difficulty.PEACEFUL) {
            return;
        }
        BlockPattern.BlockPatternMatch found = fullPattern().find(level, pos);
        if (found == null) {
            return;
        }
        WitherBoss boss = (WitherBoss) EntityTypes.WITHER.create(level, EntitySpawnReason.TRIGGERED);
        if (boss == null) {
            return;
        }
        for (int x = 0; x < found.getWidth(); x++) {
            for (int y = 0; y < found.getHeight(); y++) {
                BlockInWorld cell = found.getBlock(x, y, 0);
                level.setBlock(cell.getPos(), Blocks.AIR.defaultBlockState(), 2);
                level.levelEvent(2001, cell.getPos(), Block.getId(cell.getState()));
            }
        }
        BlockPos at = found.getBlock(1, 2, 0).getPos();
        float yaw = found.getForwards().getAxis() == Direction.Axis.X ? 0.0F : 90.0F;
        boss.snapTo(at.getX() + 0.5, at.getY() + 0.55, at.getZ() + 0.5, yaw, 0.0F);
        boss.yBodyRot = yaw;
        boss.makeInvulnerable();
        level.addFreshEntity(boss);
        for (int x = 0; x < found.getWidth(); x++) {
            for (int y = 0; y < found.getHeight(); y++) {
                level.updateNeighborsAt(found.getBlock(x, y, 0).getPos(), Blocks.AIR);
            }
        }
    }
    public static boolean canSpawnMob(Level level, BlockPos pos, ItemStack itemStack) {
        return itemStack.is(Items.WITHER_SKELETON_SKULL) && pos.getY() >= level.getMinY() + 2
            && level.getDifficulty() != Difficulty.PEACEFUL && !level.isClientSide() && basePattern().find(level, pos) != null;


    }
}
