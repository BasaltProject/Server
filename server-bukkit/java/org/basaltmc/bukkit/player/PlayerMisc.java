package org.basaltmc.bukkit.player;

import com.destroystokyo.paper.ClientOption;
import java.net.InetSocketAddress;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.resource.ResourcePackInfo;
import net.kyori.adventure.resource.ResourcePackRequest;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.BasaltServer;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.Text;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Statistic;
import org.bukkit.World;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.Player;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.MainHand;
import org.bukkit.plugin.Plugin;

public abstract class PlayerMisc extends PlayerView {
    private UUID key() {
        return getUniqueId();
    }

    private static String itemKey(Material material) {
        return material.getKey().toString();
    }

    @Override
    public boolean hasCooldown(Material material) {
        return getCooldown(material) > 0;
    }

    @Override
    public boolean hasCooldown(ItemStack item) {
        return getCooldown(item) > 0;
    }

    @Override
    public int getCooldown(Material material) {
        return (int) PlayerCalls.count(key(), "cooldown", itemKey(material));
    }

    @Override
    public int getCooldown(ItemStack item) {
        return (int) PlayerCalls.count(key(), "cooldown", itemKey(item.getType()));
    }

    @Override
    public int getCooldown(Key key) {
        return (int) PlayerCalls.count(key(), "cooldown", key.asString());
    }

    @Override
    public void setCooldown(Material material, int ticks) {
        PlayerCalls.call(key(), "set_cooldown", itemKey(material), (long) ticks);
    }

    @Override
    public void setCooldown(ItemStack item, int ticks) {
        PlayerCalls.call(key(), "set_cooldown", itemKey(item.getType()), (long) ticks);
    }

    @Override
    public void setCooldown(Key key, int ticks) {
        PlayerCalls.call(key(), "set_cooldown", key.asString(), (long) ticks);
    }

    private int stat(String key) {
        return (int) PlayerCalls.count(key(), "statistic", key);
    }

    private void addStat(String key, int by) {
        PlayerCalls.call(key(), "inc_statistic", key, (long) by);
    }

    private void putStat(String key, int value) {
        PlayerCalls.call(key(), "set_statistic", key, (long) value);
    }

    private static void positive(int amount) {
        if (amount <= 0) {
            throw new IllegalArgumentException("Amount must be greater than 0");
        }
    }

    @Override
    public void incrementStatistic(Statistic statistic) {
        addStat(PlayerStatistics.untyped(statistic), 1);
    }

    @Override
    public void incrementStatistic(Statistic statistic, int amount) {
        positive(amount);
        addStat(PlayerStatistics.untyped(statistic), amount);
    }

    @Override
    public void incrementStatistic(Statistic statistic, Material material) {
        addStat(PlayerStatistics.of(statistic, material), 1);
    }

    @Override
    public void incrementStatistic(Statistic statistic, Material material, int amount) {
        positive(amount);
        addStat(PlayerStatistics.of(statistic, material), amount);
    }

    @Override
    public void incrementStatistic(Statistic statistic, EntityType entity) {
        addStat(PlayerStatistics.of(statistic, entity), 1);
    }

    @Override
    public void incrementStatistic(Statistic statistic, EntityType entity, int amount) {
        positive(amount);
        addStat(PlayerStatistics.of(statistic, entity), amount);
    }

    @Override
    public void decrementStatistic(Statistic statistic) {
        addStat(PlayerStatistics.untyped(statistic), -1);
    }

    @Override
    public void decrementStatistic(Statistic statistic, int amount) {
        positive(amount);
        addStat(PlayerStatistics.untyped(statistic), -amount);
    }

    @Override
    public void decrementStatistic(Statistic statistic, Material material) {
        addStat(PlayerStatistics.of(statistic, material), -1);
    }

    @Override
    public void decrementStatistic(Statistic statistic, Material material, int amount) {
        positive(amount);
        addStat(PlayerStatistics.of(statistic, material), -amount);
    }

