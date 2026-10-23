package org.basaltmc.bukkit;

import java.net.InetAddress;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.UUID;
import org.basaltmc.bukkit.gen.ServerBase;
import org.basaltmc.bukkit.misc.Access;
import org.basaltmc.bukkit.misc.BasaltBanList;
import org.basaltmc.bukkit.misc.BasaltOfflinePlayer;
import org.basaltmc.bukkit.misc.BasaltPlayerProfile;
import org.bukkit.BanList;
import org.bukkit.OfflinePlayer;
import org.bukkit.entity.Player;

/** The parts of the server object that live in the player database and the smaller services. */
public abstract class MiscServer extends ServerBase {
    private final BasaltBanList.Profiles profileBans = new BasaltBanList.Profiles();
    private final BasaltBanList.Addresses addressBans = new BasaltBanList.Addresses();

    private org.basaltmc.bukkit.misc.BasaltScoreboardManager scoreboards;

    @Override
    public synchronized org.bukkit.scoreboard.ScoreboardManager getScoreboardManager() {
        if (scoreboards == null) {
            scoreboards = new org.basaltmc.bukkit.misc.BasaltScoreboardManager();
        }
        return scoreboards;
    }

    @Override
    public org.bukkit.scoreboard.Criteria getScoreboardCriteria(String criteria) {
        return new org.basaltmc.bukkit.misc.BasaltCriteria(criteria);
    }

    @Override
    public org.bukkit.boss.BossBar createBossBar(String title, org.bukkit.boss.BarColor color,
            org.bukkit.boss.BarStyle style, org.bukkit.boss.BarFlag... flags) {
        return org.basaltmc.bukkit.misc.BasaltBossBar.create(null, title, color, style, flags);
    }

    @Override
    public org.bukkit.boss.KeyedBossBar createBossBar(org.bukkit.NamespacedKey key, String title,
            org.bukkit.boss.BarColor color, org.bukkit.boss.BarStyle style, org.bukkit.boss.BarFlag... flags) {
        if (key == null) {
            throw new IllegalArgumentException("key");
        }
        return org.basaltmc.bukkit.misc.BasaltBossBar.create(key, title, color, style, flags);
    }

    @Override
    public java.util.Iterator<org.bukkit.boss.KeyedBossBar> getBossBars() {
        return java.util.Collections.unmodifiableList(org.basaltmc.bukkit.misc.BasaltBossBar.kept()).iterator();
    }

    @Override
    public org.bukkit.boss.KeyedBossBar getBossBar(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.misc.BasaltBossBar.get(key);
    }

    @Override
    public boolean removeBossBar(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.misc.BasaltBossBar.remove(key);
    }

    @Override
    public org.bukkit.advancement.Advancement getAdvancement(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.misc.BasaltAdvancement.get(key);
    }

    @Override
    public java.util.Iterator<org.bukkit.advancement.Advancement> advancementIterator() {
        return java.util.Collections.unmodifiableList(org.basaltmc.bukkit.misc.BasaltAdvancement.all()).iterator();
    }

    @Override
    public org.bukkit.ServerTickManager getServerTickManager() {
        return new org.basaltmc.bukkit.misc.BasaltTickManager();
    }

    @Override
    public org.bukkit.ServerLinks getServerLinks() {
        return new org.basaltmc.bukkit.misc.BasaltServerLinks();
    }

    @Override
    public io.papermc.paper.datapack.DatapackManager getDatapackManager() {
        return new org.basaltmc.bukkit.misc.BasaltDatapackManager();
    }

    @Override
    public org.bukkit.map.MapView getMap(int id) {
        return org.basaltmc.bukkit.misc.BasaltMapView.get(id);
    }

    @Override
    public org.bukkit.map.MapView createMap(org.bukkit.World world) {
        return org.basaltmc.bukkit.misc.BasaltMapView.create(world);
    }

    @Override
    public org.bukkit.World getWorld(net.kyori.adventure.key.Key key) {
        for (org.bukkit.World world : getWorlds()) {
            if (world.key().equals(key)) {
                return world;
            }
        }
        return null;
    }

    private record Pack(UUID id, String url, String hash, String prompt, boolean required)
            implements org.bukkit.packs.ResourcePack {
        @Override
        public UUID getId() {
            return id;
        }

        @Override
        public String getUrl() {
            return url;
        }

        @Override
        public String getHash() {
            return hash.isEmpty() ? null : hash;
        }

        @Override
        public String getPrompt() {
            return prompt;
        }

        @Override
        public boolean isRequired() {
            return required;
        }
    }

