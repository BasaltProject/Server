// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.material;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;
public class EmptyFluid extends Fluid {
    public Item getBucket() {
        return Items.AIR;
    }
    public boolean canBeReplacedWith(FluidState state, BlockGetter level, BlockPos pos, Fluid other, Direction direction) {
        return true;
    }
    public Vec3 getFlow(BlockGetter level, BlockPos pos, FluidState fluidState) {
        return Vec3.ZERO;
    }
    public int getTickDelay(LevelReader level) {
        return 0;
    }
    protected boolean isEmpty() {
        return true;
    }
    protected float getExplosionResistance() {
        return 0.0F;
    }
    public float getHeight(FluidState fluidState, BlockGetter level, BlockPos pos) {
        return 0.0F;
    }
    public float getOwnHeight(FluidState fluidState) {
        return 0.0F;
    }
    protected BlockState createLegacyBlock(FluidState fluidState) {
        return Blocks.AIR.defaultBlockState();
    }
    public boolean isSource(FluidState fluidState) {
        return false;
    }
    public int getAmount(FluidState fluidState) {
        return 0;
    }
    public VoxelShape getShape(FluidState state, BlockGetter level, BlockPos pos) {
        return Shapes.empty();
    }
}
