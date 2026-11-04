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

import net.minecraft.world.entity.decoration.LeashFenceKnotEntity;
public class LeadItem extends Item {
    public LeadItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        Level level = context.getLevel();
        BlockPos pos = context.getClickedPos();
        Player player = context.getPlayer();
        if (level.getBlockState(pos).is(BlockTags.FENCES) && !level.isClientSide() && player != null) {
            return bindPlayerMobs(player, level, pos);
        }
        return InteractionResult.PASS;
    }

    public static InteractionResult bindPlayerMobs(Player player, Level level, BlockPos pos) {
        List<Leashable> candidates = Leashable.leashableInArea(level, Vec3.atCenterOf(pos), leashable -> leashable.getLeashHolder() == player);
        if (candidates.isEmpty()) {
            return InteractionResult.PASS;
        }
        Optional<LeashFenceKnotEntity> existing = LeashFenceKnotEntity.getKnot(level, pos);
        LeashFenceKnotEntity knot = existing.orElseGet(() -> LeashFenceKnotEntity.createKnot(level, pos));
        boolean attached = false;
        for (Leashable leashable : candidates) {
            if (leashable.canHaveALeashAttachedTo(knot)) {
                leashable.setLeashedTo(knot, true);
                attached = true;
            }
        }
        if (attached) {
            knot.playPlacementSound();
            level.gameEvent(GameEvent.BLOCK_ATTACH, pos, GameEvent.Context.of((Entity) player));
            return InteractionResult.SUCCESS_SERVER;
        }
        if (existing.isEmpty()) {
            knot.discard();
        }
        return InteractionResult.PASS;
    }
}
