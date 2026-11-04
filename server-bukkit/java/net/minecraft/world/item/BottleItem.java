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

import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
import net.minecraft.world.entity.AreaEffectCloud;
import net.minecraft.world.item.alchemy.PotionContents;
public class BottleItem extends Item {
    public BottleItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        ItemStack held = player.getItemInHand(hand);
        List<AreaEffectCloud> clouds = level.getEntitiesOfClass(AreaEffectCloud.class, player.getBoundingBox().inflate(2.0),
            cloud -> cloud.isAlive() && cloud.getOwner() instanceof EnderDragon);
        if (!clouds.isEmpty()) {
            AreaEffectCloud cloud = clouds.get(0);
            cloud.setRadius(cloud.getRadius() - 0.5F);
            level.playSound(null, player.getX(), player.getY(), player.getZ(), SoundEvents.BOTTLE_FILL_DRAGONBREATH, SoundSource.NEUTRAL, 1.0F, 1.0F);
            level.gameEvent(player, GameEvent.FLUID_PICKUP, player.position());
            return InteractionResult.SUCCESS.heldItemTransformedTo(turnBottleIntoItem(held, player, new ItemStack(Items.DRAGON_BREATH)));
        }
        BlockHitResult target = getPlayerPOVHitResult(level, player, ClipContext.Fluid.SOURCE_ONLY);
        if (target.getType() != HitResult.Type.BLOCK) {
            return InteractionResult.PASS;
        }
        BlockPos pos = target.getBlockPos();
        if (!level.mayInteract(player, pos) || !level.getFluidState(pos).is(net.minecraft.tags.FluidTags.WATER)) {
            return InteractionResult.PASS;
        }
        level.playSound(player, player.getX(), player.getY(), player.getZ(), SoundEvents.BOTTLE_FILL, SoundSource.NEUTRAL, 1.0F, 1.0F);
        level.gameEvent(player, GameEvent.FLUID_PICKUP, pos);
        return InteractionResult.SUCCESS.heldItemTransformedTo(turnBottleIntoItem(held, player,
            PotionContents.createItemStack(Items.POTION, net.minecraft.world.item.alchemy.Potions.WATER)));
    }

    protected ItemStack turnBottleIntoItem(ItemStack itemStack, Player player, ItemStack itemStackToTurnInto) {
        player.awardStat(net.minecraft.stats.Stats.ITEM_USED.get(this));
        return ItemUtils.createFilledResult(itemStack, player, itemStackToTurnInto);
    }
}
