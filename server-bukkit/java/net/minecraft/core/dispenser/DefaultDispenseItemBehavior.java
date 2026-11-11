// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import net.minecraft.core.Direction;
import net.minecraft.core.Position;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.DispenserBlock;
public class DefaultDispenseItemBehavior implements DispenseItemBehavior {
    private static final int kAccuracy = 6;
    public final ItemStack dispense(BlockSource source, ItemStack dispensed) {
        ItemStack out = execute(source, dispensed);
        playSound(source);
        playAnimation(source, source.state().getValue(DispenserBlock.FACING));
        return out;
    }
    protected ItemStack execute(BlockSource source, ItemStack dispensed) {
        Direction facing = source.state().getValue(DispenserBlock.FACING);
        Position at = DispenserBlock.getDispensePosition(source);
        spawnItem(source.level(), dispensed.split(1), kAccuracy, facing, at);
        return dispensed;
    }
    public static void spawnItem(Level level, ItemStack itemStack, int accuracy, Direction direction, Position position) {
        double y = position.y() - (direction.getAxis() == Direction.Axis.Y ? 0.125 : 0.15625);
        ItemEntity item = new ItemEntity(level, position.x(), y, position.z(), itemStack);
        RandomSource random = level.getRandom();
        double speed = random.nextDouble() * 0.1 + 0.2;
        double spread = 0.0172275 * accuracy;
        item.setDeltaMovement(random.triangle(direction.getStepX() * speed, spread), random.triangle(0.2, spread), random.triangle(direction.getStepZ() * speed, spread));
        level.addFreshEntity(item);
    }
    protected void playSound(BlockSource source) {
        source.level().levelEvent(1000, source.pos(), 0);
    }
    protected void playAnimation(BlockSource source, Direction direction) {
        source.level().levelEvent(2000, source.pos(), direction.get3DDataValue());
    }
    private static ItemStack insertInto(BlockSource source, ItemStack stack) {
        var box = source.blockEntity();
        int limit = box.getMaxStackSize(stack);
        for (int i = 0; i < box.getContainerSize() && !stack.isEmpty(); i++) {
            ItemStack slot = box.getItem(i);
            if (!slot.isEmpty() && !ItemStack.isSameItemSameComponents(stack, slot)) {
                continue;
            }
            int moved = Math.min(stack.getCount(), limit - slot.getCount());
            if (moved <= 0) {
                continue;
            }
            if (slot.isEmpty()) {
                box.setItem(i, stack.split(moved));
            } else {
                stack.shrink(moved);
                slot.grow(moved);
            }
        }
        return stack;
    }
    protected ItemStack consumeWithRemainder(BlockSource source, ItemStack dispensed, ItemStack remainder) {
        dispensed.shrink(1);
        if (dispensed.isEmpty()) {
            return remainder;
        }
        ItemStack left = insertInto(source, remainder);
        if (!left.isEmpty()) {
            Direction facing = source.state().getValue(DispenserBlock.FACING);
            spawnItem(source.level(), left, kAccuracy, facing, DispenserBlock.getDispensePosition(source));
            source.level().levelEvent(1000, source.pos(), 0);
            source.level().levelEvent(2000, source.pos(), facing.get3DDataValue());
        }
        return dispensed;
    }
}
