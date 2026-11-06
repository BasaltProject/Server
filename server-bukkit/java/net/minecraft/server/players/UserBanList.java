// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import java.io.File;
import net.minecraft.server.notifications.NotificationService;

public class UserBanList extends StoredUserList<NameAndId, UserBanListEntry> {
    public UserBanList(File file, NotificationService notificationService) {
        super(file, notificationService);
    }

    @Override
    protected StoredUserEntry<NameAndId> createEntry(JsonObject object) {
        return new UserBanListEntry(object);
    }

    public boolean isBanned(NameAndId user) {
        return contains(user);
    }

    @Override
    public String[] getUserList() {
        return getEntries().stream().map(entry -> entry.getUser().name()).toArray(String[]::new);
    }

    @Override
    protected String getKeyForUser(NameAndId user) {
        return user.id().toString();
    }

    @Override
    public boolean add(UserBanListEntry infos) {
        boolean added = super.add(infos);
        if (added && notificationService != null) {
            notificationService.playerBanned(infos);
        }
        return added;
    }

    @Override
    public boolean remove(NameAndId user) {
        boolean removed = super.remove(user);
        if (removed && notificationService != null) {
            notificationService.playerUnbanned(user);
        }
        return removed;
    }

    @Override
    public void clear() {
        getEntries().forEach(entry -> remove(entry.getUser()));
    }
}