    @Override
    public void decrementStatistic(Statistic statistic, EntityType entity) {
        addStat(PlayerStatistics.of(statistic, entity), -1);
    }

    @Override
    public void decrementStatistic(Statistic statistic, EntityType entity, int amount) {
        positive(amount);
        addStat(PlayerStatistics.of(statistic, entity), -amount);
    }

    @Override
    public int getStatistic(Statistic statistic) {
        return stat(PlayerStatistics.untyped(statistic));
    }

    @Override
    public int getStatistic(Statistic statistic, Material material) {
        return stat(PlayerStatistics.of(statistic, material));
    }

    @Override
    public int getStatistic(Statistic statistic, EntityType entity) {
        return stat(PlayerStatistics.of(statistic, entity));
    }

    @Override
    public void setStatistic(Statistic statistic, int value) {
        nonNegative(value);
        putStat(PlayerStatistics.untyped(statistic), value);
    }

    @Override
    public void setStatistic(Statistic statistic, Material material, int value) {
        nonNegative(value);
        putStat(PlayerStatistics.of(statistic, material), value);
    }

    @Override
    public void setStatistic(Statistic statistic, EntityType entity, int value) {
        nonNegative(value);
        putStat(PlayerStatistics.of(statistic, entity), value);
    }

    private static void nonNegative(int value) {
        if (value < 0) {
            throw new IllegalArgumentException("Value must be greater than or equal to 0");
        }
    }

    private static String[] keys(Collection<NamespacedKey> recipes) {
        List<String> out = new ArrayList<>();
        for (NamespacedKey recipe : recipes) {
            out.add(recipe.toString());
        }
        return out.toArray(new String[0]);
    }

    @Override
    public int discoverRecipes(Collection<NamespacedKey> recipes) {
        return (int) PlayerCalls.count(key(), "discover_recipes", (Object) keys(recipes));
    }

    @Override
    public int undiscoverRecipes(Collection<NamespacedKey> recipes) {
        return (int) PlayerCalls.count(key(), "undiscover_recipes", (Object) keys(recipes));
    }

    @Override
    public boolean hasDiscoveredRecipe(NamespacedKey recipe) {
        return PlayerCalls.done(key(), "has_recipe", recipe.toString());
    }

    @Override
    public Set<NamespacedKey> getDiscoveredRecipes() {
        Set<NamespacedKey> out = new HashSet<>();
        if (PlayerCalls.call(key(), "known_recipes") instanceof String[] known) {
            for (String text : known) {
                NamespacedKey parsed = NamespacedKey.fromString(text);
                if (parsed != null) {
                    out.add(parsed);
                }
            }
        }
        return out;
    }

    private static String hex(byte[] hash) {
        if (hash == null) {
            return "";
        }
        StringBuilder out = new StringBuilder();
        for (byte value : hash) {
            out.append(String.format("%02x", value));
        }
        return out.toString();
    }

    private void pack(UUID id, String url, String hash, boolean required, Component prompt) {
        PlayerCalls.call(key(), "resource_pack", id.toString(), url, hash, required, prompt == null ? "" : Text.mini(prompt));
    }

    @Override
    public void setResourcePack(String url, byte[] hash, String prompt, boolean force) {
        pack(UUID.nameUUIDFromBytes(url.getBytes(java.nio.charset.StandardCharsets.UTF_8)), url, hex(hash), force,
            prompt == null ? null : Component.text(prompt));
    }

    @Override
    public void setResourcePack(UUID id, String url, byte[] hash, String prompt, boolean force) {
        pack(id, url, hex(hash), force, prompt == null ? null : Component.text(prompt));
    }

    @Override
    public void setResourcePack(UUID id, String url, byte[] hash, Component prompt, boolean force) {
        pack(id, url, hex(hash), force, prompt);
    }

