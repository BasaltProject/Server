// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import net.minecraft.server.level.ChunkMap;
import net.minecraft.world.entity.MobCategory;
import java.util.HashMap;
import java.util.Map;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import java.util.List;
import net.minecraft.server.level.ServerPlayer;
public class LocalMobCapCalculator {
    private final Long2ObjectOpenHashMap<List<ServerPlayer>> nearChunk = new Long2ObjectOpenHashMap<>();
    private final Map<ServerPlayer, MobCounts> counts = new HashMap<>();
    private final ChunkMap chunkMap;
    public LocalMobCapCalculator(ChunkMap chunkMap) {
        this.chunkMap = chunkMap;
    }
    private List<ServerPlayer> playersNear(ChunkPos pos) {
        return nearChunk.computeIfAbsent(pos.pack(), key -> chunkMap.getPlayersCloseForSpawning(pos));
    }
    public void addMob(ChunkPos pos, MobCategory category) {
        for (ServerPlayer player : playersNear(pos)) {
            counts.computeIfAbsent(player, key -> new MobCounts()).add(category);
        }
    }
    public boolean canSpawn(MobCategory mobCategory, ChunkPos pos) {
        for (ServerPlayer player : playersNear(pos)) {
            MobCounts seen = counts.get(player);
            if (seen == null || seen.canSpawn(mobCategory)) {
                return true;
            }
        }
        return false;
    }
    static class MobCounts {
        private final int[] perCategory = new int[MobCategory.values().length];
        public void add(MobCategory category) {
            perCategory[category.ordinal()]++;
        }
        public boolean canSpawn(MobCategory category) {
            return perCategory[category.ordinal()] < category.getMaxInstancesPerChunk();
        }
    }
}
