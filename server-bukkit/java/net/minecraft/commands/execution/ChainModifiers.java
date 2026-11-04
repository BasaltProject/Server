// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution;
public record ChainModifiers(byte flags) {
    public static final ChainModifiers DEFAULT = new ChainModifiers((byte) 0);
    private static final byte kForked = 1;
    private static final byte kIsReturn = 2;
    private ChainModifiers setFlag(byte flag) {
        int merged = flags | flag;
        return merged != flags ? new ChainModifiers((byte) merged) : this;
    }
    public boolean isForked() {
        return (flags & kForked) != 0;
    }
    public ChainModifiers setForked() {
        return setFlag(kForked);
    }
    public boolean isReturn() {
        return (flags & kIsReturn) != 0;
    }
    public ChainModifiers setReturn() {
        return setFlag(kIsReturn);
    }
}
