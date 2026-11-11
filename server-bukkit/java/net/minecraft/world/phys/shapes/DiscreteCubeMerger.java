// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import it.unimi.dsi.fastutil.doubles.DoubleList;
public final class DiscreteCubeMerger implements IndexMerger {
    private final CubePointRange result;
    private final int firstDiv;
    private final int secondDiv;
    DiscreteCubeMerger(int firstSize, int secondSize) {
        this.result = new CubePointRange((int) Shapes.lcm(firstSize, secondSize));
        int divisor = com.google.common.math.IntMath.gcd(firstSize, secondSize);
        this.firstDiv = firstSize / divisor;
        this.secondDiv = secondSize / divisor;
    }
    public boolean forMergedIndexes(IndexMerger.IndexConsumer consumer) {
        for (int at = 0; at < result.size() - 1; at++) {
            if (!consumer.merge(at / secondDiv, at / firstDiv, at)) {
                return false;
            }
        }
        return true;
    }
    public int size() {
        return result.size();
    }
    public DoubleList getList() {
        return result;
    }
}
