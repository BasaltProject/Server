// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Properties;

public class Eula {
    private final boolean agreed;

    public Eula(Path file) {
        this.agreed = read(file);
    }

    private static boolean read(Path file) {
        try (var in = Files.newInputStream(file)) {
            Properties values = new Properties();
            values.load(in);
            return Boolean.parseBoolean(values.getProperty("eula", "false").trim());
        } catch (IOException | RuntimeException missing) {
            return false;
        }
    }

    public boolean hasAgreedToEULA() {
        return agreed;
    }
}
