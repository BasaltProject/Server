package org.basaltmc.bukkit;

import java.net.InetSocketAddress;
import java.util.Set;
import java.util.UUID;
import net.kyori.adventure.text.Component;
import org.bukkit.GameMode;
import org.bukkit.Location;
import org.bukkit.Server;
import org.bukkit.World;
import org.bukkit.entity.EntityType;
import org.bukkit.event.player.PlayerTeleportEvent;
import org.bukkit.permissions.Permission;
import org.bukkit.permissions.PermissionAttachment;
import org.bukkit.permissions.PermissionAttachmentInfo;
import org.bukkit.plugin.Plugin;
import org.basaltmc.bukkit.gen.PlayerBase;
import org.basaltmc.bukkit.misc.BasaltPermissible;
import org.basaltmc.bukkit.misc.OfflineSupport;

public class BasaltPlayer extends PlayerBase {
    private static final int kHealth = 0;
    private static final int kFood = 1;
    private static final int kLevel = 2;
    private static final int kX = 3;
    private static final int kY = 4;
    private static final int kZ = 5;
    private static final int kPing = 6;
    private static final int kSneaking = 7;

    private final UUID id;
    private final String uuid;
    private final String name;

    private final BasaltPermissible permissible;

    protected BasaltPlayer(UUID id, String name) {
        this.id = id;
        this.uuid = id.toString();
        this.name = name;
        this.permissible = new BasaltPermissible(this, id);
    }

    private volatile org.bukkit.scoreboard.Scoreboard scoreboard;

    @Override
    public org.bukkit.advancement.AdvancementProgress getAdvancementProgress(
            org.bukkit.advancement.Advancement advancement) {
        return new org.basaltmc.bukkit.misc.BasaltAdvancementProgress(uuid,
            (org.basaltmc.bukkit.misc.BasaltAdvancement) advancement);
    }

    @Override
    public void showBossBar(net.kyori.adventure.bossbar.BossBar bar) {
        org.basaltmc.bukkit.misc.AdventureBars.show(id, bar);
    }

    @Override
    public void hideBossBar(net.kyori.adventure.bossbar.BossBar bar) {
        org.basaltmc.bukkit.misc.AdventureBars.hide(id, bar);
    }

    public void publishPermissions() {
        permissible.publish();
    }

    @Override
    public org.bukkit.scoreboard.Scoreboard getScoreboard() {
        org.bukkit.scoreboard.Scoreboard held = scoreboard;
        return held != null ? held : BasaltServer.instance().getScoreboardManager().getMainScoreboard();
    }

    @Override
    public void setScoreboard(org.bukkit.scoreboard.Scoreboard board) {
        if (board == null) {
            throw new IllegalArgumentException("Scoreboard cannot be null");
        }
        if (!(board instanceof org.basaltmc.bukkit.misc.BasaltScoreboard own)) {
            throw new IllegalArgumentException("Scoreboard must be a Basalt scoreboard");
        }
        MiscNatives.sbShow(uuid, own.handle());
        scoreboard = own.handle() == 0 ? null : own;
    }

    private double number(int index) {
        double[] values = Natives.playerNumbers(uuid);
        return values == null ? 0.0 : values[index];
    }

