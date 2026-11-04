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

import net.minecraft.world.entity.boss.enderdragon.EndCrystal;
public class EndCrystalItem extends Item {
    public EndCrystalItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        Level level = context.getLevel();
        BlockPos pos = context.getClickedPos();
        BlockState clicked = level.getBlockState(pos);
        if (!clicked.is(Blocks.OBSIDIAN) && !clicked.is(Blocks.BEDROCK)) {
            return InteractionResult.FAIL;
        }
        BlockPos above = pos.above();
        if (!level.isEmptyBlock(above)) {
            return InteractionResult.FAIL;
        }
        double x = above.getX();
        double y = above.getY();
        double z = above.getZ();
        if (!level.getEntities((Entity) null, new AABB(x, y, z, x + 1.0, y + 2.0, z + 1.0)).isEmpty()) {
            return InteractionResult.FAIL;
        }
        if (level instanceof ServerLevel serverLevel) {
            EndCrystal crystal = new EndCrystal(level, x + 0.5, y, z + 0.5);
            crystal.setShowBottom(false);
            level.addFreshEntity(crystal);
            level.gameEvent(context.getPlayer(), GameEvent.ENTITY_PLACE, above);
            var fight = serverLevel.getDragonFight();
            if (fight != null) {
                fight.tryRespawn();
            }
        }
        context.getItemInHand().shrink(1);
        return InteractionResult.SUCCESS;
    }
}
