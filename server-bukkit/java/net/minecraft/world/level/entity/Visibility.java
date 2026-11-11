// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.entity;
import net.minecraft.server.level.FullChunkStatus;
public enum Visibility {
    HIDDEN, TRACKED, TICKING;
    private boolean accessibleFlag() {
        return this != HIDDEN;
    }
    public boolean isTicking() {
        return this == TICKING;
    }
    public boolean isAccessible() {
        return accessibleFlag();
    }
    public static Visibility fromFullChunkStatus(FullChunkStatus status) {
        if (status.isOrAfter(net.minecraft.server.level.FullChunkStatus.ENTITY_TICKING)) {
            return TICKING;
        }
        return status.isOrAfter(net.minecraft.server.level.FullChunkStatus.FULL) ? TRACKED : HIDDEN;
    }
}
