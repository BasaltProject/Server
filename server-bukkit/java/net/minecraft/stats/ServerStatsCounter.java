// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.stats;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.mojang.datafixers.DataFixer;
import java.nio.file.Path;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.player.Player;
import org.basaltmc.bukkit.player.PlayerCalls;

/** The statistics of one player, read and written through the native statistics service. */
public class ServerStatsCounter extends StatsCounter {
    private final java.util.UUID owner;

    public ServerStatsCounter(MinecraftServer server, Path file) {
        this.owner = null;
    }

    public ServerStatsCounter(ServerPlayer player) {
        this.owner = player.getUUID();
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static String keyOf(Stat<?> stat) {
        StatType type = stat.getType();
        net.minecraft.resources.Identifier typeId = BuiltInRegistries.STAT_TYPE.getKey(type);
        net.minecraft.resources.Identifier valueId = ((Registry) type.getRegistry()).getKey(stat.getValue());
        return typeId + "/" + valueId;
    }

    @Override
    public int getValue(Stat<?> stat) {
        return owner == null ? super.getValue(stat) : (int) PlayerCalls.count(owner, "statistic", keyOf(stat));
    }

    @Override
    public void setValue(Player player, Stat<?> stat, int count) {
        if (owner == null) {
            super.setValue(player, stat, count);
        } else {
            PlayerCalls.call(owner, "set_statistic", keyOf(stat), (long) count);
        }
    }

    public void save() {
    }

    public void parse(DataFixer fixerUpper, JsonElement element) {
    }

    protected JsonElement toJson() {
        return new JsonObject();
    }

    public void markAllDirty() {
    }

    public void sendStats(ServerPlayer player) {
    }
}
