// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.Fluids;

public interface SimpleWaterloggedBlock extends BucketPickup, LiquidBlockContainer {
    default boolean canPlaceLiquid(LivingEntity user, BlockGetter level, BlockPos pos, BlockState state, Fluid type) {
        return type == Fluids.WATER && !state.getValue(BlockStateProperties.WATERLOGGED);
    }

    default boolean placeLiquid(LevelAccessor level, BlockPos pos, BlockState state, FluidState fluidState) {
        if (state.getValue(BlockStateProperties.WATERLOGGED) || fluidState.getType() != Fluids.WATER) {
            return false;
        }
        level.setBlock(pos, state.setValue(BlockStateProperties.WATERLOGGED, true), 3);
        return true;
    }

    default ItemStack pickupBlock(LivingEntity user, LevelAccessor level, BlockPos pos, BlockState state) {
        if (!state.getValue(BlockStateProperties.WATERLOGGED)) {
            return ItemStack.EMPTY;
        }
        level.setBlock(pos, state.setValue(BlockStateProperties.WATERLOGGED, false), 3);
        return new ItemStack(Items.WATER_BUCKET);
    }

    default Optional<SoundEvent> getPickupSound() {
        return Optional.ofNullable(SoundEvents.BUCKET_FILL);
    }
}
