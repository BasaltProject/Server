// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.Position;

import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;

import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.core.Direction;
import net.minecraft.core.Position;
import net.minecraft.core.dispenser.BlockSource;
import net.minecraft.world.phys.Vec3;
public class DispenserBlock extends BaseEntityBlock {
    public static final BooleanProperty TRIGGERED = BlockStateProperties.TRIGGERED;

    public static final EnumProperty<Direction> FACING = DirectionalBlock.FACING;

    public static Position getDispensePosition(net.minecraft.core.dispenser.BlockSource source, double offset, net.minecraft.world.phys.Vec3 shift) {
        Direction face = source.state().getValue(FACING);
        net.minecraft.world.phys.Vec3 center = source.center();
        return center.add(offset * face.getStepX() + shift.x, offset * face.getStepY() + shift.y, offset * face.getStepZ() + shift.z);
    }

    protected DispenserBlock(int index) {
        super(index);
    }

    @Override
    protected boolean hasInventory() {
        return true;
    }

    protected void dispenseFrom(net.minecraft.server.level.ServerLevel level, net.minecraft.world.level.block.state.BlockState state, net.minecraft.core.BlockPos pos) {
        if (level.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getState() instanceof org.bukkit.block.Dispenser dispenser) {
            dispenser.dispense();
        } else if (level.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getState() instanceof org.bukkit.block.Dropper dropper) {
            dropper.drop();
        }
    }

    public static Position getDispensePosition(BlockSource source) {
        return getDispensePosition(source, 0.7, Vec3.ZERO);
    }
}
