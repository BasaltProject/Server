// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import it.unimi.dsi.fastutil.doubles.AbstractDoubleList;
import it.unimi.dsi.fastutil.doubles.DoubleList;
public class NonOverlappingMerger extends AbstractDoubleList implements IndexMerger {
    private final DoubleList lower;
    private final DoubleList upper;
    private final boolean swap;
    protected NonOverlappingMerger(DoubleList lower, DoubleList upper, boolean swap) {
        this.lower = lower;
        this.upper = upper;
        this.swap = swap;
    }
    private boolean walk(IndexMerger.IndexConsumer consumer) {
        int split = lower.size();
        for (int at = 0; at < split; at++) {
            if (!consumer.merge(at, -1, at)) {
                return false;
            }
        }
        for (int at = 0; at < upper.size() - 1; at++) {
            if (!consumer.merge(split - 1, at, split + at)) {
                return false;
            }
        }
        return true;
    }
    public int size() {
        return lower.size() + upper.size();
    }
    public boolean forMergedIndexes(IndexMerger.IndexConsumer consumer) {
        if (!swap) {
            return walk(consumer);
        }
        return walk((a, b, out) -> consumer.merge(b, a, out));
    }
    public double getDouble(int index) {
        int split = lower.size();
        return index < split ? lower.getDouble(index) : upper.getDouble(index - split);
    }
    public DoubleList getList() {
        return this;
    }
}
