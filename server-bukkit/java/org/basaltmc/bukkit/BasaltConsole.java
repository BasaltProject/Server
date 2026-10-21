package org.basaltmc.bukkit;

import java.util.Set;
import net.kyori.adventure.text.Component;
import org.bukkit.Server;
import org.bukkit.permissions.Permission;
import org.bukkit.permissions.PermissionAttachment;
import org.bukkit.permissions.PermissionAttachmentInfo;
import org.bukkit.plugin.Plugin;
import org.basaltmc.bukkit.gen.ConsoleCommandSenderBase;

public final class BasaltConsole extends ConsoleCommandSenderBase {
    @Override
    public String getName() {
        return "CONSOLE";
    }

    @Override
    public Component name() {
        return Component.text("CONSOLE");
    }

    @Override
    public Server getServer() {
        return BasaltServer.instance();
    }

    @Override
    public void sendMessage(String message) {
        Natives.log(1, "Server", Text.plain(net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer
            .legacySection().deserialize(message)));
    }

    @Override
    public void sendMessage(String... messages) {
        for (String message : messages) {
            sendMessage(message);
        }
    }

    @Override
    public void sendMessage(java.util.UUID sender, String message) {
        sendMessage(message);
    }

    @Override
    public void sendMessage(java.util.UUID sender, String... messages) {
        sendMessage(messages);
    }

    @Override
    public void sendMessage(Component message) {
        Natives.log(1, "Server", Text.plain(message));
    }

    @Override
    public boolean isOp() {
        return true;
    }

    @Override
    public void setOp(boolean value) {}

    @Override
    public boolean isPermissionSet(String name) {
        return true;
    }

    @Override
    public boolean isPermissionSet(Permission permission) {
        return true;
    }

    @Override
    public boolean hasPermission(String name) {
        return true;
    }

    @Override
    public boolean hasPermission(Permission permission) {
        return true;
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, String name, boolean value) {
        return new PermissionAttachment(plugin, this);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin) {
        return new PermissionAttachment(plugin, this);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, String name, boolean value, int ticks) {
        return new PermissionAttachment(plugin, this);
    }

    @Override
    public PermissionAttachment addAttachment(Plugin plugin, int ticks) {
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
    public org.bukkit.command.CommandSender.Spigot spigot() {
        return SpigotText.console(this::sendMessage);
    }

    @Override
    public void sendRawMessage(String message) {
        sendMessage(message);
    }

    @Override
    public void sendRawMessage(java.util.UUID sender, String message) {
        sendMessage(message);
    }
}
