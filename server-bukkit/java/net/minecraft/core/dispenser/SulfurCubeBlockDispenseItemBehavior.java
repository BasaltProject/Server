// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.monster.cubemob.SulfurCube;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.DispenserBlock;
import net.minecraft.world.phys.AABB;
public class SulfurCubeBlockDispenseItemBehavior extends DefaultDispenseItemBehavior {
    public static final SulfurCubeBlockDispenseItemBehavior INSTANCE = new SulfurCubeBlockDispenseItemBehavior();
    protected ItemStack execute(BlockSource source, ItemStack dispensed) {
        BlockPos front = source.pos().relative(source.state().getValue(DispenserBlock.FACING));
        return dispenseBlock(source.level(), front, dispensed) ? dispensed : super.execute(source, dispensed);
    }
    public static boolean dispenseBlock(ServerLevel level, BlockPos pos, ItemStack dispensed) {
        for (SulfurCube cube : level.getEntitiesOfClass(SulfurCube.class, new AABB(pos))) {
            if (cube.equipItem(dispensed)) {
                dispensed.shrink(1);
                return true;
            }
        }
        return false;
    }
}
