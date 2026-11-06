// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.JsonObject;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Objects;
import net.minecraft.network.chat.Component;

public abstract class BanListEntry<T> extends StoredUserEntry<T> {
    public static final SimpleDateFormat DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss Z");
    public static final String EXPIRES_NEVER = "forever";
    protected final Date created;
    protected final String source;
    protected final Date expires;
    protected final String reason;

    public BanListEntry(T user, Date created, String source, Date expires, String reason) {
        super(user);
        this.created = created == null ? new Date() : created;
        this.source = source == null ? "(Unknown)" : source;
        this.expires = expires;
        this.reason = reason == null ? "Banned by an operator." : reason;
    }

    protected BanListEntry(T user, JsonObject object) {
        super(user);
        this.created = readDate(object, "created", new Date());
        this.source = object.has("source") ? object.get("source").getAsString() : "(Unknown)";
        this.expires = readDate(object, "expires", null);
        this.reason = object.has("reason") ? object.get("reason").getAsString() : "Banned by an operator.";
    }

    private static Date readDate(JsonObject object, String key, Date fallback) {
        try {
            return object.has(key) ? DATE_FORMAT.parse(object.get(key).getAsString()) : fallback;
        } catch (ParseException invalid) {
            return fallback;
        }
    }

    public Date getCreated() {
        return created;
    }

    public String getSource() {
        return source;
    }

    public Date getExpires() {
        return expires;
    }

    public String getReason() {
        return reason;
    }

    public Component getReasonMessage() {
        return Component.literal(reason);
    }

    public abstract Component getDisplayName();

    @Override
    public boolean hasExpired() {
        return expires != null && expires.before(new Date());
    }

    @Override
    protected void serialize(JsonObject object) {
        object.addProperty("created", DATE_FORMAT.format(created));
        object.addProperty("source", source);
        object.addProperty("expires", expires == null ? EXPIRES_NEVER : DATE_FORMAT.format(expires));
        object.addProperty("reason", reason);
    }

    @Override
    public boolean equals(Object o) {
        return o instanceof BanListEntry<?> other && Objects.equals(getUser(), other.getUser())
            && Objects.equals(created, other.created) && Objects.equals(source, other.source)
            && Objects.equals(expires, other.expires) && Objects.equals(reason, other.reason);
    }

    @Override
    public int hashCode() {
        return Objects.hash(getUser(), created, source, expires, reason);
    }
}
