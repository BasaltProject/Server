// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;
import net.minecraft.core.BlockPos;
import net.minecraft.core.NonNullList;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
public class Containers {
    public static void dropContents(Level level, BlockPos pos, Container container) {
        dropAll(level, pos.getX(), pos.getY(), pos.getZ(), container);
    }
    public static void dropContents(Level level, Entity entity, Container container) {
        dropAll(level, entity.getX(), entity.getY(), entity.getZ(), container);
    }
    private static void dropAll(Level level, double x, double y, double z, Container container) {
        for (int slot = 0; slot < container.getContainerSize(); slot++) {
            dropItemStack(level, x, y, z, container.getItem(slot));
        }
    }
    public static void dropContents(Level level, BlockPos pos, NonNullList<ItemStack> list) {
        list.forEach(stack -> dropItemStack(level, pos.getX(), pos.getY(), pos.getZ(), stack));
    }
    public static void dropItemStack(Level level, double x, double y, double z, ItemStack itemStack) {
        double width = EntityTypes.ITEM.getWidth();
        double room = 1.0 - width;
        RandomSource random = level.getRandom();
        double px = Math.floor(x) + random.nextDouble() * room + width / 2.0;
        double py = Math.floor(y) + random.nextDouble() * room;
        double pz = Math.floor(z) + random.nextDouble() * room + width / 2.0;
        while (!itemStack.isEmpty()) {
            ItemEntity drop = new ItemEntity(level, px, py, pz, itemStack.split(random.nextInt(21) + 10));
            drop.setDeltaMovement(random.triangle(0.0, 0.11485000171139836), random.triangle(0.2, 0.11485000171139836), random.triangle(0.0, 0.11485000171139836));
            level.addFreshEntity(drop);
        }
    }
    public static void updateNeighboursAfterDestroy(BlockState state, Level level, BlockPos pos) {
        level.updateNeighbourForOutputSignal(pos, state.getBlock());
    }
}
