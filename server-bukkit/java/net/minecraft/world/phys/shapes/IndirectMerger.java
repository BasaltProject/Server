// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import it.unimi.dsi.fastutil.doubles.DoubleList;
public class IndirectMerger implements IndexMerger {
    private static final DoubleList EMPTY = it.unimi.dsi.fastutil.doubles.DoubleLists.unmodifiable(it.unimi.dsi.fastutil.doubles.DoubleArrayList.wrap(new double[] {0.0}));
    private final double[] result;
    private final int[] firstIndices;
    private final int[] secondIndices;
    private final int resultLength;
    public IndirectMerger(DoubleList first, DoubleList second, boolean firstOnlyMatters, boolean secondOnlyMatters) {
        int n1 = first.size();
        int n2 = second.size();
        result = new double[n1 + n2];
        firstIndices = new int[n1 + n2];
        secondIndices = new int[n1 + n2];
        int out = 0;
        int i = 0;
        int j = 0;
        double prev = Double.NaN;
        while (i < n1 || j < n2) {
            boolean useFirst = i < n1 && (j >= n2 || first.getDouble(i) < second.getDouble(j) + 1.0E-7);
            if (useFirst) {
                i++;
                if (!firstOnlyMatters && (j == 0 || j >= n2)) {
                    continue;
                }
            } else {
                j++;
                if (!secondOnlyMatters && (i == 0 || i >= n1)) {
                    continue;
                }
            }
            double value = useFirst ? first.getDouble(i - 1) : second.getDouble(j - 1);
            if (prev >= value - 1.0E-7) {
                firstIndices[out - 1] = i - 1;
                secondIndices[out - 1] = j - 1;
            } else {
                firstIndices[out] = i - 1;
                secondIndices[out] = j - 1;
                result[out++] = value;
                prev = value;
            }
        }
        resultLength = Math.max(1, out);
    }
    public boolean forMergedIndexes(IndexMerger.IndexConsumer consumer) {
        for (int at = 0; at < resultLength - 1; at++) {
            if (!consumer.merge(firstIndices[at], secondIndices[at], at)) {
                return false;
            }
        }
        return true;
    }
    public int size() {
        return resultLength;
    }
    public DoubleList getList() {
        return resultLength <= 1 ? EMPTY : it.unimi.dsi.fastutil.doubles.DoubleArrayList.wrap(result, resultLength);


    }
}
