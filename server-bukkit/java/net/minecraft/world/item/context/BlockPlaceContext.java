package net.minecraft.world.item.context;

import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;
public class BlockPlaceContext extends UseOnContext {
    public BlockPlaceContext(Level level, Player player, InteractionHand interactionHand, ItemStack itemStack, BlockHitResult blockHitResult) {
        super((Player) null, (InteractionHand) null, (BlockHitResult) null);
    }
}
