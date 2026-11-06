// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.server.permissions.PermissionLevel;

public class ServerOpListEntry extends StoredUserEntry<NameAndId> {
    private final LevelBasedPermissionSet permissions;
    private final boolean bypassesPlayerLimit;

    public ServerOpListEntry(NameAndId user, LevelBasedPermissionSet permissions, boolean bypassesPlayerLimit) {
        super(user);
        this.permissions = permissions;
        this.bypassesPlayerLimit = bypassesPlayerLimit;
    }

    public ServerOpListEntry(JsonObject object) {
        super(NameAndId.fromJson(object));
        this.permissions = LevelBasedPermissionSet.forLevel(PermissionLevel.byId(object.has("level") ? object.get("level").getAsInt() : 4));
        this.bypassesPlayerLimit = object.has("bypassesPlayerLimit") && object.get("bypassesPlayerLimit").getAsBoolean();
    }

    public LevelBasedPermissionSet permissions() {
        return permissions;
    }

    public boolean getBypassesPlayerLimit() {
        return bypassesPlayerLimit;
    }

    @Override
    protected void serialize(JsonObject object) {
        if (getUser() != null) {
            getUser().appendTo(object);
            object.addProperty("level", permissions.level().id());
            object.addProperty("bypassesPlayerLimit", bypassesPlayerLimit);
        }
    }
}
