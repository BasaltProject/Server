package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;

public final class Blocks {
    private static final boolean kTrace = System.getenv("BASALT_BUKKIT_TRACE") != null;
    private static final int kMaxPendingSections = 512;
    private static final int kMaxCachedSections = 256;
    private static final ThreadLocal<Local> kLocal = ThreadLocal.withInitial(Local::new);

    private Blocks() {}

    private static final class Local {
        int depth;
        long epoch;
        final Map<BasaltWorld, Buf> buffers = new IdentityHashMap<>();
    }

    static void begin() {
        Local local = kLocal.get();
        if (local.depth++ == 0) {
            local.epoch++;
        }
    }

    static void end() {
        Local local = kLocal.get();
        if (--local.depth == 0) {
            flush(local);
        }
    }

    public static void flushAll() {
        Local local = kLocal.get();
        if (local.depth > 0) {
            flush(local);
        }
    }

    private static void flush(Local local) {
        for (Buf buf : local.buffers.values()) {
            buf.flush();
        }
    }

    static Buf of(BasaltWorld world) {
        Local local = kLocal.get();
        if (local.depth == 0) {
            if (kTrace) {
                Natives.log(1, "Blocks", "direct access outside an entry");
            }
            return null;
        }
        Buf buf = local.buffers.get(world);
        if (buf == null) {
            buf = new Buf(world);
            local.buffers.put(world, buf);
        }
        if (buf.epoch != local.epoch) {
            buf.epoch = local.epoch;
            buf.cache.clear();
            buf.lastCacheArray = null;
        }
        return buf;
    }

    static long key(int chunkX, int chunkZ, int section) {
        return ((long) (chunkX & 0x3FFFFFF) << 38) | ((long) (chunkZ & 0x3FFFFFF) << 12) | ((section + 2048) & 0xFFF);
    }

    static final class Buf {
        private final BasaltWorld world;
        private final String name;
        private final int handle;
        private final int minY;
        private final int maxY;
        long epoch;
        private final HashMap<Long, int[]> pending = new HashMap<>();
        private long lastPendingKey = Long.MIN_VALUE;
        private int[] lastPendingArray;
        final HashMap<Long, int[]> cache = new HashMap<>();
        private long cacheVersion = -2;
        private long lastCacheKey = Long.MIN_VALUE;
        int[] lastCacheArray;
        private final List<int[]> spare = new ArrayList<>();

        Buf(BasaltWorld world) {
            this.world = world;
            this.name = world.getName();
            this.handle = WorldNatives.worldHandle(name);
            this.minY = world.getMinHeight();
            this.maxY = world.getMaxHeight();
        }

        int state(int x, int y, int z) {
            if (y < minY || y >= maxY) {
                return -1;
            }
            int sy = y >> 4;
            long key = key(x >> 4, z >> 4, sy);
            int index = ((y & 15) << 8) | ((z & 15) << 4) | (x & 15);
            if (!pending.isEmpty()) {
                int[] changed = key == lastPendingKey ? lastPendingArray : pending.get(key);
                if (changed != null && changed[index] >= 0) {
                    return changed[index];
                }
            }
            if (handle >= 0) {
                long version = WorldNatives.blockVersion(handle);
                if (version != cacheVersion) {
                    cache.clear();
                    lastCacheArray = null;
                    lastCacheKey = Long.MIN_VALUE;
                    cacheVersion = version;
                }
            }
            int[] section = key == lastCacheKey && lastCacheArray != null ? lastCacheArray : cache.get(key);
            if (section == null) {
                section = new int[4096];
                if (!WorldNatives.readSection(name, x >> 4, sy, z >> 4, true, section)) {
                    return WorldNatives.state(name, x, y, z, true);
                }
                if (cache.size() >= kMaxCachedSections) {
                    cache.clear();
                }
                cache.put(key, section);
            }
            lastCacheKey = key;
            lastCacheArray = section;
            return section[index];
        }

        void put(int x, int y, int z, int state) {
            if (y < minY || y >= maxY) {
                return;
            }
            long key = key(x >> 4, z >> 4, y >> 4);
            int[] changed = key == lastPendingKey ? lastPendingArray : pending.get(key);
            if (changed == null) {
                if (pending.size() >= kMaxPendingSections) {
                    flush();
                }
                changed = spare.isEmpty() ? new int[4096] : spare.remove(spare.size() - 1);
                Arrays.fill(changed, -1);
                pending.put(key, changed);
            }
            lastPendingKey = key;
            lastPendingArray = changed;
            changed[((y & 15) << 8) | ((z & 15) << 4) | (x & 15)] = state;
        }

        void flush() {
            lastPendingKey = Long.MIN_VALUE;
            lastPendingArray = null;
            if (pending.isEmpty()) {
                return;
            }
            long[] keys = new long[pending.size()];
            int count = 0;
            for (Long key : pending.keySet()) {
                keys[count++] = key;
            }
            Arrays.sort(keys);
            int[] chunks = new int[count * 3];
            int[] sections = new int[count];
            int[] states = new int[count * 4096];
            int chunkCount = 0;
            long lastChunk = Long.MIN_VALUE;
            for (int i = 0; i < count; i++) {
                long key = keys[i];
                long chunk = key >>> 12;
                int cx = (int) (key >> 38);
                int cz = ((int) ((key >> 12) & 0x3FFFFFF) << 6) >> 6;
                if (chunk != lastChunk) {
                    chunks[chunkCount * 3] = cx;
                    chunks[chunkCount * 3 + 1] = cz;
                    chunkCount++;
                    lastChunk = chunk;
                }
                chunks[(chunkCount - 1) * 3 + 2]++;
                sections[i] = (int) (key & 0xFFF) - 2048;
                int[] changed = pending.get(key);
                System.arraycopy(changed, 0, states, i * 4096, 4096);
                spare.add(changed);
            }
            pending.clear();
            if (kTrace) {
                Natives.log(1, "Blocks", "flushing " + count + " sections of " + name);
            }
            WorldNatives.applyChunks(name, Arrays.copyOf(chunks, chunkCount * 3), sections, states, true, null);
            cacheVersion = -2;
        }
    }
}
