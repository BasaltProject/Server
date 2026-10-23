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
import org.bukkit.scheduler.BukkitScheduler;
import org.basaltmc.bukkit.gen.ServerBase;

public class BasaltServer extends ServerBase {
    public static final String kMinecraftVersion = "26.3";

    private static BasaltServer instance;

    private final Logger logger = Logger.getLogger("Minecraft");
    private volatile boolean worldsReady;
    private final File root;
    private final BasaltPluginManager pluginManager;
    private final BasaltScheduler scheduler = new BasaltScheduler();
    private final ServicesManager services = new SimpleServicesManager();
    private final BasaltMessenger messenger = new BasaltMessenger();
    private final BasaltCommandMap commandMap;
    private final BasaltConsole console = new BasaltConsole();
    private final Map<UUID, BasaltPlayer> players = new LinkedHashMap<>();
    private final Map<String, BasaltWorld> worlds = new LinkedHashMap<>();
    private final Thread primaryThread = Thread.currentThread();

    protected BasaltServer(File root, File pluginsFolder) {
        instance = this;
        org.basaltmc.bukkit.item.ItemMetas.register();
        this.root = root;
        this.pluginManager = new BasaltPluginManager(this, pluginsFolder);
        this.commandMap = new BasaltCommandMap(this);
        this.pluginManager.commandMap(commandMap);
    }

    public static BasaltServer instance() {
        return instance;
    }

    public void tickTask(Runnable task) {
        scheduler.runTask(null, task);
    }

    BasaltPluginManager pluginManagerImpl() {
        return pluginManager;
    }

