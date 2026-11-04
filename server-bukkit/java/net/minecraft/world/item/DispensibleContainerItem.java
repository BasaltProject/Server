// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;

public interface DispensibleContainerItem {
    default void checkExtraContent(LivingEntity user, Level level, ItemStack itemStack, BlockPos pos) {
    }

    boolean emptyContents(LivingEntity user, Level level, BlockPos pos, BlockHitResult hitResult);
}