    private static Pack pack() {
        String[] fields = MiscNatives.resourcePack();
        if (fields == null) {
            return null;
        }
        String prompt = fields[4].isEmpty() ? null
            : org.basaltmc.bukkit.misc.Json.legacyOf(org.basaltmc.bukkit.misc.Json.component(fields[4]));
        return new Pack(UUID.fromString(fields[0]), fields[1], fields[2], prompt, "1".equals(fields[3]));
    }

    @Override
    public org.bukkit.packs.ResourcePack getServerResourcePack() {
        return pack();
    }

    @Override
    public String getResourcePack() {
        Pack pack = pack();
        return pack == null ? "" : pack.url();
    }

    @Override
    public String getResourcePackHash() {
        Pack pack = pack();
        return pack == null ? "" : pack.hash();
    }

    @Override
    public String getResourcePackPrompt() {
        Pack pack = pack();
        return pack == null ? "" : pack.prompt() == null ? "" : pack.prompt();
    }

    @Override
    public boolean isResourcePackRequired() {
        Pack pack = pack();
        return pack != null && pack.required();
    }

    @Override
    public double[] getTPS() {
        double[] pace = MiscNatives.tmPace();
        return new double[] {pace[0], pace[1], pace[2]};
    }

    @Override
    public double getAverageTickTime() {
        return MiscNatives.tmPace()[3];
    }

    @Override
    public void sendPluginMessage(org.bukkit.plugin.Plugin source, String channel, byte[] message) {
        org.bukkit.plugin.messaging.StandardMessenger.validatePluginMessage(getMessenger(), source, channel, message);
        for (Player player : getOnlinePlayers()) {
            player.sendPluginMessage(source, channel, message);
        }
    }

    @Override
    public UUID getPlayerUniqueId(String playerName) {
        Player online = getPlayerExact(playerName);
        if (online != null) {
            return online.getUniqueId();
        }
        Access.Known known = Access.byName(playerName);
        return known == null ? null : known.id();
    }

    @Override
    public void reloadPermissions() {
        for (Player player : getOnlinePlayers()) {
            player.recalculatePermissions();
        }
    }

    @Override
    public void savePlayers() {
        Natives.runConsoleCommand("save-all flush");
    }

    @Override
    public net.kyori.adventure.text.Component motd() {
        return org.basaltmc.bukkit.misc.Json.fromLegacy(getMotd());
    }

    @Override
    public net.kyori.adventure.text.Component permissionMessage() {
        return org.basaltmc.bukkit.misc.Json.fromLegacy(getPermissionMessage());
    }

    @Override
    public String getPermissionMessage() {
        return "\u00a7cI'm sorry, but you do not have permission to perform this command. Please contact the server "
            + "administrators if you believe that this is a mistake.";
    }

    @Override
    public org.bukkit.loot.LootTable getLootTable(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.misc.BasaltLootTable.get(key);
    }

    @Override
    public java.util.List<org.bukkit.entity.Entity> selectEntities(org.bukkit.command.CommandSender sender,
            String selector) {
        String[] flat = MiscNatives.select(selector,
            sender instanceof Player player ? player.getUniqueId().toString() : null);
        if (flat == null || flat.length == 0 || "error".equals(flat[0])) {
            throw new IllegalArgumentException(flat != null && flat.length > 1 ? flat[1] : "invalid selector");
        }
        java.util.List<org.bukkit.entity.Entity> out = new java.util.ArrayList<>();
        for (int i = 1; i + 2 < flat.length; i += 3) {
            org.bukkit.entity.Entity found = getEntity(UUID.fromString(flat[i]));
            if (found != null) {
                out.add(found);
            }
        }
        return out;
    }

    @Override
    public OfflinePlayer getOfflinePlayer(UUID id) {
        Player online = getPlayer(id);
        if (online != null) {
            return online;
        }
        Access.Known known = Access.byUuid(id);
        return new BasaltOfflinePlayer(id, known == null ? null : known.name());
    }

    @Override
    public OfflinePlayer getOfflinePlayer(String name) {
        Player online = getPlayerExact(name);
        if (online != null) {
            return online;
        }
        Access.Known known = Access.byName(name);
        return known != null ? new BasaltOfflinePlayer(known.id(), known.name())
            : new BasaltOfflinePlayer(Access.offlineUuid(name), name);
    }

