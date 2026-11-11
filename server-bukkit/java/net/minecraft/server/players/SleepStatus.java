// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import java.util.List;
import net.minecraft.server.level.ServerPlayer;

public class SleepStatus {
    private int awake;
    private int asleep;

    public boolean areEnoughSleeping(int sleepPercentageNeeded) {
        return asleep >= sleepersNeeded(sleepPercentageNeeded);
    }

    public boolean areEnoughDeepSleeping(int sleepPercentageNeeded, List<ServerPlayer> players) {
        long deep = players.stream().filter(player -> player.isSleepingLongEnough()).count();
        return deep >= sleepersNeeded(sleepPercentageNeeded);
    }

    public int sleepersNeeded(int sleepPercentageNeeded) {
        return Math.max(1, (int) Math.ceil((float) (awake * sleepPercentageNeeded) / 100.0F));
    }

    public void removeAllSleepers() {
        asleep = 0;
    }

    public int amountSleeping() {
        return asleep;
    }

    public boolean update(List<ServerPlayer> players) {
        int before = awake;
        int sleptBefore = asleep;
        awake = 0;
        asleep = 0;
        for (ServerPlayer player : players) {
            if (player.isSpectator()) {
                continue;
            }
            awake++;
            if (player.isSleeping()) {
                asleep++;
            }
        }
        return (sleptBefore > 0 || asleep > 0) && (before != awake || sleptBefore != asleep);
    }
}
