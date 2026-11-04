// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

public class NbtAccounter {
    public static final int DEFAULT_NBT_QUOTA = 2097152;
    public static final int UNCOMPRESSED_NBT_QUOTA = 104857600;
    private final long quota;
    private final int maxDepth;
    private long usage;
    private int depth;

    public NbtAccounter(long quota, int maxDepth) {
        this.quota = quota;
        this.maxDepth = maxDepth;
    }

    public static NbtAccounter create(long quota) {
        return new NbtAccounter(quota, Tag.MAX_DEPTH);
    }

    public static NbtAccounter defaultQuota() {
        return new NbtAccounter(DEFAULT_NBT_QUOTA, Tag.MAX_DEPTH);
    }

    public static NbtAccounter uncompressedQuota() {
        return new NbtAccounter(UNCOMPRESSED_NBT_QUOTA, Tag.MAX_DEPTH);
    }

    public static NbtAccounter unlimitedHeap() {
        return new NbtAccounter(Long.MAX_VALUE, Tag.MAX_DEPTH);
    }

    public void accountBytes(long bytesPerEntry, long count) {
        accountBytes(bytesPerEntry * count);
    }

    public void accountBytes(long size) {
        if (usage + size > quota) {
            throw new NbtAccounterException(
                "Tried to read NBT tag that was too big; tried to allocate: " + (usage + size) + "bytes where max allowed: " + quota);
        }
        usage += size;
    }

    public void pushDepth() {
        if (depth >= maxDepth) {
            throw new NbtAccounterException("Tried to read NBT tag with too high complexity, depth > " + maxDepth);
        }
        depth++;
    }

    public void popDepth() {
        if (depth <= 0) {
            throw new NbtAccounterException("NBT-Accounter tried to pop stack-depth at top-level");
        }
        depth--;
    }

    public long getUsage() {
        return usage;
    }

    public int getDepth() {
        return depth;
    }
}
