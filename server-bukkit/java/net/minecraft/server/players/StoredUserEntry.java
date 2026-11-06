// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;

public abstract class StoredUserEntry<T> {
    private final T user;

    public StoredUserEntry(T user) {
        this.user = user;
    }

    public T getUser() {
        return user;
    }

    public boolean hasExpired() {
        return false;
    }

    protected abstract void serialize(JsonObject object);
}
