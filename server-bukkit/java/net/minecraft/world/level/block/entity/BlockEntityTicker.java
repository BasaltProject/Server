// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
public interface BlockEntityTicker<T extends BlockEntity> {
    void tick(Level level, BlockPos pos, BlockState state, T entity);
    default BlockEntityTicker<T> andThen(BlockEntityTicker<? super T> after) {
        return (level, pos, state, entity) -> {
            tick(level, pos, state, entity);
            after.tick(level, pos, state, entity);
        };
    }
}