    private String text(int index) {
        String[] values = Natives.playerStrings(uuid);
        return values == null ? "" : values[index];
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public UUID getUniqueId() {
        return id;
    }

    @Override
    public EntityType getType() {
        return EntityType.PLAYER;
    }

    @Override
    public Component name() {
        return Component.text(name);
    }

    @Override
    public String getDisplayName() {
        return name;
    }

    @Override
    public Component displayName() {
        return Component.text(name);
    }

    @Override
    public String getPlayerListName() {
        return name;
    }

    @Override
    public Server getServer() {
        return BasaltServer.instance();
    }

    @Override
    public boolean isOnline() {
        return Natives.playerStrings(uuid) != null;
    }

    @Override
    public boolean isValid() {
        return isOnline();
    }




    @Override
    public int getFoodLevel() {
        return (int) number(kFood);
    }

    @Override
    public void setFoodLevel(int food) {
        Natives.playerSetFood(uuid, food);
    }

    @Override
    public int getLevel() {
        return (int) number(kLevel);
    }

    @Override
    public int getPing() {
        return (int) number(kPing);
    }

    @Override
    public boolean isSneaking() {
        return number(kSneaking) != 0.0;
    }

    @Override
    public GameMode getGameMode() {
        String mode = text(2);
        try {
            return GameMode.valueOf(mode.toUpperCase(java.util.Locale.ROOT));
        } catch (IllegalArgumentException e) {
            return GameMode.SURVIVAL;
        }
    }

    @Override
    public void setGameMode(GameMode mode) {
        Natives.playerSetGamemode(uuid, mode.name().toLowerCase(java.util.Locale.ROOT));
    }

    @Override
    public World getWorld() {
        return BasaltServer.instance().world(text(1));
    }

    @Override
    public Location getLocation() {
        double[] extra = WorldNatives.playerExtra(uuid);
        return new Location(getWorld(), number(kX), number(kY), number(kZ), extra == null ? 0f : (float) extra[0],
            extra == null ? 0f : (float) extra[1]);
    }

    @Override
    public Location getLocation(Location into) {
        if (into != null) {
            Location here = getLocation();
            into.setWorld(here.getWorld());
            into.setX(here.getX());
            into.setY(here.getY());
            into.setZ(here.getZ());
            into.setYaw(here.getYaw());
            into.setPitch(here.getPitch());
        }
        return into;
    }

    @Override
    public boolean teleport(Location location) {
        World world = location.getWorld() == null ? getWorld() : location.getWorld();
        Blocks.flushAll();
        return WorldNatives.playerMove(uuid, world.getName(), location.getX(), location.getY(), location.getZ(),
            location.getYaw(), location.getPitch());
    }

    @Override
    public boolean teleport(Location location, PlayerTeleportEvent.TeleportCause cause) {
        return teleport(location);
    }

    @Override
    public void sendMessage(String message) {
        Natives.playerSend(uuid, Text.mini(message));
    }

    @Override
    public void sendMessage(String... messages) {
        for (String message : messages) {
            sendMessage(message);
        }
    }

    @Override
    public void sendMessage(UUID sender, String message) {
        sendMessage(message);
    }

    @Override
    public void sendMessage(UUID sender, String... messages) {
        sendMessage(messages);
    }

    @Override
    public void sendRawMessage(String message) {
        sendMessage(message);
    }

    @Override
    public void sendRawMessage(UUID sender, String message) {
        sendMessage(message);
    }

    @Override
    public void sendMessage(Component message) {
        Natives.playerSend(uuid, Text.mini(message));
    }

    @Override
    public void sendActionBar(Component message) {
        Natives.playerActionBar(uuid, Text.mini(message));
    }

    @Override
    public void kickPlayer(String message) {
        Natives.playerKick(uuid, Text.mini(message == null ? "" : message));
    }

    @Override
    public void kick(Component message) {
        Natives.playerKick(uuid, Text.mini(message == null ? Component.empty() : message));
    }

    @Override
    public void sendPluginMessage(Plugin source, String channel, byte[] message) {
        Natives.playerChannelSend(uuid, channel, message);
    }

    @Override
    public Set<String> getListeningPluginChannels() {
        return BasaltServer.instance().getMessenger().getIncomingChannels();
    }

    @Override
    public boolean isOp() {
        double[] extra = WorldNatives.playerExtra(uuid);
        return extra != null && extra[2] >= 2.0 || Natives.playerMay(uuid, "*");
    }

    @Override
    public void setOp(boolean value) {
        OfflineSupport.setOp(id, value);
        permissible.recalculatePermissions();
    }

    @Override
    public boolean isPermissionSet(String permission) {
        return permissible.isPermissionSet(permission);
    }

    @Override
    public boolean isPermissionSet(Permission permission) {
        return permissible.isPermissionSet(permission);
    }

    @Override
    public boolean hasPermission(String permission) {
        return permissible.hasPermission(permission);
    }

    @Override
    public boolean hasPermission(Permission permission) {
        return permissible.hasPermission(permission);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin) {
        return permissible.addAttachment(plugin);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, int ticks) {
        return permissible.addAttachment(plugin, ticks);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, String name, boolean value) {
        return permissible.addAttachment(plugin, name, value);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, String name, boolean value, int ticks) {
        return permissible.addAttachment(plugin, name, value, ticks);
    }

    @Override
    public void removeAttachment(PermissionAttachment attachment) {
        permissible.removeAttachment(attachment);
    }

    @Override
    public void recalculatePermissions() {
        permissible.recalculatePermissions();
    }

    @Override
    public Set<PermissionAttachmentInfo> getEffectivePermissions() {
        return permissible.getEffectivePermissions();
    }

    @Override
    public Location getRespawnLocation(boolean loadLocationAndValidate) {
        return OfflineSupport.respawnLocation(id);
    }

    @Override
    public boolean isBanned() {
        return OfflineSupport.isBanned(id);
    }

    @Override
    public <E extends org.bukkit.BanEntry<? super com.destroystokyo.paper.profile.PlayerProfile>> E ban(String reason,
            java.util.Date expires, String source) {
        return OfflineSupport.ban(id, name, reason, expires, source);
    }

    @Override
    public <E extends org.bukkit.BanEntry<? super com.destroystokyo.paper.profile.PlayerProfile>> E ban(String reason,
            java.time.Instant expires, String source) {
        return OfflineSupport.ban(id, name, reason, expires, source);
    }

    @Override
    public <E extends org.bukkit.BanEntry<? super com.destroystokyo.paper.profile.PlayerProfile>> E ban(String reason,
            java.time.Duration duration, String source) {
        return OfflineSupport.ban(id, name, reason, duration, source);
    }

    @Override
    public boolean isWhitelisted() {
        return OfflineSupport.isWhitelisted(id);
    }

    @Override
    public void setWhitelisted(boolean value) {
        OfflineSupport.setWhitelisted(id, name, value);
    }

    @Override
    public long getFirstPlayed() {
        return OfflineSupport.firstPlayed(id);
    }

    @Override
    public long getLastPlayed() {
        return System.currentTimeMillis();
    }

    @Override
    public long getLastLogin() {
        return OfflineSupport.lastSeen(id);
    }

    @Override
    public long getLastSeen() {
        return System.currentTimeMillis();
    }

    @Override
    public boolean hasPlayedBefore() {
        return OfflineSupport.hasPlayedBefore(id);
    }

    @Override
    public com.destroystokyo.paper.profile.PlayerProfile getPlayerProfile() {
        org.basaltmc.bukkit.misc.BasaltPlayerProfile profile = new org.basaltmc.bukkit.misc.BasaltPlayerProfile(id, name);
        String[] flat = MiscNatives.playerProfile(uuid);
        for (int i = 0; flat != null && i + 2 < flat.length; i += 3) {
            profile.setProperty(new com.destroystokyo.paper.profile.ProfileProperty(flat[i], flat[i + 1],
                flat[i + 2].isEmpty() ? null : flat[i + 2]));
        }
        return profile;
    }

    @Override
    public boolean isConversing() {
        return false;
    }

    @Override
    public org.bukkit.entity.Player.Spigot spigot() {
        return SpigotText.player(this::sendMessage, null);
    }

    @Override
    public InetSocketAddress getAddress() {
        String text = WorldNatives.playerAddress(uuid);
        if (text == null || text.isEmpty()) {
            return null;
        }
        int colon = text.lastIndexOf(':');
        try {
            return colon < 0 ? new InetSocketAddress(text, 0)
                : new InetSocketAddress(text.substring(0, colon), Integer.parseInt(text.substring(colon + 1)));
        } catch (NumberFormatException e) {
            return null;
        }
    }

    private double extra(int index) {
        double[] values = WorldNatives.playerExtra(uuid);
        return values == null ? 0.0 : values[index];
    }

    private org.basaltmc.bukkit.entity.EntityCore entityCore;

    @Override
    public synchronized org.basaltmc.bukkit.entity.EntityCore core() {
        BasaltWorld world = (BasaltWorld) getWorld();
        int entityId = getEntityId();
        if (entityCore == null || entityCore.world != world || entityCore.id != entityId) {
            entityCore = new org.basaltmc.bukkit.entity.EntityCore(world, entityId, id, EntityType.PLAYER);
        }
        return entityCore;
    }

    @Override
    public org.bukkit.inventory.EntityEquipment getEquipment() {
        return new org.basaltmc.bukkit.entity.BasaltPlayerEquipment(getInventory(), this);
    }

    private BasaltPlayerInventory inventory;
    private final MetadataHolder metadata = new MetadataHolder();
    private final java.util.Set<String> tags = new java.util.HashSet<>();

    @Override
    public int getEntityId() {
        return (int) extra(6);
    }

    @Override
    public org.bukkit.inventory.PlayerInventory getInventory() {
        if (inventory == null) {
            inventory = new BasaltPlayerInventory(this, uuid);
        }
        return inventory;
    }

    @Override
    public org.bukkit.inventory.ItemStack getItemInHand() {
        return getInventory().getItemInMainHand();
    }

    @Override
    public boolean teleport(Location location, PlayerTeleportEvent.TeleportCause cause,
            io.papermc.paper.entity.TeleportFlag... flags) {
        return teleport(location);
    }

    @Override
    public boolean teleport(org.bukkit.entity.Entity destination) {
        return teleport(destination.getLocation());
    }

    @Override
    public boolean teleport(org.bukkit.entity.Entity destination, PlayerTeleportEvent.TeleportCause cause) {
        return teleport(destination.getLocation());
    }

    @Override
    public java.util.concurrent.CompletableFuture<Boolean> teleportAsync(Location location,
            PlayerTeleportEvent.TeleportCause cause, io.papermc.paper.entity.TeleportFlag... flags) {
        return java.util.concurrent.CompletableFuture.completedFuture(teleport(location));
    }

    @Override
    public float getYaw() {
        return (float) extra(0);
    }

    @Override
    public float getPitch() {
        return (float) extra(1);
    }

    @Override
    public double getX() {
        return number(kX);
    }

    @Override
    public double getY() {
        return number(kY);
    }

    @Override
    public double getZ() {
        return number(kZ);
    }

    @Override
    public boolean isOnGround() {
        return extra(4) != 0.0;
    }

    @Override
    public boolean isSprinting() {
        return extra(5) != 0.0;
    }

    @Override
    public boolean isFlying() {
        return extra(3) != 0.0;
    }

    @Override
    public boolean getAllowFlight() {
        return isFlying() || getGameMode() == GameMode.CREATIVE || getGameMode() == GameMode.SPECTATOR;
    }

    @Override
    public void setAllowFlight(boolean flight) {}

    @Override
    public void setFlying(boolean value) {}

    @Override
    public float getExp() {
        return (float) extra(7);
    }

    @Override
    public void setExp(float exp) {
        WorldNatives.playerExperience(uuid, exp, getLevel());
    }

    @Override
    public void setLevel(int level) {
        WorldNatives.playerExperience(uuid, getExp(), level);
    }

    @Override
    public int getTotalExperience() {
        return (int) extra(8);
    }

    @Override
    public float getSaturation() {
        return (float) extra(9);
    }

    @Override
    public boolean performCommand(String command) {
        Blocks.flushAll();
        return WorldNatives.playerCommand(uuid, command);
    }

    @Override
    public void updateInventory() {
        InventoryNatives.windowResync(uuid);
    }

    @Override
    public void setViewDistance(int distance) {}

    @Override
    public String getLocale() {
        return "en_us";
    }

    @Override
    public double getHeight() {
        return 1.8;
    }

    @Override
    public double getWidth() {
        return 0.6;
    }

    @Override
    public double getEyeHeight() {
        return 1.62;
    }

    @Override
    public double getEyeHeight(boolean ignorePose) {
        return 1.62;
    }

    @Override
    public Location getEyeLocation() {
        Location here = getLocation();
        here.setY(here.getY() + 1.62);
        return here;
    }

    @Override
    public org.bukkit.util.BoundingBox getBoundingBox() {
        return new org.bukkit.util.BoundingBox(number(kX) - 0.3, number(kY), number(kZ) - 0.3, number(kX) + 0.3,
            number(kY) + 1.8, number(kZ) + 0.3);
    }

    @Override
    public org.bukkit.util.Vector getVelocity() {
        return new org.bukkit.util.Vector();
    }

    @Override
    public boolean isDead() {
        return getHealth() <= 0.0;
    }

    @Override
    public java.util.List<org.bukkit.entity.Entity> getNearbyEntities(double x, double y, double z) {
        return new java.util.ArrayList<>(getWorld().getNearbyEntities(getLocation(), x, y, z,
            entity -> !entity.equals(this)));
    }

    @Override
    public void sendBlockChange(Location location, org.bukkit.block.data.BlockData data) {
        WorldNatives.playerShowBlock(uuid, location.getBlockX(), location.getBlockY(), location.getBlockZ(),
            BasaltBlockData.stateOf(data));
    }

    @Override
    public void sendBlockChange(Location location, org.bukkit.Material material, byte data) {
        sendBlockChange(location, material.createBlockData());
    }

    @Override
    public void sendTitle(String title, String subtitle, int fadeIn, int stay, int fadeOut) {
        WorldNatives.playerTitle(uuid, Text.mini(title == null ? "" : title), Text.mini(subtitle == null ? "" : subtitle),
            fadeIn, stay, fadeOut);
    }

    @Override
    public void sendTitle(String title, String subtitle) {
        sendTitle(title, subtitle, 10, 70, 20);
    }

    @Override
    public void showTitle(net.kyori.adventure.title.Title title) {
        java.time.Duration in = title.times() == null ? java.time.Duration.ofMillis(500) : title.times().fadeIn();
        java.time.Duration stay = title.times() == null ? java.time.Duration.ofMillis(3500) : title.times().stay();
        java.time.Duration out = title.times() == null ? java.time.Duration.ofSeconds(1) : title.times().fadeOut();
        WorldNatives.playerTitle(uuid, Text.mini(title.title()), Text.mini(title.subtitle()),
            (int) (in.toMillis() / 50), (int) (stay.toMillis() / 50), (int) (out.toMillis() / 50));
    }

    @Override
    public void playSound(Location location, String sound, float volume, float pitch) {
        WorldNatives.playerSound(uuid, sound, volume, pitch);
    }

    @Override
    public void setMetadata(String name, org.bukkit.metadata.MetadataValue value) {
        metadata.set(name, value);
    }

    @Override
    public java.util.List<org.bukkit.metadata.MetadataValue> getMetadata(String name) {
        return metadata.get(name);
    }

    @Override
    public boolean hasMetadata(String name) {
        return metadata.has(name);
    }

    @Override
    public void removeMetadata(String name, Plugin plugin) {
        metadata.remove(name, plugin);
    }

    @Override
    public Set<String> getScoreboardTags() {
        return tags;
    }

    @Override
    public boolean addScoreboardTag(String tag) {
        return tags.add(tag);
    }

    @Override
    public boolean removeScoreboardTag(String tag) {
        return tags.remove(tag);
    }






    @Override
    public org.bukkit.block.BlockFace getFacing() {
        double yaw = ((getYaw() % 360) + 360) % 360;
        if (yaw >= 315 || yaw < 45) {
            return org.bukkit.block.BlockFace.SOUTH;
        }
        return yaw < 135 ? org.bukkit.block.BlockFace.WEST
            : yaw < 225 ? org.bukkit.block.BlockFace.NORTH : org.bukkit.block.BlockFace.EAST;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltPlayer player && player.id.equals(id);
    }

    @Override
    public int hashCode() {
        return id.hashCode();
    }

    @Override
    public String toString() {
        return "BasaltPlayer{name=" + name + "}";
    }

    @Override
    public org.bukkit.inventory.InventoryView getOpenInventory() {
        return org.basaltmc.bukkit.inv.Windows.current(this);
    }

    @Override
    public org.bukkit.inventory.InventoryView openInventory(org.bukkit.inventory.Inventory inventory) {
        return org.basaltmc.bukkit.inv.Windows.open(this, inventory);
    }

    @Override
    public void openInventory(org.bukkit.inventory.InventoryView view) {
        org.basaltmc.bukkit.inv.Windows.open(this, (org.basaltmc.bukkit.inv.BasaltInventoryView) view);
    }

    @Override
    public void closeInventory(org.bukkit.event.inventory.InventoryCloseEvent.Reason reason) {
        org.basaltmc.bukkit.inv.Windows.closeCurrent(this);
    }

    @Override
    public org.bukkit.inventory.ItemStack getItemOnCursor() {
        org.bukkit.inventory.ItemStack cursor = Items.fromBytes(InventoryNatives.cursorItem(uuid));
        return cursor == null ? new BasaltItemStack(org.bukkit.Material.AIR, 0) : cursor;
    }

    @Override
    public void setItemOnCursor(org.bukkit.inventory.ItemStack item) {
        InventoryNatives.setCursorItem(uuid, Items.toBytes(item));
    }

    @Override
    public org.bukkit.persistence.PersistentDataContainer getPersistentDataContainer() {
        return org.basaltmc.bukkit.persistence.PdcStores.player(uuid);
    }
}
