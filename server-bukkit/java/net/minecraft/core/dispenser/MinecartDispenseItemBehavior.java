// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.vehicle.minecart.AbstractMinecart;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.BaseRailBlock;
import net.minecraft.world.level.block.DispenserBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.world.phys.Vec3;
public class MinecartDispenseItemBehavior extends DefaultDispenseItemBehavior {
    private final DefaultDispenseItemBehavior fallback = new DefaultDispenseItemBehavior();
    private final EntityType<? extends AbstractMinecart> entityType;
    public MinecartDispenseItemBehavior(EntityType<? extends AbstractMinecart> entityType) {
        this.entityType = entityType;
    }
    public ItemStack execute(BlockSource source, ItemStack dispensed) {
        Direction facing = source.state().getValue(DispenserBlock.FACING);
        ServerLevel level = source.level();
        Vec3 center = source.center();
        double x = center.x() + facing.getStepX() * 1.125;
        double y = Math.floor(center.y()) + facing.getStepY();
        double z = center.z() + facing.getStepZ() * 1.125;
        BlockPos front = source.pos().relative(facing);
        BlockState ahead = level.getBlockState(front);
        double lift;
        if (ahead.is(BlockTags.RAILS)) {
            lift = shapeOf(ahead).isSlope() ? 0.6 : 0.1;
        } else {
            BlockState below = level.getBlockState(front.below());
            if (!ahead.isAir() || !below.is(BlockTags.RAILS)) {
                return fallback.dispense(source, dispensed);
            }
            lift = facing != Direction.DOWN && shapeOf(below).isSlope() ? -0.4 : -0.9;
        }
        AbstractMinecart cart = AbstractMinecart.createMinecart(level, x, y + lift, z, entityType, EntitySpawnReason.DISPENSER, dispensed, null);
        if (cart != null) {
            level.addFreshEntity(cart);
            dispensed.shrink(1);
        }
        return dispensed;
    }
    private static RailShape shapeOf(BlockState state) {
        return state.getBlock() instanceof BaseRailBlock rail ? state.getValue(rail.getShapeProperty()) : RailShape.NORTH_SOUTH;
    }
    protected void playSound(BlockSource source) {
        source.level().levelEvent(1000, source.pos(), 0);
    }
}