    @Override
    public OfflinePlayer getOfflinePlayerIfCached(String name) {
        Player online = getPlayerExact(name);
        if (online != null) {
            return online;
        }
        Access.Known known = Access.byName(name);
        return known == null ? null : new BasaltOfflinePlayer(known.id(), known.name());
    }

    @Override
    public OfflinePlayer[] getOfflinePlayers() {
        Set<UUID> seen = new LinkedHashSet<>();
        java.util.List<OfflinePlayer> out = new java.util.ArrayList<>();
        for (Player online : getOnlinePlayers()) {
            if (seen.add(online.getUniqueId())) {
                out.add(online);
            }
        }
        for (Access.Known known : Access.all()) {
            if (seen.add(known.id())) {
                out.add(new BasaltOfflinePlayer(known.id(), known.name()));
            }
        }
        return out.toArray(new OfflinePlayer[0]);
    }

    @Override
    public Set<OfflinePlayer> getOperators() {
        Set<OfflinePlayer> out = new LinkedHashSet<>();
        for (Access.Known known : Access.operators()) {
            out.add(getOfflinePlayer(known.id()));
        }
        return out;
    }

    @Override
    public Set<OfflinePlayer> getBannedPlayers() {
        Set<OfflinePlayer> out = new LinkedHashSet<>();
        for (String subject : MiscNatives.bans(false)) {
            out.add(getOfflinePlayer(UUID.fromString(subject)));
        }
        return out;
    }

    @Override
    public Set<String> getIPBans() {
        return new LinkedHashSet<>(java.util.List.of(MiscNatives.bans(true)));
    }

    @Override
    public void banIP(String address) {
        addressBans.addBan(address, null, null, null);
    }

    @Override
    public void banIP(InetAddress address) {
        addressBans.addBan(address, null, (java.util.Date) null, null);
    }

    @Override
    public void unbanIP(String address) {
        addressBans.pardon(address);
    }

    @Override
    public void unbanIP(InetAddress address) {
        addressBans.pardon(address);
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T extends BanList<?>> T getBanList(BanList.Type type) {
        return (T) (type == BanList.Type.IP ? addressBans : profileBans);
    }

    @Override
    @SuppressWarnings("unchecked")
    public <B extends BanList<E>, E> B getBanList(io.papermc.paper.ban.BanListType<B> type) {
        return (B) (type == io.papermc.paper.ban.BanListType.IP ? addressBans : profileBans);
    }

    @Override
    public boolean hasWhitelist() {
        return MiscNatives.whitelistEnforced();
    }

    @Override
    public void setWhitelist(boolean value) {
        MiscNatives.setWhitelistEnforced(value);
    }

    @Override
    public boolean isWhitelistEnforced() {
        return MiscNatives.whitelistEnforced();
    }

    @Override
    public void setWhitelistEnforced(boolean value) {
        MiscNatives.setWhitelistEnforced(value);
    }

    @Override
    public Set<OfflinePlayer> getWhitelistedPlayers() {
        Set<OfflinePlayer> out = new LinkedHashSet<>();
        for (Access.Known known : Access.whitelisted()) {
            out.add(getOfflinePlayer(known.id()));
        }
        return out;
    }

    @Override
    public void reloadWhitelist() {}

    @Override
    public org.bukkit.profile.PlayerProfile createPlayerProfile(UUID uniqueId, String name) {
        return new BasaltPlayerProfile(uniqueId, name);
    }

    @Override
    public org.bukkit.profile.PlayerProfile createPlayerProfile(UUID uniqueId) {
        return new BasaltPlayerProfile(uniqueId, null);
    }

    @Override
    public org.bukkit.profile.PlayerProfile createPlayerProfile(String name) {
        return new BasaltPlayerProfile(null, name);
    }

    @Override
    public com.destroystokyo.paper.profile.PlayerProfile createProfile(UUID uuid) {
        return new BasaltPlayerProfile(uuid, null);
    }

    @Override
    public com.destroystokyo.paper.profile.PlayerProfile createProfile(String name) {
        return new BasaltPlayerProfile(null, name);
    }

    @Override
    public com.destroystokyo.paper.profile.PlayerProfile createProfile(UUID uuid, String name) {
        return new BasaltPlayerProfile(uuid, name);
    }

    @Override
    public com.destroystokyo.paper.profile.PlayerProfile createProfileExact(UUID uuid, String name) {
        return new BasaltPlayerProfile(uuid, name);
    }
}
