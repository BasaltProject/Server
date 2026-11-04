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

import net.minecraft.core.particles.BlockParticleOption;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.world.entity.HumanoidArm;
import net.minecraft.world.level.block.RenderShape;
import net.minecraft.world.level.block.BrushableBlock;
import net.minecraft.world.entity.EntitySelector;
import net.minecraft.world.entity.projectile.ProjectileUtil;
public class BrushItem extends Item {
    public static final int ANIMATION_DURATION = 10;
    private static final int kUseDuration = 200;

    public BrushItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        Player player = context.getPlayer();
        if (player != null && calculateHitResult(player).getType() == HitResult.Type.BLOCK) {
            player.startUsingItem(context.getHand());
        }
        return InteractionResult.CONSUME;
    }

    @Override
    public ItemUseAnimation getUseAnimation(ItemStack itemStack) {
        return ItemUseAnimation.BRUSH;
    }

    @Override
    public int getUseDuration(ItemStack itemStack, LivingEntity user) {
        return kUseDuration;
    }

    @Override
    public void onUseTick(Level level, LivingEntity livingEntity, ItemStack itemStack, int ticksRemaining) {
        if (ticksRemaining < 0 || !(livingEntity instanceof Player player)
            || !(calculateHitResult(player) instanceof BlockHitResult hit) || hit.getType() != HitResult.Type.BLOCK) {
            livingEntity.releaseUsingItem();
            return;
        }
        int elapsed = getUseDuration(itemStack, livingEntity) - ticksRemaining + 1;
        if (elapsed % ANIMATION_DURATION != 5) {
            return;
        }
        BlockPos pos = hit.getBlockPos();
        BlockState state = level.getBlockState(pos);
        HumanoidArm arm = livingEntity.getUsedItemHand() == InteractionHand.MAIN_HAND ? player.getMainArm() : player.getMainArm().getOpposite();
        if (state.shouldSpawnTerrainParticles() && state.getRenderShape() != RenderShape.INVISIBLE) {
            spawnDustParticles(level, hit, state, livingEntity.getViewVector(0.0F), arm);
        }
        SoundEvent sound = state.getBlock() instanceof BrushableBlock brushable ? brushable.getBrushSound() : SoundEvents.BRUSH_GENERIC;
        level.playSound(player, pos, sound, SoundSource.BLOCKS);
    }

    private HitResult calculateHitResult(Player player) {
        return ProjectileUtil.getHitResultOnViewVector(player, EntitySelector.CAN_BE_PICKED, player.blockInteractionRange());
    }

    private void spawnDustParticles(Level level, BlockHitResult hit, BlockState state, Vec3 view, HumanoidArm arm) {
        int flip = arm == HumanoidArm.RIGHT ? 1 : -1;
        int count = level.getRandom().nextInt(7, 12);
        BlockParticleOption particle = new BlockParticleOption(ParticleTypes.BLOCK, state);
        Direction face = hit.getDirection();
        DustParticlesDelta delta = DustParticlesDelta.fromDirection(view, face);
        Vec3 at = hit.getLocation();
        for (int i = 0; i < count; i++) {
            level.addParticle(particle, at.x - (face == Direction.WEST ? 1.0E-6F : 0.0F), at.y, at.z - (face == Direction.NORTH ? 1.0E-6F : 0.0F),
                delta.xd() * flip * 3.0 * level.getRandom().nextDouble(), 0.0, delta.zd() * flip * 3.0 * level.getRandom().nextDouble());
        }
    }

    static record DustParticlesDelta(double xd, double yd, double zd) {
        static DustParticlesDelta fromDirection(Vec3 view, Direction face) {
            return switch (face) {
                case DOWN, UP -> new DustParticlesDelta(view.z(), 0.0, -view.x());
                case NORTH -> new DustParticlesDelta(1.0, 0.0, -0.1);
                case SOUTH -> new DustParticlesDelta(-1.0, 0.0, 0.1);
                case WEST -> new DustParticlesDelta(-0.1, 0.0, -1.0);
                case EAST -> new DustParticlesDelta(0.1, 0.0, 1.0);
            };
        }
    }
}
