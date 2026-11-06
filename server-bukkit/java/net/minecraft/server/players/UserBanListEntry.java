// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import java.util.Date;
import net.minecraft.network.chat.Component;

public class UserBanListEntry extends BanListEntry<NameAndId> {
    public UserBanListEntry(NameAndId user) {
        this(user, null, null, null, null);
    }

    public UserBanListEntry(NameAndId user, Date created, String source, Date expires, String reason) {
        super(user, created, source, expires, reason);
    }

    public UserBanListEntry(JsonObject object) {
        super(NameAndId.fromJson(object), object);
    }

    @Override
    protected void serialize(JsonObject object) {
        if (getUser() != null) {
            getUser().appendTo(object);
            super.serialize(object);
        }
    }

    @Override
    public Component getDisplayName() {
        return Component.literal(getUser() == null ? "(unknown)" : getUser().name());
    }
}
