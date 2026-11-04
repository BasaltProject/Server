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

import net.minecraft.network.protocol.game.ClientboundLevelParticlesPacket;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.world.attribute.EnvironmentAttributes;
import net.minecraft.world.level.material.*;
public class BucketItem extends Item implements DispensibleContainerItem {
    protected final Fluid content;

    public BucketItem(Fluid content, Item.Properties properties) {
        super(properties);
        this.content = content;
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        ItemStack held = player.getItemInHand(hand);
        BlockHitResult target = getPlayerPOVHitResult(level, player, getFluidContext());
        if (target.getType() != HitResult.Type.BLOCK) {
            return InteractionResult.PASS;
        }
        BlockPos pos = target.getBlockPos();
        Direction face = target.getDirection();
        BlockPos beside = pos.relative(face);
        if (!level.mayInteract(player, pos) || !player.mayUseItemAt(beside, face, held)) {
            return InteractionResult.FAIL;
        }
        BlockState clicked = level.getBlockState(pos);
        BlockPos placePos = clicked.getBlock() instanceof LiquidBlockContainer && content == Fluids.WATER ? pos : beside;
        if (emptyContents(player, level, placePos, target)) {
            checkExtraContent(player, level, held, placePos);
            if (player instanceof ServerPlayer serverPlayer && content != Fluids.EMPTY && CriteriaTriggers.PLACED_BLOCK != null) {
                CriteriaTriggers.PLACED_BLOCK.trigger(serverPlayer, placePos, held);
            }
            player.awardStat(Stats.ITEM_USED.get(this));
            return InteractionResult.SUCCESS.heldItemTransformedTo(ItemUtils.createFilledResult(held, player, getEmptySuccessItem(held, player)));
        }
        if (content == Fluids.EMPTY && clicked.getBlock() instanceof BucketPickup pickup) {
            ItemStack taken = pickup.pickupBlock(player, level, pos, clicked);
            if (!taken.isEmpty()) {
                player.awardStat(Stats.ITEM_USED.get(this));
                pickup.getPickupSound().ifPresent(sound -> player.playSound(sound, 1.0F, 1.0F));
                level.gameEvent(player, GameEvent.FLUID_PICKUP, pos);
                ItemStack result = ItemUtils.createFilledResult(held, player, taken);
                if (!level.isClientSide() && CriteriaTriggers.FILLED_BUCKET != null) {
                    CriteriaTriggers.FILLED_BUCKET.trigger((ServerPlayer) player, taken);
                }
                return InteractionResult.SUCCESS.heldItemTransformedTo(result);
            }
        }
        return InteractionResult.FAIL;
    }

    public ClipContext.Fluid getFluidContext() {
        return content == Fluids.EMPTY ? ClipContext.Fluid.SOURCE_ONLY : ClipContext.Fluid.NONE;
    }

    public static ItemStack getEmptySuccessItem(ItemStack itemStack, Player player) {
        return player.hasInfiniteMaterials() ? itemStack : new ItemStack(Items.BUCKET);
    }

    @Override
    public void checkExtraContent(LivingEntity user, Level level, ItemStack itemStack, BlockPos pos) {
    }

    @Override
    public boolean emptyContents(LivingEntity user, Level level, BlockPos pos, BlockHitResult hitResult) {
        if (!(content instanceof FlowingFluid flowing)) {
            return false;
        }
        BlockState state = level.getBlockState(pos);
        Block block = state.getBlock();
        boolean replaceable = state.canBeReplaced(content);
        boolean sneaking = user != null && user.isShiftKeyDown();
        boolean accepts = replaceable || block instanceof LiquidBlockContainer container && container.canPlaceLiquid(user, level, pos, state, content);
        if (!(state.isAir() || accepts && (!sneaking || hitResult == null))) {
            return hitResult != null && emptyContents(user, level, hitResult.getBlockPos().relative(hitResult.getDirection()), null);
        }
        if (level.environmentAttributes().getValue(EnvironmentAttributes.WATER_EVAPORATES, pos) && content.is(FluidTags.WATER)) {
            RandomSource random = level.getRandom();
            level.playSound(user, pos, SoundEvents.FIRE_EXTINGUISH, SoundSource.BLOCKS, 0.5F, 2.6F + (random.nextFloat() - random.nextFloat()) * 0.8F);
            if (level instanceof ServerLevel serverLevel) {
                serverLevel.sendParticles(ParticleTypes.LARGE_SMOKE, pos.getX(), pos.getY(), pos.getZ(), 8, 1.0, 1.0, 1.0, 0.0,
                    ClientboundLevelParticlesPacket.RandomizationType.ALTERNATIVE);
            }
            return true;
        }
        if (block instanceof LiquidBlockContainer container && content == Fluids.WATER) {
            container.placeLiquid(level, pos, state, flowing.getSource(false));
            playEmptySound(user, level, pos);
            return true;
        }
        if (!level.isClientSide() && replaceable && !state.liquid()) {
            level.destroyBlock(pos, true);
        }
        if (!level.setBlock(pos, content.defaultFluidState().createLegacyBlock(), 11) && !state.getFluidState().isSource()) {
            return false;
        }
        playEmptySound(user, level, pos);
        return true;
    }

    protected void playEmptySound(LivingEntity user, LevelAccessor level, BlockPos pos) {
        SoundEvent sound = content.is(FluidTags.LAVA) ? SoundEvents.BUCKET_EMPTY_LAVA : SoundEvents.BUCKET_EMPTY;
        level.playSound(user, pos, sound, SoundSource.BLOCKS, 1.0F, 1.0F);
        level.gameEvent(user, GameEvent.FLUID_PLACE, pos);
    }

    public Fluid getContent() {
        return content;
    }
}
