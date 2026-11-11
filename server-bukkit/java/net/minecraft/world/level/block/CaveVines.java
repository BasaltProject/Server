// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import java.util.function.ToIntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.phys.shapes.VoxelShape;
public interface CaveVines {
    VoxelShape SHAPE = Block.column(14.0, 0.0, 16.0);
    BooleanProperty BERRIES = BlockStateProperties.BERRIES;
    static InteractionResult use(Entity sourceEntity, BlockState state, Level level, BlockPos pos) {
        if (!state.getValue(BERRIES)) {
            return InteractionResult.PASS;
        }
        if (level instanceof ServerLevel serverLevel) {
            Block.dropFromBlockInteractLootTable(serverLevel, net.minecraft.world.level.storage.loot.BuiltInLootTables.HARVEST_CAVE_VINE, pos, state,
                level.getBlockEntity(pos), null, sourceEntity, (lvl, stack) -> Block.popResource(lvl, pos, stack));
            float pitch = net.minecraft.util.Mth.randomBetween(serverLevel.getRandom(), 0.8F, 1.2F);
            serverLevel.playSound(null, pos, net.minecraft.sounds.SoundEvents.CAVE_VINES_PICK_BERRIES, net.minecraft.sounds.SoundSource.BLOCKS, 1.0F, pitch);
            BlockState newState = state.setValue(BERRIES, false);
            serverLevel.setBlock(pos, newState, 2);
            serverLevel.gameEvent(net.minecraft.world.level.gameevent.GameEvent.BLOCK_CHANGE, pos, net.minecraft.world.level.gameevent.GameEvent.Context.of(sourceEntity, newState));
        }
        return InteractionResult.SUCCESS;
    }
    static boolean hasGlowBerries(BlockState state) {
        return state.hasProperty(BERRIES) && state.getValue(BERRIES);
    }
    static ToIntFunction<BlockState> emission(int lightEmission) {
        return state -> state.getValue(BlockStateProperties.BERRIES) ? lightEmission : 0;
    }
}
