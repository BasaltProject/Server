// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.BooleanProperty;

import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.entity.EnderChestBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;

public class CampfireBlock extends BaseEntityBlock implements SimpleWaterloggedBlock {
    public static final BooleanProperty LIT = BlockStateProperties.LIT;
    public static final BooleanProperty WATERLOGGED = BlockStateProperties.WATERLOGGED;

    protected CampfireBlock(int index) {
        super(index);
    }

    public static boolean canLight(BlockState state) {
        return state.is(net.minecraft.tags.BlockTags.CAMPFIRES) && state.hasProperty(BlockStateProperties.WATERLOGGED)
            && !state.getValue(BlockStateProperties.WATERLOGGED) && state.hasProperty(BlockStateProperties.LIT)
            && !state.getValue(BlockStateProperties.LIT);
    }

    public static boolean isLitCampfire(BlockState state) {
        return state.hasProperty(BlockStateProperties.LIT) && state.is(net.minecraft.tags.BlockTags.CAMPFIRES) && state.getValue(BlockStateProperties.LIT);
    }

    public static boolean isSmokeyPos(net.minecraft.world.level.Level level, net.minecraft.core.BlockPos pos) {
        net.minecraft.world.phys.AABB post = new net.minecraft.world.phys.AABB(0.375, 0.0, 0.375, 0.625, 1.0, 0.625);
        for (int i = 1; i <= 5; i++) {
            net.minecraft.core.BlockPos checked = pos.below(i);
            BlockState state = level.getBlockState(checked);
            if (isLitCampfire(state)) {
                return true;
            }
            boolean blocked = state.getCollisionShape(level, pos, net.minecraft.world.phys.shapes.CollisionContext.empty()).toAabbs()
                .stream().anyMatch(box -> box.intersects(post));
            if (blocked) {
                return isLitCampfire(level.getBlockState(checked.below()));
            }
        }
        return false;
    }
}
