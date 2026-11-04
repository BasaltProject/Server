// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.world.entity.npc.villager.AbstractVillager;
import net.minecraft.world.entity.player.Player;

public class LookAtTradingPlayerGoal extends LookAtPlayerGoal {
    private final AbstractVillager villager;

    public LookAtTradingPlayerGoal(AbstractVillager villager) {
        super(villager, Player.class, 8.0F);
        this.villager = villager;
    }

    @Override
    public boolean canUse() {
        if (!villager.isTrading()) {
            return false;
        }
        lookAt = villager.getTradingPlayer();
        return true;
    }
}
