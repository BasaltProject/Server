// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

public class MemoryReserve {
    private static final int kReserveBytes = 10485760;
    private static byte[] reserve;

    public static void allocate() {
        reserve = new byte[kReserveBytes];
    }

    public static void release() {
        if (reserve == null) {
            return;
        }
        reserve = null;
        for (int i = 0; i < 3; i++) {
            System.gc();
        }
    }
}
