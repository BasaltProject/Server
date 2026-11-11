// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.FluidTags;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.vehicle.boat.AbstractBoat;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.DispenserBlock;
import net.minecraft.world.phys.Vec3;
public class BoatDispenseItemBehavior extends DefaultDispenseItemBehavior {
    private final DefaultDispenseItemBehavior fallback = new DefaultDispenseItemBehavior();
    private final EntityType<? extends AbstractBoat> type;
    public BoatDispenseItemBehavior(EntityType<? extends AbstractBoat> type) {
        this.type = type;
    }
    public ItemStack execute(BlockSource source, ItemStack dispensed) {
        Direction facing = source.state().getValue(DispenserBlock.FACING);
        ServerLevel level = source.level();
        Vec3 center = source.center();
        double reach = 0.5625 + type.getWidth() / 2.0;
        double x = center.x() + facing.getStepX() * reach;
        double y = center.y() + (float) facing.getStepY() * 1.125F;
        double z = center.z() + facing.getStepZ() * reach;
        BlockPos front = source.pos().relative(facing);
        double lift;
        if (level.getFluidState(front).is(FluidTags.WATER)) {
            lift = 1.0;
        } else if (level.getBlockState(front).isAir() && level.getFluidState(front.below()).is(FluidTags.WATER)) {
            lift = 0.0;
        } else {
            return fallback.dispense(source, dispensed);
        }
        if (type.create(level, EntitySpawnReason.DISPENSER) instanceof AbstractBoat boat) {
            boat.setInitialPos(x, y + lift, z);
            EntityType.createDefaultStackConfig(level, dispensed, null).apply(boat);
            boat.setYRot(facing.toYRot());
            level.addFreshEntity(boat);
            dispensed.shrink(1);
        }
        return dispensed;
    }
    protected void playSound(BlockSource source) {
        source.level().levelEvent(1000, source.pos(), 0);
    }
}
