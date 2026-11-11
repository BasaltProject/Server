// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix;

import java.nio.file.CopyOption;
import java.nio.file.LinkOption;
import java.nio.file.StandardCopyOption;

public record FileSystemCapabilities(boolean atomicMove, boolean hardLinks) {
    public CopyOption[] getMoveOptions() {
        if (atomicMove) {
            return new CopyOption[] {StandardCopyOption.ATOMIC_MOVE, LinkOption.NOFOLLOW_LINKS};
        }
        return new CopyOption[] {LinkOption.NOFOLLOW_LINKS};
    }
}
