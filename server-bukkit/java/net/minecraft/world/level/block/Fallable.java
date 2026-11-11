// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.item.FallingBlockEntity;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
public interface Fallable {
    default void onLand(Level level, BlockPos pos, BlockState state, BlockState replacedBlock, FallingBlockEntity entity) {
        return;
    }
    default void onBrokenAfterFall(Level level, BlockPos pos, FallingBlockEntity entity) {
        return;
    }
    default DamageSource getFallDamageSource(Entity entity) {
        return entity.damageSources().fallingBlock(entity);


    }
}
