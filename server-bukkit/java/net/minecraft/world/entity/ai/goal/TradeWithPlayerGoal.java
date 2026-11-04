// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.npc.villager.AbstractVillager;
import net.minecraft.world.entity.player.Player;

public class TradeWithPlayerGoal extends Goal {
    private final AbstractVillager mob;

    public TradeWithPlayerGoal(AbstractVillager mob) {
        this.mob = mob;
        setFlags(EnumSet.of(Goal.Flag.JUMP, Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        if (!mob.isAlive() || mob.isInWater() || !mob.onGround() || mob.wasHurtRecently()) {
            return false;
        }
        Player trader = mob.getTradingPlayer();
        return trader != null && mob.distanceToSqr(trader) <= 16.0;
    }

    @Override
    public void start() {
        mob.getNavigation().stop();
    }

    @Override
    public void stop() {
        mob.setTradingPlayer(null);
    }
}
