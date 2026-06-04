package org.bukkit.plugin.java;

/** Lives in the package of JavaPlugin because the switch it needs is protected there. */
public final class PluginSwitch {
    private PluginSwitch() {}

    public static void enable(JavaPlugin plugin) {
        plugin.setEnabled(true);
    }

    public static void disable(JavaPlugin plugin) {
        plugin.setEnabled(false);
    }
}
