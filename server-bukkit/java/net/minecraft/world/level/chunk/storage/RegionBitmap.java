// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import it.unimi.dsi.fastutil.ints.IntSet;
public class RegionBitmap {
    private final java.util.BitSet used = new java.util.BitSet();
    public void force(int position, int size) {
        used.set(position, position + size);
    }
    public void free(int position, int size) {
        used.clear(position, position + size);
    }
    public int allocate(int size) {
        int cursor = 0;
        while (true) {
            int start = used.nextClearBit(cursor);
            int blocker = used.nextSetBit(start);
            if (blocker == -1 || blocker - start >= size) {
                force(start, size);
                return start;
            }
            cursor = blocker;
        }
    }
    public IntSet getUsed() {
        IntSet set = new it.unimi.dsi.fastutil.ints.IntArraySet();
        used.stream().forEach(set::add);
        return set;
    }
}
