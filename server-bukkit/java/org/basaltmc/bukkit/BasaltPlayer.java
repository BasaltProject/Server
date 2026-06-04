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

public final class BasaltPlayer extends PlayerBase {
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

    BasaltPlayer(UUID id, String name) {
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
        return new Location(getWorld(), number(kX), number(kY), number(kZ));
    }

    @Override
    public Location getLocation(Location into) {
        if (into != null) {
            into.setWorld(getWorld());
            into.setX(number(kX));
            into.setY(number(kY));
            into.setZ(number(kZ));
        }
        return into;
    }

    @Override
    public boolean teleport(Location location) {
        World world = location.getWorld() == null ? getWorld() : location.getWorld();
        return Natives.playerTeleport(uuid, world.getName(), location.getX(), location.getY(), location.getZ());
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
        return Natives.playerMay(uuid, "*");
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
        return null;
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
