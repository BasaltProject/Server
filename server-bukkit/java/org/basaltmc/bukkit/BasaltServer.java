package org.basaltmc.bukkit;

import java.io.File;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.logging.Logger;
import net.kyori.adventure.text.Component;
import org.bukkit.GameMode;
import org.bukkit.Warning;
import org.bukkit.World;
import org.bukkit.command.CommandMap;
import org.bukkit.command.CommandSender;
import org.bukkit.command.ConsoleCommandSender;
import org.bukkit.command.PluginCommand;
import org.bukkit.entity.Player;
import org.bukkit.plugin.PluginManager;
import org.bukkit.plugin.ServicesManager;
import org.bukkit.plugin.SimpleServicesManager;
import org.bukkit.plugin.messaging.Messenger;
import org.bukkit.plugin.messaging.StandardMessenger;
import org.bukkit.scheduler.BukkitScheduler;
import org.basaltmc.bukkit.gen.ServerBase;

public final class BasaltServer extends ServerBase {
    public static final String kMinecraftVersion = "26.3";

    private static BasaltServer instance;

    private final Logger logger = Logger.getLogger("Minecraft");
    private final File root;
    private final BasaltPluginManager pluginManager;
    private final BasaltScheduler scheduler = new BasaltScheduler();
    private final ServicesManager services = new SimpleServicesManager();
    private final Messenger messenger = new StandardMessenger();
    private final BasaltCommandMap commandMap;
    private final BasaltConsole console = new BasaltConsole();
    private final Map<UUID, BasaltPlayer> players = new LinkedHashMap<>();
    private final Map<String, BasaltWorld> worlds = new LinkedHashMap<>();
    private final Thread primaryThread = Thread.currentThread();

    BasaltServer(File root, File pluginsFolder) {
        instance = this;
        this.root = root;
        this.pluginManager = new BasaltPluginManager(this, pluginsFolder);
        this.commandMap = new BasaltCommandMap(this);
    }

    public static BasaltServer instance() {
        return instance;
    }

    BasaltPluginManager pluginManagerImpl() {
        return pluginManager;
    }

    BasaltScheduler schedulerImpl() {
        return scheduler;
    }

    BasaltCommandMap commandMapImpl() {
        return commandMap;
    }

    BasaltConsole consoleImpl() {
        return console;
    }

    synchronized BasaltPlayer player(UUID id, String name) {
        return players.computeIfAbsent(id, key -> new BasaltPlayer(key, name));
    }

    synchronized void forget(UUID id) {
        players.remove(id);
    }

    synchronized BasaltPlayer playerByName(String name) {
        String[] flat = Natives.onlinePlayers();
        for (int i = 0; i + 1 < flat.length; i += 2) {
            if (flat[i + 1].equalsIgnoreCase(name)) {
                return player(UUID.fromString(flat[i]), flat[i + 1]);
            }
        }
        return null;
    }

    synchronized BasaltWorld world(String name) {
        return worlds.computeIfAbsent(name, BasaltWorld::new);
    }

    @Override
    public String getName() {
        return "Basalt";
    }

    @Override
    public String getVersion() {
        return "Basalt (MC: " + kMinecraftVersion + ")";
    }

    @Override
    public String getBukkitVersion() {
        return kMinecraftVersion + "-R0.1-SNAPSHOT";
    }

    @Override
    public String getMinecraftVersion() {
        return kMinecraftVersion;
    }

    @Override
    public Logger getLogger() {
        return logger;
    }

    @Override
    public PluginManager getPluginManager() {
        return pluginManager;
    }

    @Override
    public BukkitScheduler getScheduler() {
        return scheduler;
    }

    @Override
    public ServicesManager getServicesManager() {
        return services;
    }

    @Override
    public Messenger getMessenger() {
        return messenger;
    }

    @Override
    public CommandMap getCommandMap() {
        return commandMap;
    }

    @Override
    public ConsoleCommandSender getConsoleSender() {
        return console;
    }

    @Override
    public PluginCommand getPluginCommand(String name) {
        org.bukkit.command.Command command = commandMap.getCommand(name);
        return command instanceof PluginCommand pluginCommand ? pluginCommand : null;
    }

    @Override
    public boolean dispatchCommand(CommandSender sender, String commandLine) {
        return commandMap.dispatch(sender, commandLine);
    }

    @Override
    public Collection<? extends Player> getOnlinePlayers() {
        String[] flat = Natives.onlinePlayers();
        List<Player> online = new ArrayList<>();
        for (int i = 0; i + 1 < flat.length; i += 2) {
            online.add(player(UUID.fromString(flat[i]), flat[i + 1]));
        }
        return Collections.unmodifiableList(online);
    }

    @Override
    public Player getPlayer(String name) {
        Player best = null;
        for (Player candidate : getOnlinePlayers()) {
            String lowered = candidate.getName().toLowerCase(Locale.ROOT);
            if (lowered.equals(name.toLowerCase(Locale.ROOT))) {
                return candidate;
            }
            if (best == null && lowered.startsWith(name.toLowerCase(Locale.ROOT))) {
                best = candidate;
            }
        }
        return best;
    }

