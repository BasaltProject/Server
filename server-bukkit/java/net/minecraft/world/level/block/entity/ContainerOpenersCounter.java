// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.ContainerUser;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.AABB;

public abstract class ContainerOpenersCounter {
    private static final int kRecheckDelay = 5;

    private int openers;
    private double reach;

    protected abstract void onOpen(Level level, BlockPos pos, BlockState blockState);

    protected abstract void onClose(Level level, BlockPos pos, BlockState blockState);

    protected abstract void openerCountChanged(Level level, BlockPos pos, BlockState blockState, int previous, int current);

    public abstract boolean isOwnContainer(Player player);

    private static void queueRecheck(Level level, BlockPos pos, BlockState state) {
        level.scheduleTick(pos, state.getBlock(), kRecheckDelay);
    }

    public void incrementOpeners(LivingEntity entity, Level level, BlockPos pos, BlockState blockState, double maxInteractionRange) {
        int before = openers;
        openers = before + 1;
        if (before == 0) {
            onOpen(level, pos, blockState);
            level.gameEvent(entity, GameEvent.CONTAINER_OPEN, pos);
            queueRecheck(level, pos, blockState);
        }
        openerCountChanged(level, pos, blockState, before, openers);
        reach = Math.max(reach, maxInteractionRange);
    }

    public void decrementOpeners(LivingEntity entity, Level level, BlockPos pos, BlockState blockState) {
        int before = openers;
        openers = before - 1;
        if (openers == 0) {
            onClose(level, pos, blockState);
            level.gameEvent(entity, GameEvent.CONTAINER_CLOSE, pos);
            reach = 0.0;
        }
        openerCountChanged(level, pos, blockState, before, openers);
    }

    public List<ContainerUser> getEntitiesWithContainerOpen(Level level, BlockPos pos) {
        AABB area = new AABB(pos).inflate(reach + 4.0);
        List<ContainerUser> users = new ArrayList<>();
        for (Entity candidate : level.getEntities((Entity) null, area, e -> e instanceof ContainerUser user && user.hasContainerOpen(this, pos))) {
            users.add((ContainerUser) candidate);
        }
        return users;
    }

    public void recheckOpeners(Level level, BlockPos pos, BlockState blockState) {
        List<ContainerUser> users = getEntitiesWithContainerOpen(level, pos);
        reach = 0.0;
        for (ContainerUser user : users) {
            reach = Math.max(reach, user.getContainerInteractionRange());
        }
        int now = users.size();
        int before = openers;
        if (now != before) {
            if (now > 0 && before == 0) {
                onOpen(level, pos, blockState);
                level.gameEvent((Entity) null, GameEvent.CONTAINER_OPEN, pos);
            } else if (now == 0) {
                onClose(level, pos, blockState);
                level.gameEvent((Entity) null, GameEvent.CONTAINER_CLOSE, pos);
            }
            openers = now;
        }
        openerCountChanged(level, pos, blockState, before, now);
        if (now > 0) {
            queueRecheck(level, pos, blockState);
        }
    }

    public int getOpenerCount() {
        return openers;
    }
}
