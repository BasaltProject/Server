// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import java.io.File;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import net.minecraft.server.notifications.NotificationService;

public class IpBanList extends StoredUserList<String, IpBanListEntry> {
    public IpBanList(File file, NotificationService notificationService) {
        super(file, notificationService);
    }

    @Override
    protected StoredUserEntry<String> createEntry(JsonObject object) {
        return new IpBanListEntry(object);
    }

    private static String addressOf(SocketAddress address) {
        String text = address.toString();
        if (address instanceof InetSocketAddress inet && inet.getAddress() != null) {
            return inet.getAddress().getHostAddress();
        }
        if (text.contains("/")) {
            text = text.substring(text.indexOf('/') + 1);
        }
        if (text.contains(":")) {
            text = text.substring(0, text.indexOf(':'));
        }
        return text;
    }

    public boolean isBanned(SocketAddress address) {
        return contains(addressOf(address));
    }

    public boolean isBanned(String ip) {
        return contains(ip);
    }

    public IpBanListEntry get(SocketAddress address) {
        return get(addressOf(address));
    }

    @Override
    public boolean add(IpBanListEntry infos) {
        boolean added = super.add(infos);
        if (added && notificationService != null) {
            notificationService.ipBanned(infos);
        }
        return added;
    }

    @Override
    public boolean remove(String ip) {
        boolean removed = super.remove(ip);
        if (removed && notificationService != null) {
            notificationService.ipUnbanned(ip);
        }
        return removed;
    }

    @Override
    public void clear() {
        getEntries().forEach(entry -> remove(entry.getUser()));
    }
}
