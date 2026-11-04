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

import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.util.Util;
public class DebugStickItem extends Item {
    public DebugStickItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public boolean canDestroyBlock(ItemStack itemStack, BlockState state, Level level, BlockPos pos, LivingEntity user) {
        if (user instanceof ServerPlayer player) {
            handleInteraction(player, state, level, pos, false, itemStack);
        }
        return false;
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        if (context.getPlayer() instanceof ServerPlayer player) {
            BlockPos pos = context.getClickedPos();
            if (!handleInteraction(player, context.getLevel().getBlockState(pos), context.getLevel(), pos, true, context.getItemInHand())) {
                return InteractionResult.FAIL;
            }
        }
        return InteractionResult.SUCCESS;
    }

    private boolean handleInteraction(ServerPlayer player, BlockState state, LevelAccessor level, BlockPos pos, boolean cycle, ItemStack held) {
        if (!player.canUseGameMasterBlocks()) {
            return false;
        }
        Holder<Block> block = state.typeHolder();
        Collection<Property<?>> properties = block.value().getStateDefinition().getProperties();
        if (properties.isEmpty()) {
            player.sendOverlayMessage(Component.translatable(getDescriptionId() + ".empty", block.getRegisteredName()));
            return false;
        }
        DebugStickState selection = held.get(DataComponents.DEBUG_STICK_STATE);
        if (selection == null) {
            return false;
        }
        Property<?> property = selection.properties().get(block);
        if (cycle) {
            if (property == null) {
                property = properties.iterator().next();
            }
            BlockState cycled = cycleState(state, property, player.isSecondaryUseActive());
            level.setBlock(pos, cycled, 18);
            player.sendOverlayMessage(Component.translatable(getDescriptionId() + ".update", property.getName(), valueName(cycled, property)));
        } else {
            property = neighbour(properties, property, player.isSecondaryUseActive());
            held.set(DataComponents.DEBUG_STICK_STATE, selection.withProperty(block, property));
            player.sendOverlayMessage(Component.translatable(getDescriptionId() + ".select", property.getName(), valueName(state, property)));
        }
        return true;
    }

    private static <T extends Comparable<T>> BlockState cycleState(BlockState state, Property<T> property, boolean backward) {
        return state.setValue(property, neighbour(property.getPossibleValues(), state.getValue(property), backward));
    }

    private static <T> T neighbour(Iterable<T> values, T current, boolean backward) {
        return backward ? Util.findPreviousInIterable(values, current) : Util.findNextInIterable(values, current);
    }

    private static <T extends Comparable<T>> String valueName(BlockState state, Property<T> property) {
        return property.getName(state.getValue(property));
    }
}