    @Override
    public void addResourcePack(UUID id, String url, byte[] hash, String prompt, boolean force) {
        pack(id, url, hex(hash), force, prompt == null ? null : Component.text(prompt));
    }

    @Override
    public void sendResourcePacks(ResourcePackRequest request) {
        for (ResourcePackInfo info : request.packs()) {
            pack(info.id(), info.uri().toString(), info.hash(), request.required(), request.prompt());
        }
    }

    @Override
    public void removeResourcePack(UUID id) {
        PlayerCalls.call(key(), "remove_resource_pack", id.toString());
    }

    @Override
    public void removeResourcePacks() {
        PlayerCalls.call(key(), "remove_resource_pack", (Object) null);
    }

    @Override
    public void clearResourcePacks() {
        removeResourcePacks();
    }

    @Override
    public void removeResourcePacks(Iterable<UUID> ids) {
        for (UUID id : ids) {
            removeResourcePack(id);
        }
    }

    @Override
    public org.bukkit.event.player.PlayerResourcePackStatusEvent.Status getResourcePackStatus() {
        long status = PlayerCalls.integer(key(), "pack_status");
        org.bukkit.event.player.PlayerResourcePackStatusEvent.Status[] all =
            org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.values();
        return status < 0 ? null : switch ((int) status) {
            case 0 -> all[org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.SUCCESSFULLY_LOADED.ordinal()];
            case 1 -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.DECLINED;
            case 2 -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.FAILED_DOWNLOAD;
            case 3 -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.ACCEPTED;
            case 4 -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.DOWNLOADED;
            case 5 -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.INVALID_URL;
            case 6 -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.FAILED_RELOAD;
            case 7 -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.DISCARDED;
            default -> null;
        };
    }

    @Override
    public void transfer(String host, int port) {
        PlayerCalls.call(key(), "transfer", host, (long) port);
    }

    @Override
    public int getViewDistance() {
        Object held = PlayerCalls.get(key(), "view_distance");
        return held instanceof Number distance ? distance.intValue() : Bukkit.getViewDistance();
    }

    @Override
    public void setViewDistance(int distance) {
        PlayerCalls.set(key(), "view_distance", distance < 0 ? null : (Object) (long) distance);
    }

    @Override
    public int getSendViewDistance() {
        return getViewDistance();
    }

    @Override
    public void setSendViewDistance(int distance) {
        setViewDistance(distance);
    }

    @Override
    public int getSimulationDistance() {
        Object held = PlayerCalls.get(key(), "simulation_distance");
        return held instanceof Number distance ? distance.intValue() : Bukkit.getSimulationDistance();
    }

    @Override
    public void setSimulationDistance(int distance) {
        PlayerCalls.set(key(), "simulation_distance", distance < 0 ? null : (Object) (long) distance);
    }

    @Override
    public java.util.Locale locale() {
        return Locale.forLanguageTag(getLocale().replace('_', '-'));
    }

    @Override
    public String getLocale() {
        String held = PlayerCalls.text(key(), "locale");
        return held == null ? "en_us" : held;
    }

    @Override
    public int getClientViewDistance() {
        return (int) PlayerCalls.integer(key(), "client_view_distance");
    }

    @Override
    public String getClientBrandName() {
        String brand = PlayerCalls.text(key(), "client_brand");
        return brand == null || brand.isEmpty() ? null : brand;
    }

    @Override
    public int getProtocolVersion() {
        return (int) PlayerCalls.integer(key(), "protocol_version");
    }

    @Override
    public boolean isAllowingServerListings() {
        return PlayerCalls.flag(key(), "server_listings");
    }

    @Override
    public MainHand getMainHand() {
        return PlayerCalls.integer(key(), "main_hand") == 0 ? MainHand.LEFT : MainHand.RIGHT;
    }

