package org.basaltmc.bukkit.player;

import com.destroystokyo.paper.ClientOption;
import io.papermc.paper.connection.PlayerGameConnection;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.NamespacedKey;
import org.bukkit.ServerLinks;
import org.bukkit.entity.Player;
import org.bukkit.plugin.Plugin;

/** The connection of a player in the game, answered by the player it belongs to. */
public final class BasaltConnection implements PlayerGameConnection {
    private final Player player;

    public BasaltConnection(Player player) {
        this.player = player;
    }

    @Override
    public Player getPlayer() {
        return player;
    }

    @Override
    public void reenterConfiguration() {
        throw Unsupported.hit("PlayerGameConnection#reenterConfiguration");
    }

    @Override
    public void sendReportDetails(Map<String, String> details) {
        if (details.size() > 32) {
            throw new IllegalArgumentException("A report holds at most 32 details");
        }
        java.util.List<Object> flat = new java.util.ArrayList<>();
        for (Map.Entry<String, String> entry : details.entrySet()) {
            if (entry.getKey().length() > 128 || entry.getValue().length() > 4096) {
                throw new IllegalArgumentException("A report detail is too long");
            }
            flat.add(entry.getKey());
            flat.add(entry.getValue());
        }
        PlayerCalls.call(player.getUniqueId(), "report_details", flat.toArray());
    }

    @Override
    public void sendLinks(ServerLinks links) {
        player.sendLinks(links);
    }

    @Override
    public void transfer(String host, int port) {
        player.transfer(host, port);
    }

    @Override
    public <T> T getClientOption(ClientOption<T> option) {
        return player.getClientOption(option);
    }

    @Override
    public String getClientBrandName() {
        return player.getClientBrandName();
    }

    @Override
    public void disconnect(Component reason) {
        player.kick(reason);
    }

    @Override
    public boolean isConnected() {
        return player.isOnline();
    }

    @Override
    public boolean isTransferred() {
        return player.isTransferred();
    }

    @Override
    public SocketAddress getAddress() {
        return player.getAddress();
    }

    @Override
    public InetSocketAddress getClientAddress() {
        return player.getAddress();
    }

    @Override
    public InetSocketAddress getVirtualHost() {
        return player.getVirtualHost();
    }

    @Override
    public InetSocketAddress getHAProxyAddress() {
        return player.getHAProxyAddress();
    }

    @Override
    public CompletableFuture<byte[]> retrieveCookie(NamespacedKey key) {
        return player.retrieveCookie(key);
    }

    @Override
    public void storeCookie(NamespacedKey key, byte[] value) {
        player.storeCookie(key, value);
    }

    @Override
    public void sendPluginMessage(Plugin source, String channel, byte[] message) {
        player.sendPluginMessage(source, channel, message);
    }

    @Override
    public Set<String> getListeningPluginChannels() {
        return player.getListeningPluginChannels();
    }
}
