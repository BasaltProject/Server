// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.TickRateManager;

public class ServerTickRateManager extends TickRateManager {
    public ServerTickRateManager(MinecraftServer server) {
    }

    public boolean isSprinting() {
        return manager().isSprinting();
    }

    public void setFrozen(boolean frozen) {
        manager().setFrozen(frozen);
    }

    public boolean stepGameIfPaused(int ticks) {
        return manager().stepGameIfFrozen(ticks);
    }

    public boolean stopStepping() {
        return manager().stopStepping();
    }

    public boolean stopSprinting() {
        return manager().stopSprinting();
    }

    public boolean requestGameToSprint(int time) {
        return manager().requestGameToSprint(time);
    }

    public boolean checkShouldSprintThisTick() {
        return manager().isSprinting();
    }

    public void endTickWork() {
    }

    public void setTickRate(float rate) {
        super.setTickRate(rate);
    }

    public void updateJoiningPlayer(ServerPlayer player) {
    }
}
