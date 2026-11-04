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
import net.minecraft.world.entity.projectile.*;
import net.minecraft.world.entity.projectile.throwableitemprojectile.*;
import net.minecraft.world.entity.projectile.hurtingprojectile.*;
import net.minecraft.world.entity.projectile.arrow.*;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.stats.Stats;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.tags.*;

import net.minecraft.world.entity.ItemSteerable;
public class FoodOnAStickItem<T extends Entity & ItemSteerable> extends Item {
    private final EntityType<T> canInteractWith;
    private final int consumeItemDamage;

    public FoodOnAStickItem(EntityType<T> canInteractWith, int consumeItemDamage, Item.Properties properties) {
        super(properties);
        this.canInteractWith = canInteractWith;
        this.consumeItemDamage = consumeItemDamage;
    }

    @SuppressWarnings("unchecked")
    private EntityType<T> steerable() {
        return canInteractWith != null ? canInteractWith : (EntityType<T>) ItemClasses.entityType(this);
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        if (level.isClientSide()) {
            return InteractionResult.PASS;
        }
        ItemStack held = player.getItemInHand(hand);
        Entity vehicle = player.getControlledVehicle();
        if (player.isPassenger() && vehicle instanceof ItemSteerable steerable && vehicle.is(steerable()) && steerable.boost()) {
            ItemStack result = held.hurtAndConvertOnBreak(consumeItemDamage, Items.FISHING_ROD, player, hand.asEquipmentSlot());
            return InteractionResult.SUCCESS_SERVER.heldItemTransformedTo(result);
        }
        player.awardStat(Stats.ITEM_USED.get(this));
        return InteractionResult.PASS;
    }
}
