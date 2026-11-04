// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.IntegerProperty;

public class FarmlandBlock extends Block {
    public static final IntegerProperty MOISTURE = BlockStateProperties.MOISTURE;
    public static final int MAX_MOISTURE = 7;

    protected FarmlandBlock(int index) {
        super(index);
    }

    public void turnToBaseBlock(Entity sourceEntity, BlockState state, Level level, BlockPos pos) {
        level.setBlock(pos, Blocks.DIRT.defaultBlockState(), 3);
    }

    @Override
    public void fallOn(Level level, BlockState state, BlockPos pos, Entity entity, double fallDistance) {
        if (fallDistance > 0.75 && level instanceof net.minecraft.server.level.ServerLevel) {
            turnToBaseBlock(entity, state, level, pos);
        }
        super.fallOn(level, state, pos, entity, fallDistance);
    }
}
