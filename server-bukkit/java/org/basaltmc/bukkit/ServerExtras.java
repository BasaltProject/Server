package org.basaltmc.bukkit;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.function.Consumer;
import net.kyori.adventure.audience.Audience;
import net.kyori.adventure.text.Component;
import org.bukkit.GameMode;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.WorldBorder;
import org.bukkit.WorldCreator;
import org.bukkit.WorldType;
import org.bukkit.command.CommandSender;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.entity.SpawnCategory;
import org.bukkit.util.CachedServerIcon;

/** The server-wide settings and the world lifecycle, answered by the server rather than held here. */
public abstract class ServerExtras extends MiscServer {
    private static String setting(String key) {
        String value = WorldEnvNatives.setting(key);
        if (value == null) {
            throw new IllegalStateException("The server has no setting " + key);
        }
        return value;
    }

    private static int number(String key) {
        return Integer.parseInt(setting(key));
    }

    private static boolean flag(String key) {
        return Boolean.parseBoolean(setting(key));
    }

    private static List<String> list(String key) {
        String value = setting(key);
        return value.isEmpty() ? List.of() : List.of(value.split(","));
    }

    abstract BasaltWorld world(String name);

    @Override
    public GameMode getDefaultGameMode() {
        return GameMode.valueOf(setting("gamemode").toUpperCase(Locale.ROOT));
    }

    @Override
    public void setDefaultGameMode(GameMode mode) {
        WorldEnvNatives.setSetting("gamemode", mode.name().toLowerCase(Locale.ROOT));
    }

    @Override
    public boolean forcesDefaultGameMode() {
        return false;
    }

    @Override
    public boolean getOnlineMode() {
        return flag("online_mode");
    }

    @Override
    public int getMaxPlayers() {
        return number("max_players");
    }

    @Override
    public void setMaxPlayers(int maxPlayers) {
        WorldEnvNatives.setSetting("max_players", Integer.toString(maxPlayers));
    }

    @Override
    public int getPort() {
        return number("port");
    }

    @Override
    public String getIp() {
        String bind = setting("bind");
        return bind.equals("0.0.0.0") ? "" : bind;
    }

    @Override
    public String getMotd() {
        return setting("motd");
    }

    @Override
    public void setMotd(String motd) {
        WorldEnvNatives.setSetting("motd", motd);
    }

    @Override
    public void motd(Component motd) {
        WorldEnvNatives.setSetting("motd", Text.mini(motd));
    }

    @Override
    public Component shutdownMessage() {
        return Component.text("Server closed");
    }

    @Override
    public String getShutdownMessage() {
        return "Server closed";
    }

    @Override
    public int getViewDistance() {
        return number("view_distance");
    }

    @Override
    public int getSimulationDistance() {
        return number("simulation_distance");
    }

    @Override
    public int getIdleTimeout() {
        return number("idle_timeout");
    }

    @Override
    public void setIdleTimeout(int minutes) {
        if (!WorldEnvNatives.setSetting("idle_timeout", Integer.toString(minutes))) {
            throw new IllegalArgumentException("The idle timeout cannot be " + minutes);
        }
    }

    @Override
    public boolean getAllowNether() {
        return flag("allow_nether");
    }

    @Override
    public boolean getAllowEnd() {
        return flag("allow_end");
    }

    @Override
    public boolean isEnforcingSecureProfiles() {
        return flag("enforce_secure_profile");
    }

    @Override
    public boolean getHideOnlinePlayers() {
        return flag("hide_online_players");
    }

    @Override
    public boolean isAcceptingTransfers() {
        return false;
    }

    @Override
    public boolean isLoggingIPs() {
        return true;
    }

    @Override
    public boolean isStopping() {
        return WorldEnvNatives.stopping();
    }

    @Override
    public String getWorldType() {
        return setting("level_type");
    }

    @Override
    public boolean getGenerateStructures() {
        return true;
    }

    @Override
    public int getMaxWorldSize() {
        return 29999984;
    }

    @Override
    public int getMaxChainedNeighborUpdates() {
        return 1000000;
    }

    @Override
    public long getConnectionThrottle() {
        return -1L;
    }

    @Override
    public List<String> getInitialEnabledPacks() {
        return list("initial_enabled_packs");
    }

    @Override
    public List<String> getInitialDisabledPacks() {
        return list("initial_disabled_packs");
    }

    @Override
    public boolean suggestPlayerNamesWhenNullTabCompletions() {
        return true;
    }

    @Override
    public io.papermc.paper.configuration.ServerConfiguration getServerConfig() {
        return new io.papermc.paper.configuration.ServerConfiguration() {
            @Override
            public boolean isProxyOnlineMode() {
                return flag("online_mode");
            }

            @Override
            public boolean isProxyEnabled() {
                return false;
            }
        };
    }

    @Override
    public int getSpawnLimit(SpawnCategory category) {
        return Spawning.limit(category);
    }

    @Override
    public int getTicksPerSpawns(SpawnCategory category) {
        return Spawning.ticksPer(category);
    }

    @Override
    public boolean isOwnedByCurrentRegion(org.bukkit.World world, int chunkX, int chunkZ) {
        return isPrimaryThread();
    }

