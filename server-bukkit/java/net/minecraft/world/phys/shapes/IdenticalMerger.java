// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import it.unimi.dsi.fastutil.doubles.DoubleList;
public class IdenticalMerger implements IndexMerger {
    private final DoubleList coords;
    public IdenticalMerger(DoubleList coords) {
        this.coords = coords;
    }
    public boolean forMergedIndexes(IndexMerger.IndexConsumer consumer) {
        for (int at = 0; at < coords.size() - 1; at++) {
            if (!consumer.merge(at, at, at)) {
                return false;
            }
        }
        return true;
    }
    public int size() {
        return coords.size();
    }
    public DoubleList getList() {
        return coords;
    }
}
