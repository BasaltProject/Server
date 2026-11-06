// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import java.io.File;
import net.minecraft.server.notifications.NotificationService;

public class ServerOpList extends StoredUserList<NameAndId, ServerOpListEntry> {
    public ServerOpList(File file, NotificationService notificationService) {
        super(file, notificationService);
    }

    @Override
    protected StoredUserEntry<NameAndId> createEntry(JsonObject object) {
        return new ServerOpListEntry(object);
    }

    @Override
    public String[] getUserList() {
        return getEntries().stream().map(entry -> entry.getUser().name()).toArray(String[]::new);
    }

    @Override
    public boolean add(ServerOpListEntry infos) {
        boolean added = super.add(infos);
        if (added && notificationService != null) {
            notificationService.playerOped(infos);
        }
        return added;
    }

    @Override
    public boolean remove(NameAndId user) {
        ServerOpListEntry entry = get(user);
        boolean removed = super.remove(user);
        if (removed && entry != null && notificationService != null) {
            notificationService.playerDeoped(entry);
        }
        return removed;
    }

    @Override
    public void clear() {
        getEntries().forEach(entry -> remove(entry.getUser()));
    }

    public boolean canBypassPlayerLimit(NameAndId user) {
        ServerOpListEntry entry = get(user);
        return entry != null && entry.getBypassesPlayerLimit();
    }

    @Override
    protected String getKeyForUser(NameAndId user) {
        return user.id().toString();
    }
}