    @Override
    public boolean isOwnedByCurrentRegion(org.bukkit.World world, int chunkX, int chunkZ, int squareRadius) {
        return isPrimaryThread();
    }

    @Override
    public boolean isOwnedByCurrentRegion(org.bukkit.World world, io.papermc.paper.math.Position position) {
        return isPrimaryThread();
    }

    @Override
    public boolean isOwnedByCurrentRegion(org.bukkit.World world, io.papermc.paper.math.Position position,
        int squareRadiusChunks) {
        return isPrimaryThread();
    }

    @Override
    public boolean isOwnedByCurrentRegion(org.bukkit.Location location) {
        return isPrimaryThread();
    }

    @Override
    public boolean isOwnedByCurrentRegion(org.bukkit.Location location, int squareRadiusChunks) {
        return isPrimaryThread();
    }

    @Override
    public boolean isOwnedByCurrentRegion(org.bukkit.block.Block block) {
        return isPrimaryThread();
    }

    @Override
    public boolean isOwnedByCurrentRegion(Entity entity) {
        return isPrimaryThread();
    }

    @Override
    public io.papermc.paper.threadedregions.scheduler.GlobalRegionScheduler getGlobalRegionScheduler() {
        return new PaperSchedulers.Global();
    }

    @Override
    public io.papermc.paper.threadedregions.scheduler.RegionScheduler getRegionScheduler() {
        return new PaperSchedulers.Region();
    }

    @Override
    public io.papermc.paper.threadedregions.scheduler.AsyncScheduler getAsyncScheduler() {
        return new PaperSchedulers.Async();
    }

    @Override
    public WorldBorder createWorldBorder() {
        return new StandaloneBorder();
    }

    @Override
    public CachedServerIcon loadServerIcon(File file) throws Exception {
        return ServerIcon.load(file);
    }

    @Override
    public <T extends org.bukkit.Keyed> org.bukkit.Registry<T> getRegistry(Class<T> type) {
        return io.papermc.paper.registry.RegistryAccess.registryAccess().getRegistry(type);
    }

    @Override
    public org.bukkit.Server.Spigot spigot() {
        return new org.bukkit.Server.Spigot() {
            @Override
            public void restart() {
                ServerExtras.this.restart();
            }
        };
    }

    @Override
    public void restart() {
        Natives.runConsoleCommand("restart");
    }

    @Override
    public Iterable<? extends Audience> audiences() {
        List<Audience> out = new ArrayList<>(getOnlinePlayers());
        out.add(getConsoleSender());
        return out;
    }

    @Override
    public CommandSender createCommandSender(Consumer<? super Component> feedback) {
        return CallbackSender.of(getConsoleSender(), feedback);
    }

    @Override
    public World createWorld(WorldCreator creator) {
        String name = creator.name();
        World known = getWorld(name);
        if (known != null) {
            return known;
        }
        if (name.isEmpty() || !name.matches("[a-zA-Z0-9_-]{1,64}")) {
            throw new IllegalArgumentException("A world name is letters, digits, underscore or dash, up to 64: " + name);
        }
        String dimension = switch (creator.environment()) {
            case NETHER -> "nether";
            case THE_END -> "end";
            default -> "overworld";
        };
        WorldType type = creator.type();
        String kind = type == WorldType.FLAT ? "flat" : type == WorldType.AMPLIFIED ? "amplified"
            : type == WorldType.LARGE_BIOMES ? "large_biomes" : type == WorldType.DEBUG_ALL_BLOCK_STATES ? "debug"
            : "normal";
        if (creator.generator() != null) {
            kind = "void";
        }
        if (!WorldEnvNatives.createWorld(name, dimension, kind, creator.seed(), true)) {
            return null;
        }
        BasaltWorld made = world(name);
        if (creator.generator() != null) {
            org.basaltmc.bukkit.worldgen.CustomGenerators.install(made, creator.generator(), creator.biomeProvider());
            Location spawn = creator.generator().getFixedSpawnLocation(made, new java.util.Random(creator.seed()));
            if (spawn != null) {
                made.setSpawnLocation(spawn);
            } else {
                made.setSpawnLocation(0, made.getHighestBlockYAt(0, 0) + 1, 0);
            }
        }
        getPluginManager().callEvent(new org.bukkit.event.world.WorldInitEvent(made));
        return made;
    }

    @Override
    public org.bukkit.generator.ChunkGenerator.ChunkData createChunkData(World world) {
        return org.basaltmc.bukkit.worldgen.CustomGenerators.newChunkData(world);
    }

    @Override
    public boolean unloadWorld(String name, boolean save) {
        World world = getWorld(name);
        return world != null && unloadWorld(world, save);
    }

    @Override
    public boolean unloadWorld(World world, boolean save) {
        if (world == null || getWorld(world.getName()) == null) {
            return false;
        }
        org.bukkit.event.world.WorldUnloadEvent event = new org.bukkit.event.world.WorldUnloadEvent(world);
        getPluginManager().callEvent(event);
        return !event.isCancelled() && WorldEnvNatives.unloadWorld(world.getName(), save);
    }

    @Override
    public long[] getTickTimes() {
        throw Unsupported.hit("Server#getTickTimes");
    }
}
