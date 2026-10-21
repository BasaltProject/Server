package org.basaltmc.bukkit.worldedit;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import org.enginehub.linbus.stream.LinBinaryIO;
import org.enginehub.linbus.tree.LinCompoundTag;
import org.enginehub.linbus.tree.LinRootEntry;

/** NBT as the server's native services exchange it (a named root compound) and as WorldEdit holds it. */
public final class TileNbt {
    private TileNbt() {}

    public static LinCompoundTag read(byte[] bytes) {
        if (bytes == null) {
            return null;
        }
        try {
            return LinBinaryIO.readUsing(new DataInputStream(new ByteArrayInputStream(bytes)), LinRootEntry::readFrom)
                .value();
        } catch (IOException e) {
            return null;
        }
    }

    public static byte[] write(LinCompoundTag tag) {
        return new LinRootEntry("", tag).writeToArray();
    }
}
