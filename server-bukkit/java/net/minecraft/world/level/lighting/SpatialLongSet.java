// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
import net.minecraft.util.Mth;
import java.util.NoSuchElementException;
import it.unimi.dsi.fastutil.longs.Long2LongLinkedOpenHashMap;
import it.unimi.dsi.fastutil.longs.LongLinkedOpenHashSet;
public class SpatialLongSet extends LongLinkedOpenHashSet {
    private final InternalMap map;
    public SpatialLongSet(int expected, float f) {
        super();
        this.map = new InternalMap(expected / 64, f);
    }
    public boolean add(long k) {
        return map.addBit(k);
    }
    public boolean rem(long k) {
        return map.removeBit(k);
    }
    public long removeFirstLong() {
        return map.removeFirstBit();
    }
    public int size() {
        throw new UnsupportedOperationException();
    }
    public boolean isEmpty() {
        return map.isEmpty();
    }
    static class InternalMap extends Long2LongLinkedOpenHashMap {
        private static final int kXBits = Mth.log2(60000000);
        private static final int kZBits = kXBits;
        private static final int kYBits = 64 - kXBits - kZBits;
        private static final int kZOffset = kYBits;
        private static final int kXOffset = kYBits + kZBits;
        private static final long kOuterMask = 3L << kXOffset | 3L | 3L << kZOffset;
        private final java.util.LinkedHashMap<Long, Long> buckets = new java.util.LinkedHashMap<>();
        @Override
        public boolean isEmpty() {
            return buckets.isEmpty();
        }
        public InternalMap(int expected, float f) {
            
        }
        static long getOuterKey(long key) {
            return key & ~kOuterMask;
        }
        static int getInnerKey(long key) {
            long x = key >>> kXOffset & 3L;
            long y = key & 3L;
            long z = key >>> kZOffset & 3L;
            return (int) (x << 4 | z << 2 | y);
        }
        static long getFullKey(long outerKey, int innerKey) {
            return outerKey | (long) (innerKey >>> 4 & 3) << kXOffset | (long) (innerKey >>> 2 & 3) << kZOffset | (long) (innerKey & 3);
        }
        public boolean addBit(long key) {
            long outer = getOuterKey(key);
            long bit = 1L << getInnerKey(key);
            Long held = buckets.get(outer);
            if (held == null) {
                buckets.put(outer, bit);
                return false;
            }
            buckets.put(outer, held | bit);
            return (held & bit) != 0L;
        }
        public boolean removeBit(long key) {
            long outer = getOuterKey(key);
            long bit = 1L << getInnerKey(key);
            Long held = buckets.get(outer);
            if (held == null || (held & bit) == 0L) {
                return false;
            }
            long left = held & ~bit;
            if (left == 0L) {
                buckets.remove(outer);
            } else {
                buckets.put(outer, left);
            }
            return true;
        }
        public long removeFirstBit() {
            java.util.Iterator<java.util.Map.Entry<Long, Long>> head = buckets.entrySet().iterator();
            if (!head.hasNext()) {
                throw new NoSuchElementException();
            }
            java.util.Map.Entry<Long, Long> entry = head.next();
            int inner = Long.numberOfTrailingZeros(entry.getValue());
            long outer = entry.getKey();
            long left = entry.getValue() & ~(1L << inner);
            if (left == 0L) {
                head.remove();
            } else {
                entry.setValue(left);
            }
            return getFullKey(outer, inner);
        }
        protected void rehash(int newN) {
        }
    }
}