    @Override
    public Player getPlayer(UUID id) {
        for (Player candidate : getOnlinePlayers()) {
            if (candidate.getUniqueId().equals(id)) {
                return candidate;
            }
        }
        return null;
    }

    @Override
    public Player getPlayerExact(String name) {
        return playerByName(name);
    }

    @Override
    public List<Player> matchPlayer(String name) {
        List<Player> matches = new ArrayList<>();
        for (Player candidate : getOnlinePlayers()) {
            if (candidate.getName().toLowerCase(Locale.ROOT).startsWith(name.toLowerCase(Locale.ROOT))) {
                matches.add(candidate);
            }
        }
        return matches;
    }

    @Override
    public List<World> getWorlds() {
        List<World> all = new ArrayList<>();
        for (String name : Natives.worlds()) {
            all.add(world(name));
        }
        return all;
    }

    @Override
    public World getWorld(String name) {
        for (String known : Natives.worlds()) {
            if (known.equalsIgnoreCase(name)) {
                return world(known);
            }
        }
        return null;
    }

    @Override
    public World getWorld(UUID id) {
        for (World world : getWorlds()) {
            if (world.getUID().equals(id)) {
                return world;
            }
        }
        return null;
    }

    @Override
    public int broadcast(Component message, String permission) {
        String mini = Text.mini(message);
        int reached = 0;
        for (Player player : getOnlinePlayers()) {
            if (permission == null || player.hasPermission(permission)) {
                Natives.playerSend(player.getUniqueId().toString(), mini);
                reached++;
            }
        }
        Natives.log(1, "Server", Text.plain(message));
        return reached;
    }

    @Override
    public int broadcastMessage(String message) {
        return broadcast(net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer.legacySection()
            .deserialize(message), null);
    }

    @Override
    public boolean isPrimaryThread() {
        return Thread.currentThread() == primaryThread || Thread.currentThread().getName().equals("main");
    }

    @Override
    public int getCurrentTick() {
        return (int) Natives.currentTick();
    }

    @Override
    public File getWorldContainer() {
        return new File(root, "worlds");
    }

    @Override
    public File getPluginsFolder() {
        return pluginManager.folder();
    }

    @Override
    public String getUpdateFolder() {
        return "update";
    }

    @Override
    public File getUpdateFolderFile() {
        return new File(pluginManager.folder(), "update");
    }

    @Override
    public Path getLevelDirectory() {
        return getWorldContainer().toPath();
    }

    @Override
    public GameMode getDefaultGameMode() {
        return GameMode.SURVIVAL;
    }

    @Override
    public boolean getOnlineMode() {
        return true;
    }

    @Override
    public int getMaxPlayers() {
        return 20;
    }

    @Override
    public int getPort() {
        return 25565;
    }

    @Override
    public String getIp() {
        return "";
    }

    @Override
    public String getMotd() {
        return "A Basalt Server";
    }

    @Override
    public int getViewDistance() {
        return 10;
    }

    @Override
    public int getSimulationDistance() {
        return 10;
    }

    @Override
    public boolean getAllowNether() {
        return true;
    }

    @Override
    public boolean getAllowEnd() {
        return true;
    }

    @Override
    public boolean getAllowFlight() {
        return false;
    }

    @Override
    public boolean hasWhitelist() {
        return false;
    }

    @Override
    public boolean isHardcore() {
        return false;
    }

    @Override
    public boolean isStopping() {
        return false;
    }

    @Override
    public boolean isGlobalTickThread() {
        return isPrimaryThread();
    }

    @Override
    public double[] getTPS() {
        return new double[] {20.0, 20.0, 20.0};
    }

    @Override
    public Warning.WarningState getWarningState() {
        return Warning.WarningState.DEFAULT;
    }

    @Override
    public Set<String> getListeningPluginChannels() {
        return messenger.getIncomingChannels();
    }

    @Override
    public Set<org.bukkit.OfflinePlayer> getOperators() {
        return Set.of();
    }

    @Override
    public Map<String, String[]> getCommandAliases() {
        return Map.of();
    }

    @Override
    public org.bukkit.help.HelpMap getHelpMap() {
        return new BasaltHelpMap();
    }

    @Override
    public org.bukkit.UnsafeValues getUnsafe() {
        return BasaltUnsafe.INSTANCE;
    }

    @Override
    public void shutdown() {
        Natives.runConsoleCommand("stop");
    }

    @Override
    public void reload() {
        Natives.runConsoleCommand("reload");
    }

    @Override
    public org.bukkit.OfflinePlayer getOfflinePlayer(UUID id) {
        Player online = getPlayer(id);
        if (online != null) {
            return online;
        }
        throw Unsupported.hit("Server#getOfflinePlayer(UUID) for a player who is not online");
    }

    @Override
    public org.bukkit.OfflinePlayer getOfflinePlayer(String name) {
        Player online = getPlayerExact(name);
        if (online != null) {
            return online;
        }
        throw Unsupported.hit("Server#getOfflinePlayer(String) for a player who is not online");
    }
}
