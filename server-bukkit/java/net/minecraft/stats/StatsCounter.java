// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.stats;

import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntMaps;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import net.minecraft.world.entity.player.Player;

public class StatsCounter {
    protected final Object2IntMap<Stat<?>> stats = Object2IntMaps.synchronize(new Object2IntOpenHashMap<>());

    public StatsCounter() {
        stats.defaultReturnValue(0);
    }

    public void increment(Player player, Stat<?> stat, int count) {
        int value = (int) Math.min((long) getValue(stat) + (long) count, 2147483647L);
        setValue(player, stat, value);
    }

    public void setValue(Player player, Stat<?> stat, int count) {
        stats.put(stat, count);
    }

    public <T> int getValue(StatType<T> type, T key) {
        return type.contains(key) ? getValue(type.get(key)) : 0;
    }

    public int getValue(Stat<?> stat) {
        return stats.getInt(stat);
    }
}
