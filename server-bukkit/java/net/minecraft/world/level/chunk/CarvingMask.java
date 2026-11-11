// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;
public class CarvingMask implements CarverOutput {
    private final int minY;
    private final int maxY;
    private final int height;
    private final java.util.BitSet mask;
    private int indexOf(int x, int y, int z) {
        return y - minY + (z + (x << 4)) * height;
    }
    public CarvingMask(int minY, int maxY) {
        this.minY = minY;
        this.maxY = maxY;
        this.height = maxY - minY + 1;
        this.mask = new java.util.BitSet(256 * height);
    }
    public int minY() {
        return minY;
    }
    public int maxY() {
        return maxY;
    }
    public void carve(int x, int y, int z) {
        mask.set(indexOf(x, y, z));
    }
    public void visit(Visitor visitor) {
        int from = mask.nextSetBit(0);
        while (from != -1) {
            int to = mask.nextClearBit(from) - 1;
            for (int column = from / height; column <= to / height; column++) {
                int base = column * height;
                int bottom = Math.max(from - base, 0) + minY;
                int top = Math.min(to - base, height - 1) + minY;
                visitor.visitColumn(column >> 4 & 15, column & 15, bottom, top);
            }
            from = mask.nextSetBit(to + 1);
        }
    }
    public boolean isEmpty() {
        return mask.isEmpty();
    }
    public interface Filter {
        boolean test(int x, int y, int z);
    }
    public interface Visitor {
        void visitColumn(int x, int z, int bottomY, int topY);
    }
}
