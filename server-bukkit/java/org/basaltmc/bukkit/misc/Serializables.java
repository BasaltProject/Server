package org.basaltmc.bukkit.misc;

import org.bukkit.configuration.serialization.ConfigurationSerialization;

/** The server's own serializable classes, known to the YAML loader under the names plugins have always stored. */
public final class Serializables {
    private Serializables() {}

    public static void register() {
        ConfigurationSerialization.registerClass(BasaltOfflinePlayer.class);
        ConfigurationSerialization.registerClass(BasaltPlayerProfile.class);
    }
}
