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

    protected BasaltPlayer(UUID id, String name) {
        this.id = id;
        this.uuid = id.toString();
        this.name = name;
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
    public double getHealth() {
        return number(kHealth);
    }

    @Override
    public void setHealth(double health) {
        Natives.playerSetHealth(uuid, health);
    }

    @Override
    public double getMaxHealth() {
        return 20.0;
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
        throw Unsupported.hit("Player#setOp");
    }

    @Override
    public boolean isPermissionSet(String permission) {
        return hasPermission(permission);
    }

    @Override
    public boolean isPermissionSet(Permission permission) {
        return hasPermission(permission.getName());
    }

    @Override
    public boolean hasPermission(String permission) {
        if (Natives.playerMay(uuid, permission)) {
            return true;
        }
        return BasaltServer.instance().pluginManagerImpl().grantedByDefault(permission, isOp());
    }

    @Override
    public boolean hasPermission(Permission permission) {
        return hasPermission(permission.getName());
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin) {
        return new PermissionAttachment(plugin, this);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, int ticks) {
        return new PermissionAttachment(plugin, this);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, String name, boolean value) {
        return new PermissionAttachment(plugin, this);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, String name, boolean value, int ticks) {
        return new PermissionAttachment(plugin, this);
    }

    @Override
    public void removeAttachment(PermissionAttachment attachment) {}

    @Override
    public void recalculatePermissions() {}

    @Override
    public Set<PermissionAttachmentInfo> getEffectivePermissions() {
        return Set.of();
    }

    @Override
    public boolean isConversing() {
        return false;
    }

    @Override
    public org.bukkit.entity.Player.Spigot spigot() {
        return new org.bukkit.entity.Player.Spigot();
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
        return WorldNatives.playerCommand(uuid, command);
    }

    @Override
    public void updateInventory() {}

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
    public org.bukkit.entity.Entity getVehicle() {
        return null;
    }

    @Override
    public boolean isInsideVehicle() {
        return false;
    }

    @Override
    public java.util.List<org.bukkit.entity.Entity> getPassengers() {
        return new java.util.ArrayList<>();
    }

    @Override
    public boolean isPersistent() {
        return true;
    }

    @Override
    public int getTicksLived() {
        return 0;
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
}
