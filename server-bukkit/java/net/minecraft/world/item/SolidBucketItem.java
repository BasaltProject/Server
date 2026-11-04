// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import net.minecraft.core.BlockPos;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.context.UseOnContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.BlockHitResult;

public class SolidBucketItem extends BlockItem implements DispensibleContainerItem {
    private final SoundEvent placeSound;

    public SolidBucketItem(Block content, SoundEvent placeSound, Item.Properties properties) {
        super(content, properties);
        this.placeSound = placeSound;
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        InteractionResult result = super.useOn(context);
        Player player = context.getPlayer();
        if (result.consumesAction() && player != null) {
            player.setItemInHand(context.getHand(), BucketItem.getEmptySuccessItem(context.getItemInHand(), player));
        }
        return result;
    }

    @Override
    protected SoundEvent getPlaceSound(BlockState blockState) {
        return placeSound;
    }

    @Override
    public boolean emptyContents(LivingEntity user, Level level, BlockPos pos, BlockHitResult hitResult) {
        if (!level.isInWorldBounds(pos) || !level.isEmptyBlock(pos)) {
            return false;
        }
        if (!level.isClientSide()) {
            level.setBlockAndUpdate(pos, getBlock().defaultBlockState());
        }
        level.gameEvent(user, GameEvent.FLUID_PLACE, pos);
        level.playSound(user, pos, placeSound, SoundSource.BLOCKS, 1.0F, 1.0F);
        return true;
    }
}
