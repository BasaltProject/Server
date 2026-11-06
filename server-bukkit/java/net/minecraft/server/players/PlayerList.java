// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;
import java.io.File;
import java.net.SocketAddress;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.function.Function;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.Connection;
import net.minecraft.network.chat.ChatType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.PlayerChatMessage;
import net.minecraft.network.protocol.Packet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.PlayerAdvancements;
import net.minecraft.server.RegistryLayer;
import net.minecraft.server.ServerScoreboard;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.network.CommonListenerCookie;
import net.minecraft.server.network.ServerGamePacketListenerImpl;
import net.minecraft.server.notifications.NotificationService;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.stats.ServerStatsCounter;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.PlayerDataStorage;
public class PlayerList {
    public static final File USERBANLIST_FILE = new File("banned-players.json");
    public static final File IPBANLIST_FILE = new File("banned-ips.json");
    public static final File OPLIST_FILE = new File("ops.json");
    public static final File WHITELIST_FILE = new File("whitelist.json");
    public static final Component CHAT_FILTERED_FULL = Component.translatable("chat.filtered_full");
    public static final Component DUPLICATE_LOGIN_DISCONNECT_MESSAGE = Component.translatable("multiplayer.disconnect.duplicate_login");
    private final MinecraftServer server;
    private UserBanList bansList;
    private IpBanList ipBansList;
    private UserWhiteList whiteListList;
    private ServerOpList opsList;
    public PlayerList(MinecraftServer server, LayeredRegistryAccess<RegistryLayer> registries, PlayerDataStorage playerIo, NotificationService notificationService) {
        this.server = server;
    }
    public void placeNewPlayer(Connection connection, ServerPlayer player, CommonListenerCookie cookie) {
    }
    protected void updateEntireScoreboard(ServerScoreboard scoreboard, ServerPlayer player) {
        player.bukkitPlayer().setScoreboard(player.bukkitPlayer().getScoreboard());
    }
    public void addWorldborderListener(ServerLevel level) {
        // the native runtime already broadcasts border changes to every viewer of the level
    }
    public Optional<CompoundTag> loadPlayerData(NameAndId nameAndId) {
        byte[] bytes = org.basaltmc.bukkit.MiscNatives.playerData(nameAndId.id().toString());
        if (bytes == null || bytes.length == 0) {
            return Optional.empty();
        }
        try {
            return Optional.of(net.minecraft.nbt.NbtIo.read(new java.io.DataInputStream(new java.io.ByteArrayInputStream(bytes))));
        } catch (java.io.IOException unreadable) {
            return Optional.empty();
        }
    }
    protected void save(ServerPlayer player) {
        (player.bukkitPlayer()).saveData();
    }
    public void remove(ServerPlayer player) {
        save(player);
    }
    public Component canPlayerLogin(SocketAddress address, NameAndId nameAndId) {
        org.bukkit.Server server = org.bukkit.Bukkit.getServer();
        org.bukkit.OfflinePlayer who = server.getOfflinePlayer(nameAndId.id());
        if (who.isBanned()) {
            org.bukkit.BanEntry<?> ban = server.getBanList(org.bukkit.BanList.Type.PROFILE).getBanEntry(who.getName());
            net.minecraft.network.chat.MutableComponent reason = Component.translatable("multiplayer.disconnect.banned.reason",
                ban == null || ban.getReason() == null ? "" : ban.getReason());
            if (ban != null && ban.getExpiration() != null) {
                reason.append(Component.translatable("multiplayer.disconnect.banned.expiration", ban.getExpiration().toString()));
            }
            return reason;
        }
        if (!isWhiteListed(nameAndId)) {
            return Component.translatable("multiplayer.disconnect.not_whitelisted");
        }
        if (address instanceof java.net.InetSocketAddress inet && server.getIPBans().contains(inet.getAddress().getHostAddress())) {
            return Component.translatable("multiplayer.disconnect.banned_ip.reason", "");
        }
        if (getPlayerCount() >= getMaxPlayers() && !canBypassPlayerLimit(nameAndId)) {
            return Component.translatable("multiplayer.disconnect.server_full");
        }
        return null;
    }
    public boolean disconnectAllPlayersWithProfile(UUID playerId) {
        org.bukkit.entity.Player found = org.bukkit.Bukkit.getPlayer(playerId);
        if (found == null) {
            return false;
        }
        found.kick();
        return true;
    }
    public ServerPlayer respawn(ServerPlayer serverPlayer, boolean keepAllPlayerData, Entity.RemovalReason removalReason) {
        (serverPlayer.bukkitPlayer()).spigot().respawn();
        return serverPlayer;
    }
    public void sendActivePlayerEffects(ServerPlayer player) {
        sendActiveEffects(player, player.connection);
    }
    public void sendActiveEffects(LivingEntity livingEntity, ServerGamePacketListenerImpl connection) {
        for (net.minecraft.world.effect.MobEffectInstance effect : livingEntity.getActiveEffects()) {
            connection.send(new net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket(livingEntity.getId(), effect, false));
        }
    }
    public void sendPlayerPermissionLevel(ServerPlayer player) {
        (player.bukkitPlayer()).updateCommands();
    }
    public void tick() {
        // the Basalt server ticks its own players
    }
    public void broadcastAll(Packet<?> packet) {
        for (org.bukkit.entity.Player player : org.bukkit.Bukkit.getOnlinePlayers()) {
            ((org.bukkit.craftbukkit.entity.CraftPlayer) player).getHandle().connection.send(packet);
        }
    }
    public void broadcastAll(Packet<?> packet, ResourceKey<Level> dimension) {
        for (org.bukkit.entity.Player player : org.bukkit.Bukkit.getOnlinePlayers()) {
            ServerPlayer handle = ((org.bukkit.craftbukkit.entity.CraftPlayer) player).getHandle();
            if (handle.level().dimension() == dimension) {
                handle.connection.send(packet);
            }
        }
    }
    public void broadcastSystemToTeam(Player player, Component message) {
        org.bukkit.entity.Player self = ((net.minecraft.server.level.ServerPlayer) player).bukkitPlayer();
        org.bukkit.scoreboard.Team team = self.getScoreboard().getEntryTeam(self.getName());
        if (team == null) {
            return;
        }
        for (org.bukkit.entity.Player each : org.bukkit.Bukkit.getOnlinePlayers()) {
            boolean member = team != null && team.hasEntry(each.getName());
            if (member == true) {
                ((org.bukkit.craftbukkit.entity.CraftPlayer) each).getHandle().connection.send(new net.minecraft.network.protocol.game.ClientboundSystemChatPacket(message, false));
            }
        }
    }
    public void broadcastSystemToAllExceptTeam(Player player, Component message) {
        org.bukkit.entity.Player self = ((net.minecraft.server.level.ServerPlayer) player).bukkitPlayer();
        org.bukkit.scoreboard.Team team = self.getScoreboard().getEntryTeam(self.getName());
        for (org.bukkit.entity.Player each : org.bukkit.Bukkit.getOnlinePlayers()) {
            boolean member = team != null && team.hasEntry(each.getName());
            if (member == false) {
                ((org.bukkit.craftbukkit.entity.CraftPlayer) each).getHandle().connection.send(new net.minecraft.network.protocol.game.ClientboundSystemChatPacket(message, false));
            }
        }
    }
    public String[] getPlayerNamesArray() {
        return getServer().getPlayerNames();
    }
    public UserBanList getBans() {
        if (bansList == null) {
            bansList = org.basaltmc.bukkit.facade.NativeLists.userBans();
        }
        return bansList;
    }
    public IpBanList getIpBans() {
        if (ipBansList == null) {
            ipBansList = org.basaltmc.bukkit.facade.NativeLists.ipBans();
        }
        return ipBansList;
    }
    public void op(NameAndId nameAndId) {
        org.bukkit.Bukkit.getServer().getOfflinePlayer(nameAndId.id()).setOp(true);
    }
    public void op(NameAndId nameAndId, Optional<LevelBasedPermissionSet> permissions, Optional<Boolean> canBypassPlayerLimit) {
        org.bukkit.Bukkit.getOfflinePlayer(nameAndId.id()).setOp(true);
    }
    public void deop(NameAndId nameAndId) {
        org.bukkit.Bukkit.getServer().getOfflinePlayer(nameAndId.id()).setOp(false);
    }
    public boolean isWhiteListed(NameAndId nameAndId) {
        return !isUsingWhitelist() || org.bukkit.Bukkit.getServer().getWhitelistedPlayers().stream().anyMatch(p -> p.getUniqueId().equals(nameAndId.id()));
    }
    public boolean isOp(NameAndId nameAndId) {
        return org.bukkit.Bukkit.getServer().getOperators().stream().anyMatch(p -> p.getUniqueId().equals(nameAndId.id()));
    }
    public ServerPlayer getPlayerByName(String name) {
        return getPlayer(name);
    }
    public void broadcast(Player except, double x, double y, double z, double range, ResourceKey<Level> dimension, Packet<?> packet) {
        for (org.bukkit.entity.Player each : org.bukkit.Bukkit.getOnlinePlayers()) {
            ServerPlayer handle = ((org.bukkit.craftbukkit.entity.CraftPlayer) each).getHandle();
            if (handle == except || handle.level().dimension() != dimension) {
                continue;
            }
            org.bukkit.Location at = each.getLocation();
            double dx = x - at.getX();
            double dy = y - at.getY();
            double dz = z - at.getZ();
            if (dx * dx + dy * dy + dz * dz < range * range) {
                handle.connection.send(packet);
            }
        }
    }
    public void saveAll() {
        org.bukkit.Bukkit.getServer().savePlayers();
    }
    public UserWhiteList getWhiteList() {
        if (whiteListList == null) {
            whiteListList = org.basaltmc.bukkit.facade.NativeLists.whitelist();
        }
        return whiteListList;
    }
    public String[] getWhiteListNames() {
        return org.bukkit.Bukkit.getServer().getWhitelistedPlayers().stream().map(org.bukkit.OfflinePlayer::getName).toArray(String[]::new);
    }
    public ServerOpList getOps() {
        if (opsList == null) {
            opsList = org.basaltmc.bukkit.facade.NativeLists.operators();
        }
        return opsList;
    }
    public String[] getOpNames() {
        return org.bukkit.Bukkit.getServer().getOperators().stream().map(org.bukkit.OfflinePlayer::getName).toArray(String[]::new);
    }
    public void reloadWhiteList() {
        org.bukkit.Bukkit.getServer().reloadWhitelist();
    }
    public void sendLevelInfo(ServerPlayer player, ServerLevel level) {
        org.bukkit.entity.Player target = player.bukkitPlayer();
        target.resetPlayerTime();
        target.resetPlayerWeather();
    }
    public void sendAllPlayerInfo(ServerPlayer player) {
        player.bukkitPlayer().updateInventory();
    }
    public int getPlayerCount() {
        return org.bukkit.Bukkit.getServer().getOnlinePlayers().size();
    }
    public int getMaxPlayers() {
        return org.bukkit.Bukkit.getServer().getMaxPlayers();
    }
    public boolean isUsingWhitelist() {
        return org.bukkit.Bukkit.getServer().hasWhitelist();
    }
    public List<ServerPlayer> getPlayersWithAddress(String ip) {
        List<ServerPlayer> result = new java.util.ArrayList<>();
        for (ServerPlayer player : getPlayers()) {
            java.net.SocketAddress address = player.connection.getRemoteAddress();
            if (address instanceof java.net.InetSocketAddress inet && inet.getAddress() != null && inet.getAddress().getHostAddress().equals(ip)) {
        result.add(player);
            }
        }
        return result;
    }
    public int getViewDistance() {
        return org.bukkit.Bukkit.getServer().getViewDistance();
    }
    public int getSimulationDistance() {
        return org.bukkit.Bukkit.getServer().getSimulationDistance();
    }
    public MinecraftServer getServer() {
        return server;
    }
    public void removeAll() {
        for (org.bukkit.entity.Player player : org.bukkit.Bukkit.getOnlinePlayers()) {
            player.kick();
        }
    }
    public void broadcastSystemMessage(Component message, boolean overlay) {
        for (org.bukkit.entity.Player player : org.bukkit.Bukkit.getServer().getOnlinePlayers()) {
            if (overlay) {
        player.sendActionBar(message.asAdventure());
            } else {
        player.sendMessage(message.asAdventure());
            }
        }
    }
    public void broadcastSystemMessage(Component message, Function<ServerPlayer, Component> playerMessages, boolean overlay) {
        for (ServerPlayer player : getPlayers()) {
            Component personal = playerMessages.apply(player);
            org.bukkit.entity.Player bukkit = org.bukkit.Bukkit.getServer().getPlayer(player.getUUID());
            if (bukkit != null) {
        Component shown = personal != null ? personal : message;
        if (overlay) {
            bukkit.sendActionBar(shown.asAdventure());
        } else {
            bukkit.sendMessage(shown.asAdventure());
        }
            }
        }
    }
    public void broadcastChatMessage(PlayerChatMessage message, CommandSourceStack sender, ChatType.Bound chatType) {
        broadcastAll(new net.minecraft.network.protocol.game.ClientboundDisguisedChatPacket(message.unsignedContent() != null ? message.unsignedContent() : Component.literal(message.signedContent()), chatType));
    }
    public void broadcastChatMessage(PlayerChatMessage message, ServerPlayer sender, ChatType.Bound chatType) {
        broadcastAll(new net.minecraft.network.protocol.game.ClientboundDisguisedChatPacket(message.unsignedContent() != null ? message.unsignedContent() : Component.literal(message.signedContent()), chatType));
    }
    public ServerStatsCounter getPlayerStats(Player player) {
        return new net.minecraft.stats.ServerStatsCounter((ServerPlayer) player);
    }
    public PlayerAdvancements getPlayerAdvancements(ServerPlayer player) {
        return new PlayerAdvancements(null, this, server.getAdvancements(), null, player);
    }
    public void setViewDistance(int viewDistance) {
        // distances are server settings
    }
    public void setSimulationDistance(int simulationDistance) {
        // distances are server settings
    }
    public List<ServerPlayer> getPlayers() {
        List<ServerPlayer> result = new java.util.ArrayList<>();
        for (org.bukkit.entity.Player player : org.bukkit.Bukkit.getServer().getOnlinePlayers()) {
            if (player instanceof org.bukkit.craftbukkit.entity.CraftPlayer craft) {
        result.add(craft.getHandle());
            }
        }
        return result;
    }
    public Map<UUID, ServerPlayer> getPlayersByUUID() {
        Map<UUID, ServerPlayer> result = new java.util.LinkedHashMap<>();
        for (ServerPlayer player : getPlayers()) {
            result.put(player.getUUID(), player);
        }
        return result;
    }
    public ServerPlayer getPlayer(UUID uuid) {
        return org.bukkit.Bukkit.getServer().getPlayer(uuid) instanceof org.bukkit.craftbukkit.entity.CraftPlayer craft ? craft.getHandle() : null;
    }
    public ServerPlayer getPlayer(String playerName) {
        return org.bukkit.Bukkit.getServer().getPlayerExact(playerName) instanceof org.bukkit.craftbukkit.entity.CraftPlayer craft ? craft.getHandle() : null;
    }
    public boolean canBypassPlayerLimit(NameAndId nameAndId) {
        return false;
    }
    public void reloadResources() {
    }
}
