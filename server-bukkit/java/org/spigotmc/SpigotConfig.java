package org.spigotmc;

import org.bukkit.configuration.file.YamlConfiguration;

/** Plugins read and write Spigot's settings tree; Basalt keeps its own, so this one only stores what they put. */
public class SpigotConfig {
    public static final YamlConfiguration config = new YamlConfiguration();

    private SpigotConfig() {}
}
