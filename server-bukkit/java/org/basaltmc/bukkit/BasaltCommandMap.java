package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import org.bukkit.Server;
import org.bukkit.command.Command;
import org.bukkit.command.PluginIdentifiableCommand;
import org.bukkit.command.SimpleCommandMap;
import org.bukkit.plugin.Plugin;

public final class BasaltCommandMap extends SimpleCommandMap {
    private final Set<String> announced = new HashSet<>();

    BasaltCommandMap(Server server) {
        super(server, new HashMap<>());
    }

    @Override
    public boolean register(String label, String fallbackPrefix, Command command) {
        boolean registered = super.register(label, fallbackPrefix, command);
        if (command instanceof PluginIdentifiableCommand owned) {
            announce(owned.getPlugin().getName(), label, command);
            for (String alias : command.getAliases()) {
                announce(owned.getPlugin().getName(), alias, command);
            }
        }
        return registered;
    }

    private void announce(String owner, String label, Command command) {
        String name = label.toLowerCase(Locale.ROOT).trim();
        if (!announced.add(owner + "/" + name)) {
            return;
        }
        Natives.registerCommand(owner, name, basaltPermission(command));
    }

    /** Empty opens the command to everyone and leaves the check to Bukkit's own permission test. */
    private String basaltPermission(Command command) {
        String permission = command.getPermission();
        if (permission == null || permission.isEmpty()) {
            return "";
        }
        org.bukkit.permissions.Permission known = BasaltServer.instance().pluginManagerImpl().getPermission(permission);
        if (known != null && known.getDefault().getValue(false)) {
            return "";
        }
        return permission;
    }

    void unregisterPlugin(Plugin plugin) {
        for (String key : new ArrayList<>(knownCommands.keySet())) {
            Command command = knownCommands.get(key);
            if (command instanceof PluginIdentifiableCommand owned && owned.getPlugin() == plugin) {
                knownCommands.remove(key);
            }
        }
        announced.removeIf(entry -> entry.startsWith(plugin.getName() + "/"));
        Natives.unregisterCommands(plugin.getName());
    }
}
