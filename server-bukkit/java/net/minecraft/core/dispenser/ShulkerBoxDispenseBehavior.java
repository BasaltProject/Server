// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.item.BlockItem;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.DirectionalPlaceContext;
import net.minecraft.world.level.block.DispenserBlock;
import org.slf4j.Logger;
public class ShulkerBoxDispenseBehavior extends OptionalDispenseItemBehavior {
    private static final Logger LOGGER = org.slf4j.LoggerFactory.getLogger(ShulkerBoxDispenseBehavior.class);
    protected ItemStack execute(BlockSource source, ItemStack dispensed) {
        setSuccess(false);
        if (dispensed.getItem() instanceof BlockItem blockItem) {
            Direction facing = source.state().getValue(DispenserBlock.FACING);
            BlockPos at = source.pos().relative(facing);
            Direction face = source.level().isEmptyBlock(at.below()) ? facing : Direction.UP;
            try {
                setSuccess(blockItem.place(new DirectionalPlaceContext(source.level(), at, facing, dispensed, face)).consumesAction());
            } catch (Exception e) {
                LOGGER.error("Error trying to place shulker box at {}", at, e);
            }
        }
        return dispensed;
    }
}
