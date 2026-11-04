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

import net.minecraft.world.entity.decoration.Cushion;
public class CushionItem extends Item {
    private static final double kRaycastEpsilon = 0.001;

    public CushionItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        UseOnContext adjusted = recalculateContext(context);
        Level level = context.getLevel();
        if (adjusted.getClickedFace() != Direction.UP) {
            return InteractionResult.FAIL;
        }
        BlockPlaceContext placement = new BlockPlaceContext(adjusted);
        BlockPos pos = placement.getClickedPos();
        Vec3 at = Vec3.atCenterOfWithY(pos, adjusted.getClickLocation().y);
        AABB spawnBox = EntityTypes.CUSHION.getSpawnAABB(at);
        if (!Cushion.canBePlacedAt(level, spawnBox)) {
            return InteractionResult.FAIL;
        }
        ItemStack stack = context.getItemInHand();
        if (level instanceof ServerLevel serverLevel) {
            if (!serverLevel.getEntitiesOfClass(Cushion.class, spawnBox).isEmpty()) {
                return InteractionResult.FAIL;
            }
            Cushion cushion = EntityTypes.CUSHION.create(serverLevel, EntityType.createDefaultStackConfig(serverLevel, stack, context.getPlayer()), pos,
                EntitySpawnReason.SPAWN_ITEM_USE, true, true);
            if (cushion == null) {
                return InteractionResult.FAIL;
            }
            cushion.snapTo(at, Direction.fromYRot(placement.getRotation()).toYRot(), 0.0F);
            serverLevel.addFreshEntity(cushion);
            cushion.destroyIfInFire(serverLevel);
            level.playSound(null, cushion.getX(), cushion.getY(), cushion.getZ(), SoundEvents.CUSHION_PLACE, SoundSource.BLOCKS, 0.75F, 0.8F);
            cushion.gameEvent(GameEvent.ENTITY_PLACE, context.getPlayer());
            stack.consume(1, placement.getPlayer());
        }
        return InteractionResult.SUCCESS;
    }

    private static UseOnContext recalculateContext(UseOnContext context) {
        Player player = context.getPlayer();
        if (player == null) {
            return context;
        }
        Level level = context.getLevel();
        BlockPos pos = context.getClickedPos();
        BlockState state = level.getBlockState(pos);
        if (!state.is(BlockTags.CUSHION_USES_COLLISION_SHAPE)) {
            return context;
        }
        Vec3 from = player.getEyePosition();
        Vec3 ray = context.getClickLocation().subtract(from);
        Vec3 to = context.getClickLocation().add(ray.normalize().scale(kRaycastEpsilon));
        BlockHitResult hit = state.getCollisionShape(level, pos).clip(from, to, pos);
        return hit == null ? context : new UseOnContext(player, context.getHand(), hit);
    }
}
