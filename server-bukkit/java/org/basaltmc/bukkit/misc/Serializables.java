package org.basaltmc.bukkit.misc;

import org.bukkit.configuration.serialization.ConfigurationSerialization;

public final class Serializables {
    private Serializables() {}

    public static void register() {
        ConfigurationSerialization.registerClass(BasaltOfflinePlayer.class);
        ConfigurationSerialization.registerClass(BasaltPlayerProfile.class);
    }
}