    BasaltMessenger messengerImpl() {
        return messenger;
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
        return players.computeIfAbsent(id, key -> new org.bukkit.craftbukkit.entity.CraftPlayer(key, name));
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

    public org.bukkit.entity.Entity entityById(BasaltWorld world, int id) {
        for (org.bukkit.entity.Player player : getOnlinePlayers()) {
            if (player.getEntityId() == id) {
                return player;
            }
        }
        return world.entity(id);
    }

    synchronized BasaltWorld world(String name) {
        return worlds.computeIfAbsent(name, BasaltWorld::new);
    }

    @Override
    public boolean isOwnedByCurrentRegion(org.bukkit.entity.Entity entity) {
        return isPrimaryThread();
    }

    @Override
    public org.bukkit.entity.EntityFactory getEntityFactory() {
        return org.basaltmc.bukkit.entity.BasaltEntityFactory.instance;
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

    /** Plugins that load at STARTUP see no worlds, as on any Bukkit server, until the worlds are announced. */
    void announceWorlds() {
        worldsReady = true;
        for (World world : getWorlds()) {
            getPluginManager().callEvent(new org.bukkit.event.world.WorldInitEvent(world));
            getPluginManager().callEvent(new org.bukkit.event.world.WorldLoadEvent(world));
        }
    }

    @Override
    public List<World> getWorlds() {
        List<World> all = new ArrayList<>();
        if (!worldsReady) {
            return all;
        }
        for (String name : Natives.worlds()) {
            all.add(world(name));
        }
        return all;
    }

    @Override
    public World getWorld(String name) {
        if (!worldsReady) {
            return null;
        }
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
    public org.bukkit.block.data.BlockData createBlockData(org.bukkit.Material material) {
        int state = Registries.defaultStateOf(material);
        if (state < 0) {
            throw new IllegalArgumentException(material + " is not a block");
        }
        return BasaltBlockData.of(state);
    }

    @Override
    public org.bukkit.block.data.BlockData createBlockData(org.bukkit.Material material,
            java.util.function.Consumer<? super org.bukkit.block.data.BlockData> consumer) {
        org.bukkit.block.data.BlockData data = createBlockData(material);
        if (consumer != null) {
            consumer.accept(data);
        }
        return data;
    }

    @Override
    public org.bukkit.block.data.BlockData createBlockData(String data) {
        org.bukkit.block.data.BlockData parsed = BasaltBlockData.parse(data);
        if (parsed == null) {
            throw new IllegalArgumentException("Could not parse data: " + data);
        }
        return parsed;
    }

    @Override
    public org.bukkit.block.data.BlockData createBlockData(org.bukkit.Material material, String data) {
        if (data == null || data.isEmpty()) {
            return createBlockData(material);
        }
        return createBlockData(data.startsWith("[") ? material.getKey() + data : data);
    }

    @Override
    @SuppressWarnings({"unchecked", "rawtypes"})
    public <T extends org.bukkit.Keyed> org.bukkit.Tag<T> getTag(String registry, org.bukkit.NamespacedKey tag,
            Class<T> type) {
        return (org.bukkit.Tag<T>) BasaltTag.all(registry, (Class) type).get(tag);
    }

    @Override
    @SuppressWarnings({"unchecked", "rawtypes"})
    public <T extends org.bukkit.Keyed> Iterable<org.bukkit.Tag<T>> getTags(String registry, Class<T> type) {
        return (Iterable) java.util.List.copyOf(BasaltTag.all(registry, (Class) type).values());
    }

    @Override
    public org.bukkit.entity.Entity getEntity(UUID id) {
        for (World world : getWorlds()) {
            org.bukkit.entity.Entity found = world.getEntity(id);
            if (found != null) {
                return found;
            }
        }
        return null;
    }

    @Override
    public io.papermc.paper.datapack.DatapackManager getDatapackManager() {
        return (io.papermc.paper.datapack.DatapackManager) java.lang.reflect.Proxy.newProxyInstance(
            getClass().getClassLoader(), new Class<?>[] {io.papermc.paper.datapack.DatapackManager.class},
            (proxy, method, args) -> {
                Class<?> type = method.getReturnType();
                if (java.util.Collection.class.isAssignableFrom(type)) {
                    return type == java.util.Set.class ? java.util.Set.of() : java.util.List.of();
                }
                if (type == boolean.class) {
                    return false;
                }
                return null;
            });
    }

    @Override
    public boolean isTickingWorlds() {
        return true;
    }

    @Override
    public boolean isPrimaryThread() {
        return Thread.currentThread() == primaryThread || Natives.onPluginThread();
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

    @Override
    public org.bukkit.inventory.ItemFactory getItemFactory() {
        return BasaltItemFactory.INSTANCE;
    }

    @Override
    public org.bukkit.inventory.Inventory createInventory(org.bukkit.inventory.InventoryHolder holder,
            org.bukkit.event.inventory.InventoryType type) {
        return org.basaltmc.bukkit.inv.Creation.of(holder, type, (Component) null);
    }

    @Override
    public org.bukkit.inventory.Inventory createInventory(org.bukkit.inventory.InventoryHolder holder,
            org.bukkit.event.inventory.InventoryType type, Component title) {
        return org.basaltmc.bukkit.inv.Creation.of(holder, type, title);
    }

    @Override
    public org.bukkit.inventory.Inventory createInventory(org.bukkit.inventory.InventoryHolder holder,
            org.bukkit.event.inventory.InventoryType type, String title) {
        return org.basaltmc.bukkit.inv.Creation.of(holder, type, title);
    }

    @Override
    public org.bukkit.inventory.Inventory createInventory(org.bukkit.inventory.InventoryHolder holder, int size) {
        return org.basaltmc.bukkit.inv.Creation.of(holder, size, (Component) null);
    }

    @Override
    public org.bukkit.inventory.Inventory createInventory(org.bukkit.inventory.InventoryHolder holder, int size,
            Component title) {
        return org.basaltmc.bukkit.inv.Creation.of(holder, size, title);
    }

    @Override
    public org.bukkit.inventory.Inventory createInventory(org.bukkit.inventory.InventoryHolder holder, int size,
            String title) {
        return org.basaltmc.bukkit.inv.Creation.of(holder, size, title);
    }

    @Override
    public boolean addRecipe(org.bukkit.inventory.Recipe recipe) {
        return org.basaltmc.bukkit.recipe.Recipes.add(recipe);
    }

    @Override
    public boolean addRecipe(org.bukkit.inventory.Recipe recipe, boolean resendRecipes) {
        return org.basaltmc.bukkit.recipe.Recipes.add(recipe);
    }

    @Override
    public boolean addRecipes(Iterable<org.bukkit.inventory.Recipe> recipes) {
        return org.basaltmc.bukkit.recipe.Recipes.addAll(recipes);
    }

    @Override
    public org.bukkit.inventory.Recipe getRecipe(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.recipe.Recipes.get(key);
    }

    @Override
    public boolean hasRecipe(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.recipe.Recipes.has(key);
    }

    @Override
    public boolean removeRecipe(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.recipe.Recipes.remove(key);
    }

    @Override
    public boolean removeRecipe(org.bukkit.NamespacedKey key, boolean resendRecipes) {
        return org.basaltmc.bukkit.recipe.Recipes.remove(key);
    }

    @Override
    public boolean removeRecipes(Iterable<org.bukkit.NamespacedKey> keys) {
        return org.basaltmc.bukkit.recipe.Recipes.removeAll(keys);
    }

    @Override
    public java.util.Iterator<org.bukkit.inventory.Recipe> recipeIterator() {
        return org.basaltmc.bukkit.recipe.Recipes.iterator();
    }

    @Override
    public java.util.List<org.bukkit.inventory.Recipe> getRecipesFor(org.bukkit.inventory.ItemStack result) {
        return org.basaltmc.bukkit.recipe.Recipes.producing(result);
    }

    @Override
    public void clearRecipes() {
        org.basaltmc.bukkit.recipe.Recipes.clear();
    }

    @Override
    public void resetRecipes() {
        org.basaltmc.bukkit.recipe.Recipes.reset();
    }

    @Override
    public org.bukkit.inventory.Merchant createMerchant() {
        return new org.basaltmc.bukkit.inv.BasaltMerchant(null);
    }

    @Override
    public org.bukkit.inventory.Merchant createMerchant(Component title) {
        return new org.basaltmc.bukkit.inv.BasaltMerchant(title);
    }

    @Override
    public org.bukkit.inventory.Merchant createMerchant(String title) {
        return new org.basaltmc.bukkit.inv.BasaltMerchant(title == null ? null
            : org.basaltmc.bukkit.item.Texts.fromLegacy(title));
    }

    @Override
    public org.bukkit.inventory.Recipe getCraftingRecipe(org.bukkit.inventory.ItemStack[] matrix, World world) {
        org.basaltmc.bukkit.recipe.Recipes.Crafted crafted = org.basaltmc.bukkit.recipe.Recipes.craft(matrix);
        return crafted == null ? null : org.basaltmc.bukkit.recipe.Recipes.get(crafted.recipe());
    }

    @Override
    public org.bukkit.inventory.ItemStack craftItem(org.bukkit.inventory.ItemStack[] matrix, World world, Player player) {
        return craftItemResult(matrix, world, player).getResult();
    }

    @Override
    public org.bukkit.inventory.ItemStack craftItem(org.bukkit.inventory.ItemStack[] matrix, World world) {
        return craftItemResult(matrix, world, null).getResult();
    }

    @Override
    public org.bukkit.inventory.ItemCraftResult craftItemResult(org.bukkit.inventory.ItemStack[] matrix, World world) {
        return craftItemResult(matrix, world, null);
    }

    @Override
    public org.bukkit.inventory.ItemCraftResult craftItemResult(org.bukkit.inventory.ItemStack[] matrix, World world,
            Player player) {
        org.basaltmc.bukkit.recipe.Recipes.Crafted crafted = org.basaltmc.bukkit.recipe.Recipes.craft(matrix);
        if (crafted == null) {
            return new org.basaltmc.bukkit.recipe.CraftResult(new BasaltItemStack(org.bukkit.Material.AIR, 0), matrix);
        }
        org.bukkit.inventory.ItemStack[] left = new org.bukkit.inventory.ItemStack[matrix.length];
        for (int at = 0; at < left.length; at++) {
            org.bukkit.inventory.ItemStack used = matrix[at];
            if (used != null && used.getAmount() > 1) {
                left[at] = used.asQuantity(used.getAmount() - 1);
            } else {
                left[at] = crafted.remaining()[at];
            }
        }
        return new org.basaltmc.bukkit.recipe.CraftResult(crafted.result(), left);
    }
}