    @Override
    public InetSocketAddress getVirtualHost() {
        String held = PlayerCalls.text(key(), "virtual_host");
        if (held == null || held.isEmpty()) {
            return null;
        }
        int colon = held.lastIndexOf(':');
        return InetSocketAddress.createUnresolved(colon < 0 ? held : held.substring(0, colon),
            colon < 0 ? 0 : Integer.parseInt(held.substring(colon + 1)));
    }

    @Override
    public InetSocketAddress getHAProxyAddress() {
        return null;
    }

    @Override
    public java.net.SocketAddress getSocketAddress() {
        return getAddress();
    }

    @Override
    public boolean isConnected() {
        return isOnline();
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T> T getClientOption(ClientOption<T> option) {
        Object value;
        if (option == ClientOption.LOCALE) {
            value = getLocale();
        } else if (option == ClientOption.VIEW_DISTANCE) {
            value = getClientViewDistance();
        } else if (option == ClientOption.CHAT_COLORS_ENABLED) {
            value = PlayerCalls.flag(key(), "chat_colors");
        } else if (option == ClientOption.TEXT_FILTERING_ENABLED) {
            value = PlayerCalls.flag(key(), "text_filtering");
        } else if (option == ClientOption.ALLOW_SERVER_LISTINGS) {
            value = isAllowingServerListings();
        } else if (option == ClientOption.MAIN_HAND) {
            value = getMainHand();
        } else if (option == ClientOption.CHAT_VISIBILITY) {
            value = switch ((int) PlayerCalls.integer(key(), "chat_visibility")) {
                case 0 -> ClientOption.ChatVisibility.FULL;
                case 1 -> ClientOption.ChatVisibility.SYSTEM;
                case 2 -> ClientOption.ChatVisibility.HIDDEN;
                default -> ClientOption.ChatVisibility.UNKNOWN;
            };
        } else if (option == ClientOption.PARTICLE_VISIBILITY) {
            value = switch ((int) PlayerCalls.integer(key(), "client_particles")) {
                case 0 -> ClientOption.ParticleVisibility.ALL;
                case 1 -> ClientOption.ParticleVisibility.DECREASED;
                default -> ClientOption.ParticleVisibility.MINIMAL;
            };
        } else if (option == ClientOption.SKIN_PARTS) {
            value = skinParts((int) PlayerCalls.integer(key(), "skin_parts"));
        } else {
            throw new IllegalArgumentException("Unknown client option " + option);
        }
        return (T) value;
    }

    private static com.destroystokyo.paper.SkinParts skinParts(int raw) {
        com.destroystokyo.paper.SkinParts.Mutable parts = com.destroystokyo.paper.SkinParts.allParts();
        parts.setCapeEnabled((raw & 1) != 0);
        parts.setJacketEnabled((raw & 2) != 0);
        parts.setLeftSleeveEnabled((raw & 4) != 0);
        parts.setRightSleeveEnabled((raw & 8) != 0);
        parts.setLeftPantsEnabled((raw & 16) != 0);
        parts.setRightPantsEnabled((raw & 32) != 0);
        parts.setHatsEnabled((raw & 64) != 0);
        return parts;
    }

    @Override
    public void sendOpLevel(byte level) {
        PlayerCalls.call(key(), "entity_event", (long) getEntityId(), (long) (24 + level));
    }

    @Override
    public void chat(String message) {
        if (message.startsWith("/")) {
            performCommand(message.substring(1));
        } else {
            PlayerCalls.call(key(), "chat", message);
        }
    }

    @Override
    public void kick(Component message, org.bukkit.event.player.PlayerKickEvent.Cause cause) {
        kick(message);
    }

    @Override
    public <E extends org.bukkit.BanEntry<? super com.destroystokyo.paper.profile.PlayerProfile>> E ban(String reason,
            java.util.Date expires, String source, boolean kickPlayer) {
        E entry = ban(reason, expires, source);
        if (kickPlayer) {
            kickPlayer(reason);
        }
        return entry;
    }

    @Override
    public <E extends org.bukkit.BanEntry<? super com.destroystokyo.paper.profile.PlayerProfile>> E ban(String reason,
            java.time.Instant expires, String source, boolean kickPlayer) {
        E entry = ban(reason, expires, source);
        if (kickPlayer) {
            kickPlayer(reason);
        }
        return entry;
    }

    @Override
    public <E extends org.bukkit.BanEntry<? super com.destroystokyo.paper.profile.PlayerProfile>> E ban(String reason,
            java.time.Duration duration, String source, boolean kickPlayer) {
        E entry = ban(reason, duration, source);
        if (kickPlayer) {
            kickPlayer(reason);
        }
        return entry;
    }

    private org.bukkit.BanList<java.net.InetAddress> ipBans() {
        return org.bukkit.Bukkit.getBanList(io.papermc.paper.ban.BanListType.IP);
    }

    private java.net.InetAddress address() {
        InetSocketAddress at = getAddress();
        if (at == null) {
            throw new IllegalStateException("The player has no address to ban");
        }
        return at.getAddress();
    }

    @Override
    public org.bukkit.BanEntry<java.net.InetAddress> banIp(String reason, java.util.Date expires, String source,
            boolean kickPlayer) {
        org.bukkit.BanEntry<java.net.InetAddress> entry = ipBans().addBan(address(), reason, expires, source);
        if (kickPlayer) {
            kickPlayer(reason);
        }
        return entry;
    }

    @Override
    public org.bukkit.BanEntry<java.net.InetAddress> banIp(String reason, java.time.Instant expires, String source,
            boolean kickPlayer) {
        org.bukkit.BanEntry<java.net.InetAddress> entry = ipBans().addBan(address(), reason, expires, source);
        if (kickPlayer) {
            kickPlayer(reason);
        }
        return entry;
    }

    @Override
    public org.bukkit.BanEntry<java.net.InetAddress> banIp(String reason, java.time.Duration duration, String source,
            boolean kickPlayer) {
        org.bukkit.BanEntry<java.net.InetAddress> entry = ipBans().addBan(address(), reason, duration, source);
        if (kickPlayer) {
            kickPlayer(reason);
        }
        return entry;
    }

    @Override
    public Player.Spigot spigot() {
        return new PlayerSpigot(this);
    }

    @Override
    public void sendLinks(org.bukkit.ServerLinks links) {
        List<String> rows = new ArrayList<>();
        for (org.bukkit.ServerLinks.ServerLink link : links.getLinks()) {
            org.bukkit.ServerLinks.Type type = link.getType();
            rows.add(type == null ? "" : type.name().toLowerCase(Locale.ROOT));
            rows.add(type == null || link.displayName() != null ? Text.mini(link.displayName()) : "");
            rows.add(link.getUrl().toString());
        }
        PlayerCalls.call(key(), "server_links", (Object) rows.toArray(new String[0]));
    }

    @Override
    public void storeCookie(NamespacedKey key, byte[] value) {
        if (value.length > 5120) {
            throw new IllegalArgumentException("Cookie payload is over 5120 bytes");
        }
        PlayerCalls.call(key(), "store_cookie", key.toString(), value);
    }

    @Override
    public java.util.concurrent.CompletableFuture<byte[]> retrieveCookie(NamespacedKey key) {
        java.util.concurrent.CompletableFuture<byte[]> future = new java.util.concurrent.CompletableFuture<>();
        String name = key.toString();
        PlayerCalls.call(key(), "cookie_request", name);
        org.bukkit.scheduler.BukkitTask[] handle = new org.bukkit.scheduler.BukkitTask[1];
        int[] waited = {0};
        handle[0] = BasaltServer.instance().getScheduler().runTaskTimer((Plugin) null, () -> {
            Object answer = PlayerCalls.call(key(), "cookie_answer", name);
            if (answer instanceof byte[] bytes) {
                handle[0].cancel();
                future.complete(bytes.length > 1 ? java.util.Arrays.copyOfRange(bytes, 1, bytes.length)
                    : bytes.length == 1 && bytes[0] == 'P' ? new byte[0] : null);
            } else if (++waited[0] > 200) {
                handle[0].cancel();
                future.complete(null);
            }
        }, 1L, 1L);
        return future;
    }

    @Override
    public Player getPlayer() {
        return this;
    }

    @Override
    public void setRespawnLocation(Location location, boolean force) {
        if (location == null) {
            PlayerCalls.call(key(), "clear_spawn_point");
            return;
        }
        if (location.getWorld() == null) {
            throw new IllegalArgumentException("Location must have a world");
        }
        PlayerCalls.call(key(), "set_spawn_point", location.getWorld().getName(), location.getX(), location.getY(),
            location.getZ(), (double) location.getYaw());
    }

    @Override
    public Location getBedLocation() {
        Location spawn = getRespawnLocation();
        if (spawn == null) {
            throw new IllegalStateException("Player has not slept in a bed");
        }
        return spawn;
    }

    @Override
    public Location getPotentialRespawnLocation() {
        Location spawn = getRespawnLocation();
        return spawn != null ? spawn : getWorld().getSpawnLocation();
    }

    @Override
    public Location getLastDeathLocation() {
        if (!(PlayerCalls.call(key(), "last_death") instanceof String[] at) || at.length != 4) {
            return null;
        }
        for (World world : org.bukkit.Bukkit.getWorlds()) {
            if (WorldNatives.worldStrings(world.getName())[0].equals(at[0])) {
                return new Location(world, Integer.parseInt(at[1]), Integer.parseInt(at[2]), Integer.parseInt(at[3]));
            }
        }
        return null;
    }

    @Override
    public void setLastDeathLocation(Location location) {
        if (location == null) {
            PlayerCalls.call(key(), "set_last_death", (Object) null);
            return;
        }
        PlayerCalls.call(key(), "set_last_death", WorldNatives.worldStrings(location.getWorld().getName())[0],
            (long) location.getBlockX(), (long) location.getBlockY(), (long) location.getBlockZ());
    }

    @Override
    public void wakeup(boolean setSpawnLocation) {
        if (!isSleeping()) {
            throw new IllegalStateException("Cannot wakeup if not sleeping");
        }
        PlayerCalls.call(key(), "wake_up");
    }

    @Override
    public int getSleepTicks() {
        return (int) PlayerCalls.integer(key(), "sleep_ticks");
    }

    @Override
    public boolean isDeeplySleeping() {
        return getSleepTicks() >= 100;
    }

    @Override
    public void swingMainHand() {
        PlayerCalls.call(key(), "swing", false);
    }

    @Override
    public void swingOffHand() {
        PlayerCalls.call(key(), "swing", true);
    }

    @Override
    public void playHurtAnimation(float yaw) {
        PlayerCalls.call(key(), "hurt_animation", (double) yaw);
    }

    @Override
    public void sendHurtAnimation(float yaw) {
        PlayerCalls.call(key(), "hurt_animation", (double) yaw);
    }

    @Override
    public void broadcastHurtAnimation(Collection<Player> players) {
        for (Player viewer : players) {
            viewer.sendHurtAnimation(0.0F);
        }
    }

    @Override
    public void sendEquipmentChange(org.bukkit.entity.LivingEntity entity, org.bukkit.inventory.EquipmentSlot slot,
            ItemStack item) {
        PlayerCalls.call(key(), "send_equipment", (long) entity.getEntityId(), (long) equipmentIndex(slot),
            snbt(item));
    }

    @Override
    public void sendEquipmentChange(org.bukkit.entity.LivingEntity entity,
            java.util.Map<org.bukkit.inventory.EquipmentSlot, ItemStack> items) {
        for (java.util.Map.Entry<org.bukkit.inventory.EquipmentSlot, ItemStack> entry : items.entrySet()) {
            sendEquipmentChange(entity, entry.getKey(), entry.getValue());
        }
    }

    private static byte[] snbt(ItemStack item) {
        byte[] bytes = item == null ? null : Items.toBytes(item);
        return bytes == null ? new byte[0] : bytes;
    }

    private static int equipmentIndex(org.bukkit.inventory.EquipmentSlot slot) {
        return switch (slot) {
            case HAND -> 0;
            case OFF_HAND -> 1;
            case FEET -> 2;
            case LEGS -> 3;
            case CHEST -> 4;
            case HEAD -> 5;
            case BODY -> 6;
            case SADDLE -> 7;
        };
    }

    @Override
    public void showElderGuardian(boolean silent) {
        PlayerCalls.call(key(), "game_event", 10L, silent ? 0.0 : 1.0);
    }

    @Override
    public void showDemoScreen() {
        PlayerCalls.call(key(), "game_event", 5L, 0.0);
    }

    @Override
    public void showWinScreen() {
        PlayerCalls.call(key(), "game_event", 4L, 1.0);
    }

    @Override
    public boolean hasSeenWinScreen() {
        return PlayerCalls.flag(key(), "seen_win_screen");
    }

    @Override
    public void setHasSeenWinScreen(boolean seen) {
        PlayerCalls.set(key(), "seen_win_screen", seen);
    }

    @Override
    public void addCustomChatCompletions(Collection<String> completions) {
        PlayerCalls.call(key(), "chat_completions", 0L, (Object) completions.toArray(new String[0]));
    }

    @Override
    public void addAdditionalChatCompletions(Collection<String> completions) {
        addCustomChatCompletions(completions);
    }

    @Override
    public void removeCustomChatCompletions(Collection<String> completions) {
        PlayerCalls.call(key(), "chat_completions", 1L, (Object) completions.toArray(new String[0]));
    }

    @Override
    public void removeAdditionalChatCompletions(Collection<String> completions) {
        removeCustomChatCompletions(completions);
    }

    @Override
    public void setCustomChatCompletions(Collection<String> completions) {
        PlayerCalls.call(key(), "chat_completions", 2L, (Object) completions.toArray(new String[0]));
    }

    @Override
    public boolean hasData(io.papermc.paper.datacomponent.DataComponentType type) {
        return false;
    }

    @Override
    public <T> T getData(io.papermc.paper.datacomponent.DataComponentType.Valued<T> type) {
        return null;
    }

    @Override
    public <T> T getDataOrDefault(io.papermc.paper.datacomponent.DataComponentType.Valued<? extends T> type,
            T fallback) {
        return fallback;
    }

    @Override
    public io.papermc.paper.connection.PlayerGameConnection getConnection() {
        return new BasaltConnection(this);
    }

    @Override
    public io.papermc.paper.threadedregions.scheduler.EntityScheduler getScheduler() {
        return new PlayerEntityScheduler(this);
    }

    @Override
    public void saveData() {
        PlayerCalls.call(key(), "save_data");
    }

    @Override
    public org.bukkit.entity.FishHook getFishHook() {
        long id = PlayerCalls.integer(key(), "fishing_hook");
        Entity hook = id < 0 ? null : BasaltServer.instance().entityById((org.basaltmc.bukkit.BasaltWorld) getWorld(), (int) id);
        return hook instanceof org.bukkit.entity.FishHook fish ? fish : null;
    }

    @Override
    public java.util.Collection<org.bukkit.entity.EnderPearl> getEnderPearls() {
        List<org.bukkit.entity.EnderPearl> pearls = new ArrayList<>();
        for (org.bukkit.entity.EnderPearl pearl : getWorld().getEntitiesByClass(org.bukkit.entity.EnderPearl.class)) {
            if (this.equals(pearl.getShooter())) {
                pearls.add(pearl);
            }
        }
        return pearls;
    }
}
