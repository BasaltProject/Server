package org.basaltmc.bukkit.misc;

import java.util.Date;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.BanEntry;

public final class BasaltBanEntry<T> implements BanEntry<T> {
    private final T target;
    private final String subject;
    private final String name;
    private final boolean ip;
    private Date created;
    private String source;
    private Date expires;
    private String reason;

    BasaltBanEntry(T target, String subject, String name, boolean ip, Date created, String source, Date expires,
            String reason) {
        this.target = target;
        this.subject = subject;
        this.name = name;
        this.ip = ip;
        this.created = created;
        this.source = source;
        this.expires = expires;
        this.reason = reason;
    }

    @Override
    public String getTarget() {
        return ip || name == null ? subject : name;
    }

    @Override
    public T getBanTarget() {
        return target;
    }

    @Override
    public Date getCreated() {
        return created;
    }

    @Override
    public void setCreated(Date created) {
        this.created = created;
    }

    @Override
    public String getSource() {
        return source;
    }

    @Override
    public void setSource(String source) {
        this.source = source;
    }

    @Override
    public Date getExpiration() {
        return expires;
    }

    @Override
    public void setExpiration(Date expiration) {
        this.expires = expiration;
    }

    @Override
    public String getReason() {
        return reason;
    }

    @Override
    public void setReason(String reason) {
        this.reason = reason;
    }

    @Override
    public void save() {
        MiscNatives.ban(subject, name == null ? "" : name, reason == null ? "" : reason,
            source == null ? "" : source, created.getTime(), expires == null ? -1 : expires.getTime(), ip);
    }

    @Override
    public void remove() {
        MiscNatives.pardon(subject, ip);
    }
}
