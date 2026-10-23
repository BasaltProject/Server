package org.basaltmc.bukkit;

import java.io.File;
import java.nio.file.Files;
import java.util.Base64;
import org.bukkit.util.CachedServerIcon;

/** A 64 by 64 PNG kept as the base64 text a server list shows. */
final class ServerIcon implements CachedServerIcon {
    private final String data;

    private ServerIcon(String data) {
        this.data = data;
    }

    static CachedServerIcon load(File file) throws Exception {
        byte[] bytes = Files.readAllBytes(file.toPath());
        boolean png = bytes.length > 24 && bytes[0] == (byte) 0x89 && bytes[1] == 'P' && bytes[2] == 'N'
            && bytes[3] == 'G';
        int width = png ? ((bytes[16] & 255) << 24) | ((bytes[17] & 255) << 16) | ((bytes[18] & 255) << 8)
            | (bytes[19] & 255) : -1;
        int height = png ? ((bytes[20] & 255) << 24) | ((bytes[21] & 255) << 16) | ((bytes[22] & 255) << 8)
            | (bytes[23] & 255) : -1;
        if (!png || width != 64 || height != 64) {
            throw new IllegalArgumentException("Must be a 64 by 64 PNG");
        }
        return new ServerIcon("data:image/png;base64," + Base64.getEncoder().encodeToString(bytes));
    }

    static CachedServerIcon of(byte[] png) {
        return new ServerIcon("data:image/png;base64," + Base64.getEncoder().encodeToString(png));
    }

    @Override
    public String getData() {
        return data;
    }
}
