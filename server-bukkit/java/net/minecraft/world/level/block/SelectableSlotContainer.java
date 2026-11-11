// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import java.util.OptionalInt;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.BlockHitResult;
public interface SelectableSlotContainer {
    int getRows();
    int getColumns();
    default OptionalInt getHitSlot(BlockHitResult hitResult, Direction blockFacing) {
        if (blockFacing != hitResult.getDirection() || blockFacing.getAxis() == Direction.Axis.Y) {
            return OptionalInt.empty();
        }
        BlockPos hitPos = hitResult.getBlockPos().relative(blockFacing);
        double rx = hitResult.getLocation().x - hitPos.getX();
        double ry = hitResult.getLocation().y - hitPos.getY();
        double rz = hitResult.getLocation().z - hitPos.getZ();
        float across = switch (blockFacing) {
            case NORTH -> (float) (1.0 - rx);
            case SOUTH -> (float) rx;
            case WEST -> (float) rz;
            default -> (float) (1.0 - rz);
        };
        int row = Math.clamp((int) Math.floor((1.0F - (float) ry) * 16.0F / (16.0F / getRows())), 0, getRows() - 1);
        int column = Math.clamp((int) Math.floor(across * 16.0F / (16.0F / getColumns())), 0, getColumns() - 1);
        return OptionalInt.of(column + row * getColumns());
    }
}
