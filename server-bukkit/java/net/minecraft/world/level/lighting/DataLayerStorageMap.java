// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import net.minecraft.world.level.chunk.DataLayer;
public abstract class DataLayerStorageMap<M extends DataLayerStorageMap<M>> {
    protected final Long2ObjectOpenHashMap<DataLayer> map;
    private boolean cacheEnabled = true;
    protected DataLayerStorageMap(Long2ObjectOpenHashMap<DataLayer> map) {
        this.map = map;
    }
    public abstract M copy();
    public DataLayer copyDataLayer(long sectionNode) {
        DataLayer duplicate = map.get(sectionNode).copy();
        map.put(sectionNode, duplicate);
        return duplicate;
    }
    public boolean hasLayer(long sectionNode) {
        return map.containsKey(sectionNode);
    }
    public DataLayer getLayer(long sectionNode) {
        return map.get(sectionNode);
    }
    public DataLayer removeLayer(long sectionNode) {
        return map.remove(sectionNode);
    }
    public void setLayer(long sectionNode, DataLayer layer) {
        map.put(sectionNode, layer);
    }
    public void clearCache() {
    }
    public void disableCache() {
        this.cacheEnabled = false;


    }
}
