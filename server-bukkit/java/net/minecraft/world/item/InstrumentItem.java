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

public class InstrumentItem extends Item {
    public InstrumentItem(Item.Properties properties) {
        super(properties);
    }

    public static ItemStack create(Item item, Holder<Instrument> instrument) {
        ItemStack stack = new ItemStack(item);
        stack.set(DataComponents.INSTRUMENT, new InstrumentComponent(instrument));
        return stack;
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        ItemStack held = player.getItemInHand(hand);
        Optional<Holder<Instrument>> holder = getInstrument(held);
        if (holder.isEmpty()) {
            return InteractionResult.FAIL;
        }
        Instrument instrument = holder.get().value();
        player.startUsingItem(hand);
        play(level, player, instrument);
        if (instrument.durabilityDamage() > 0) {
            held.hurtAndBreak(instrument.durabilityDamage(), player, hand.asEquipmentSlot());
        }
        if (instrument.useDuration() > 0.0F) {
            player.getCooldowns().addCooldown(held, Mth.floor(instrument.useDuration() * 20.0F));
        }
        player.awardStat(Stats.ITEM_USED.get(this));
        return InteractionResult.CONSUME;
    }

    @Override
    public int getUseDuration(ItemStack itemStack, LivingEntity user) {
        return getInstrument(itemStack).map(holder -> Mth.floor(holder.value().useDuration() * 20.0F)).orElse(0);
    }

    private static Optional<Holder<Instrument>> getInstrument(ItemStack itemStack) {
        InstrumentComponent component = itemStack.get(DataComponents.INSTRUMENT);
        return component == null ? Optional.empty() : Optional.of(component.instrument());
    }

    @Override
    public ItemUseAnimation getUseAnimation(ItemStack itemStack) {
        return ItemUseAnimation.TOOT_HORN;
    }

    private static void play(Level level, Player player, Instrument instrument) {
        SoundEvent sound = instrument.soundEvent().value();
        level.playSound(player, player, sound, SoundSource.RECORDS, instrument.range() / 16.0F, 1.0F);
        level.gameEvent(GameEvent.INSTRUMENT_PLAY, player.position(), GameEvent.Context.of((Entity) player));
    }
}
