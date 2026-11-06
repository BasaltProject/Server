// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import java.util.Date;
import net.minecraft.network.chat.Component;

public class IpBanListEntry extends BanListEntry<String> {
    public IpBanListEntry(String address) {
        this(address, null, null, null, null);
    }

    public IpBanListEntry(String address, Date created, String source, Date expires, String reason) {
        super(address, created, source, expires, reason);
    }

    public IpBanListEntry(JsonObject object) {
        super(object.has("ip") ? object.get("ip").getAsString() : null, object);
    }

    @Override
    protected void serialize(JsonObject object) {
        if (getUser() != null) {
            object.addProperty("ip", getUser());
            super.serialize(object);
        }
    }

    @Override
    public Component getDisplayName() {
        return Component.literal(String.valueOf(getUser()));
    }
}
