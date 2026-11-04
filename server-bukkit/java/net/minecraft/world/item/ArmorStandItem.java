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

import net.minecraft.util.Mth;
public class ArmorStandItem extends Item {
    public ArmorStandItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        if (context.getClickedFace() == Direction.DOWN) {
            return InteractionResult.FAIL;
        }
        Level level = context.getLevel();
        BlockPos pos = new BlockPlaceContext(context).getClickedPos();
        ItemStack stack = context.getItemInHand();
        Vec3 base = Vec3.atBottomCenterOf(pos);
        AABB box = net.minecraft.world.entity.EntityTypes.ARMOR_STAND.getDimensions().makeBoundingBox(base.x, base.y, base.z);
        if (!level.noCollision(null, box) || !level.getEntities((Entity) null, box).isEmpty()) {
            return InteractionResult.FAIL;
        }
        if (level instanceof ServerLevel serverLevel) {
            var stand = net.minecraft.world.entity.EntityTypes.ARMOR_STAND.create(serverLevel,
                EntityType.createDefaultStackConfig(serverLevel, stack, context.getPlayer()), pos, EntitySpawnReason.SPAWN_ITEM_USE,
                true, true);
            if (stand == null) {
                return InteractionResult.FAIL;
            }
            float yaw = Mth.floor((Mth.wrapDegrees(context.getRotation() - 180.0F) + 22.5F) / 45.0F) * 45.0F;
            stand.snapTo(stand.getX(), stand.getY(), stand.getZ(), yaw, 0.0F);
            serverLevel.addFreshEntityWithPassengers(stand);
            level.playSound(null, stand.getX(), stand.getY(), stand.getZ(), SoundEvents.ARMOR_STAND_PLACE, SoundSource.BLOCKS, 0.75F, 0.8F);
            stand.gameEvent(GameEvent.ENTITY_PLACE, context.getPlayer());
        }
        stack.shrink(1);
        return InteractionResult.SUCCESS;
    }
}
