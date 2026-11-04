// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;

public class CompassItem extends Item {
    private static final Component kLodestoneName = Component.translatable("item.minecraft.lodestone_compass");

    public CompassItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public boolean isFoil(ItemStack itemStack) {
        return itemStack.has(DataComponents.LODESTONE_TRACKER) || super.isFoil(itemStack);
    }

    @Override
    public void inventoryTick(ItemStack itemStack, ServerLevel level, Entity owner, EquipmentSlot slot) {
        LodestoneTracker tracker = itemStack.get(DataComponents.LODESTONE_TRACKER);
        if (tracker != null) {
            LodestoneTracker ticked = tracker.tick(level);
            if (ticked != tracker) {
                itemStack.set(DataComponents.LODESTONE_TRACKER, ticked);
            }
        }
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        BlockPos pos = context.getClickedPos();
        Level level = context.getLevel();
        if (!level.getBlockState(pos).is(Blocks.LODESTONE)) {
            return super.useOn(context);
        }
        level.playSound(null, pos, SoundEvents.LODESTONE_COMPASS_LOCK, SoundSource.PLAYERS, 1.0F, 1.0F);
        Player player = context.getPlayer();
        ItemStack stack = context.getItemInHand();
        LodestoneTracker target = new LodestoneTracker(Optional.of(GlobalPos.of(level.dimension(), pos)), true);
        if (!player.hasInfiniteMaterials() && stack.getCount() == 1) {
            stack.set(DataComponents.LODESTONE_TRACKER, target);
        } else {
            ItemStack locked = stack.transmuteCopy(Items.COMPASS, 1);
            stack.consume(1, player);
            locked.set(DataComponents.LODESTONE_TRACKER, target);
            if (!player.getInventory().add(locked)) {
                player.drop(locked, false, net.minecraft.util.Prediction.PREDICTED);
            }
        }
        return InteractionResult.SUCCESS;
    }

    @Override
    public Component getName(ItemStack itemStack) {
        return itemStack.has(DataComponents.LODESTONE_TRACKER) ? kLodestoneName : super.getName(itemStack);
    }
}
