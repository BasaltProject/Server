// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import java.io.File;
import net.minecraft.server.notifications.NotificationService;

public class UserWhiteList extends StoredUserList<NameAndId, UserWhiteListEntry> {
    public UserWhiteList(File file, NotificationService notificationService) {
        super(file, notificationService);
    }

    @Override
    protected StoredUserEntry<NameAndId> createEntry(JsonObject object) {
        return new UserWhiteListEntry(object);
    }

    public boolean isWhiteListed(NameAndId user) {
        return contains(user);
    }

    @Override
    public boolean add(UserWhiteListEntry infos) {
        boolean added = super.add(infos);
        if (added && notificationService != null) {
            notificationService.playerAddedToAllowlist(infos.getUser());
        }
        return added;
    }

    @Override
    public boolean remove(NameAndId user) {
        boolean removed = super.remove(user);
        if (removed && notificationService != null) {
            notificationService.playerRemovedFromAllowlist(user);
        }
        return removed;
    }

    @Override
    public void clear() {
        getEntries().forEach(entry -> remove(entry.getUser()));
    }

    @Override
    public String[] getUserList() {
        return getEntries().stream().map(entry -> entry.getUser().name()).toArray(String[]::new);
    }

    @Override
    protected String getKeyForUser(NameAndId user) {
        return user.id().toString();
    }
}
