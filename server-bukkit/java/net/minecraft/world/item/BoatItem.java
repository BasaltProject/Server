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

import net.minecraft.world.entity.vehicle.boat.AbstractBoat;
public class BoatItem extends Item {
    private final EntityType<? extends AbstractBoat> entityType;

    public BoatItem(EntityType<? extends AbstractBoat> entityType, Item.Properties properties) {
        super(properties);
        this.entityType = entityType;
    }

    @SuppressWarnings("unchecked")
    private EntityType<? extends AbstractBoat> boatType() {
        return entityType != null ? entityType : (EntityType<? extends AbstractBoat>) ItemClasses.entityType(this);
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        ItemStack held = player.getItemInHand(hand);
        HitResult target = getPlayerPOVHitResult(level, player, ClipContext.Fluid.ANY);
        if (target.getType() == HitResult.Type.MISS) {
            return InteractionResult.PASS;
        }
        Vec3 look = player.getViewVector(1.0F);
        List<Entity> nearby = level.getEntities(player, player.getBoundingBox().expandTowards(look.scale(5.0)).inflate(1.0), EntitySelector.CAN_BE_PICKED);
        Vec3 eye = player.getEyePosition();
        for (Entity entity : nearby) {
            if (entity.getBoundingBox().inflate(entity.getPickRadius()).contains(eye)) {
                return InteractionResult.PASS;
            }
        }
        if (target.getType() != HitResult.Type.BLOCK) {
            return InteractionResult.PASS;
        }
        AbstractBoat boat = createBoat(level, target, held, player);
        if (boat == null) {
            return InteractionResult.FAIL;
        }
        boat.setYRot(player.getYRot());
        if (!level.noCollision(boat, boat.getBoundingBox())) {
            return InteractionResult.FAIL;
        }
        if (!level.isClientSide()) {
            level.addFreshEntity(boat);
            level.gameEvent(player, GameEvent.ENTITY_PLACE, target.getLocation());
            held.consume(1, player);
        }
        player.awardStat(Stats.ITEM_USED.get(this));
        return InteractionResult.SUCCESS;
    }

    private AbstractBoat createBoat(Level level, HitResult hit, ItemStack itemStack, Player player) {
        AbstractBoat boat = (AbstractBoat) boatType().create(level, EntitySpawnReason.SPAWN_ITEM_USE);
        if (boat != null) {
            Vec3 at = hit.getLocation();
            boat.setInitialPos(at.x, at.y, at.z);
            if (level instanceof ServerLevel serverLevel) {
                EntityType.createDefaultStackConfig(serverLevel, itemStack, player).apply(boat);
            }
        }
        return boat;
    }
}
