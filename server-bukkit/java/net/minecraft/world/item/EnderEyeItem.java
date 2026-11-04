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

import net.minecraft.world.level.block.state.pattern.BlockPattern;
public class EnderEyeItem extends Item {
    public EnderEyeItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        Level level = context.getLevel();
        BlockPos pos = context.getClickedPos();
        BlockState frame = level.getBlockState(pos);
        if (!frame.is(Blocks.END_PORTAL_FRAME) || frame.getValue(EndPortalFrameBlock.HAS_EYE)) {
            return InteractionResult.PASS;
        }
        if (level.isClientSide()) {
            return InteractionResult.SUCCESS;
        }
        BlockState filled = frame.setValue(EndPortalFrameBlock.HAS_EYE, true);
        Block.pushEntitiesUp(frame, filled, level, pos);
        level.setBlock(pos, filled, 2);
        level.updateNeighbourForOutputSignal(pos, Blocks.END_PORTAL_FRAME);
        context.getItemInHand().shrink(1);
        level.levelEvent(1503, pos, 0);
        BlockPattern.BlockPatternMatch match = EndPortalFrameBlock.getOrCreatePortalShape().find(level, pos);
        if (match != null) {
            BlockPos corner = match.getFrontTopLeft().offset(-3, 0, -3);
            for (int x = 0; x < 3; x++) {
                for (int z = 0; z < 3; z++) {
                    BlockPos portal = corner.offset(x, 0, z);
                    level.destroyBlock(portal, true, null);
                    level.setBlock(portal, Blocks.END_PORTAL.defaultBlockState(), 2);
                }
            }
            level.globalLevelEvent(1038, corner.offset(1, 0, 1), 0);
        }
        return InteractionResult.SUCCESS;
    }

    @Override
    public int getUseDuration(ItemStack itemStack, LivingEntity user) {
        return 0;
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        ItemStack held = player.getItemInHand(hand);
        BlockHitResult target = getPlayerPOVHitResult(level, player, ClipContext.Fluid.NONE);
        if (target.getType() == HitResult.Type.BLOCK && level.getBlockState(target.getBlockPos()).is(Blocks.END_PORTAL_FRAME)) {
            return InteractionResult.PASS;
        }
        player.startUsingItem(hand);
        if (level instanceof ServerLevel serverLevel) {
            BlockPos found = serverLevel.findNearestMapStructure(StructureTags.EYE_OF_ENDER_LOCATED, player.blockPosition(), 100, false);
            if (found == null) {
                return InteractionResult.CONSUME;
            }
            EyeOfEnder eye = new EyeOfEnder(level, player.getX(), player.getY(0.5), player.getZ());
            eye.setItem(held);
            eye.signalTo(Vec3.atLowerCornerOf(found));
            level.gameEvent(GameEvent.PROJECTILE_SHOOT, eye.position(), GameEvent.Context.of((Entity) player));
            level.addFreshEntity(eye);
            if (player instanceof ServerPlayer serverPlayer && CriteriaTriggers.USED_ENDER_EYE != null) {
                CriteriaTriggers.USED_ENDER_EYE.trigger(serverPlayer, found);
            }
            float pitch = Mth.lerp(level.getRandom().nextFloat(), 0.33F, 0.5F);
            level.playSound(null, player.getX(), player.getY(), player.getZ(), SoundEvents.ENDER_EYE_LAUNCH, SoundSource.NEUTRAL, 1.0F, pitch);
            held.consume(1, player);
            player.awardStat(Stats.ITEM_USED.get(this));
        }
        return InteractionResult.SUCCESS_SERVER;
    }
}
