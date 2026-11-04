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

import net.minecraft.world.entity.decoration.*;
import net.minecraft.world.entity.decoration.painting.Painting;
import net.minecraft.world.entity.decoration.painting.PaintingVariant;
public class HangingEntityItem extends Item {
    private static final Component kRandomVariant = Component.translatable("painting.random").withStyle(net.minecraft.ChatFormatting.GRAY);
    private final EntityType<? extends HangingEntity> type;

    public HangingEntityItem(EntityType<? extends HangingEntity> type, Item.Properties properties) {
        super(properties);
        this.type = type;
    }

    @SuppressWarnings("unchecked")
    private EntityType<? extends HangingEntity> hangingType() {
        return type != null ? type : (EntityType<? extends HangingEntity>) ItemClasses.entityType(this);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        Direction face = context.getClickedFace();
        BlockPos pos = context.getClickedPos().relative(face);
        Player player = context.getPlayer();
        ItemStack stack = context.getItemInHand();
        if (player != null && !mayPlace(player, face, stack, pos)) {
            return InteractionResult.FAIL;
        }
        Level level = context.getLevel();
        EntityType<? extends HangingEntity> kind = hangingType();
        HangingEntity entity;
        if (kind == EntityTypes.PAINTING) {
            Optional<Painting> painting = Painting.create(level, pos, face);
            if (painting.isEmpty()) {
                return InteractionResult.CONSUME;
            }
            entity = painting.get();
        } else if (kind == EntityTypes.ITEM_FRAME) {
            entity = new ItemFrame(level, pos, face);
        } else if (kind == EntityTypes.GLOW_ITEM_FRAME) {
            entity = new GlowItemFrame(level, pos, face);
        } else {
            return InteractionResult.SUCCESS;
        }
        EntityType.createDefaultStackConfig(level, stack, player).apply(entity);
        if (!entity.survives()) {
            return InteractionResult.CONSUME;
        }
        if (!level.isClientSide()) {
            entity.playPlacementSound();
            level.gameEvent(player, GameEvent.ENTITY_PLACE, entity.position());
            level.addFreshEntity(entity);
        }
        stack.shrink(1);
        return InteractionResult.SUCCESS;
    }

    protected boolean mayPlace(Player player, Direction direction, ItemStack itemStack, BlockPos blockPos) {
        return !direction.getAxis().isVertical() && player.mayUseItemAt(blockPos, direction, itemStack);
    }

    @Override
    public void appendHoverText(ItemStack itemStack, Item.TooltipContext context, TooltipDisplay display, Consumer<Component> builder, TooltipFlag tooltipFlag) {
        if (hangingType() != EntityTypes.PAINTING || !display.shows(DataComponents.PAINTING_VARIANT)) {
            return;
        }
        Holder<PaintingVariant> variant = itemStack.get(DataComponents.PAINTING_VARIANT);
        if (variant != null) {
            variant.value().title().ifPresent(builder);
            variant.value().author().ifPresent(builder);
            builder.accept(Component.translatable("painting.dimensions", variant.value().width(), variant.value().height()));
        } else if (tooltipFlag.isCreative()) {
            builder.accept(kRandomVariant);
        }
    }
}
