package org.basaltmc.bukkit.misc;

import com.destroystokyo.paper.profile.PlayerProfile;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.time.Duration;
import java.time.Instant;
import java.util.Date;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.UUID;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.BanEntry;
import org.bukkit.BanList;
import org.bukkit.ban.IpBanList;
import org.bukkit.ban.ProfileBanList;

public final class BasaltBanList {
    private BasaltBanList() {}

    private static final String kDefaultSource = "(Unknown)";

    private abstract static class Base<T> implements BanList<T> {
        private final boolean ip;

        Base(boolean ip) {
            this.ip = ip;
        }

        abstract String subjectOf(T target);

        abstract String subjectOf(String target);

        abstract T targetOf(String subject, String name);

        @SuppressWarnings("unchecked")
        private <E> E entryFor(String subject) {
            if (subject == null) {
                return null;
            }
            String[] strings = MiscNatives.banOf(subject, ip);
            long[] times = MiscNatives.banTimes(subject, ip);
            if (strings == null || times == null) {
                return null;
            }
            String name = strings[1].isEmpty() ? null : strings[1];
            Date expires = times[1] < 0 ? null : new Date(times[1]);
            if (expires != null && expires.before(new Date())) {
                MiscNatives.pardon(subject, ip);
                return null;
            }
            return (E) new BasaltBanEntry<>(targetOf(subject, name), subject, name, ip, new Date(times[0]),
                strings[3], expires, strings[2]);
        }

        private <E> E add(String subject, String name, String reason, Date expires, String source) {
            if (subject == null) {
                return null;
            }
            long created = System.currentTimeMillis();
            MiscNatives.ban(subject, name == null ? "" : name, reason == null ? "" : reason,
                source == null ? kDefaultSource : source, created, expires == null ? -1 : expires.getTime(), ip);
            return entryFor(subject);
        }

        String nameOf(String subject) {
            return null;
        }

        @Override
        public <E extends BanEntry<? super T>> E getBanEntry(String target) {
            return entryFor(subjectOf(target));
        }

        @Override
        public BanEntry<T> getBanEntry(T target) {
            return entryFor(subjectOf(target));
        }

        @Override
        public <E extends BanEntry<? super T>> E addBan(String target, String reason, Date expires, String source) {
            return add(subjectOf(target), nameOf(target), reason, expires, source);
        }

        @Override
        public BanEntry<T> addBan(T target, String reason, Date expires, String source) {
            return add(subjectOf(target), nameOfTarget(target), reason, expires, source);
        }

        @Override
        public BanEntry<T> addBan(T target, String reason, Instant expires, String source) {
            return addBan(target, reason, expires == null ? null : Date.from(expires), source);
        }

        @Override
        public BanEntry<T> addBan(T target, String reason, Duration duration, String source) {
            return addBan(target, reason, duration == null ? null : Date.from(Instant.now().plus(duration)), source);
        }

        String nameOfTarget(T target) {
            return null;
        }

        @Override
        @SuppressWarnings({"rawtypes", "unchecked"})
        public Set<BanEntry> getBanEntries() {
            Set<BanEntry> out = new LinkedHashSet<>();
            for (String subject : MiscNatives.bans(ip)) {
                BanEntry entry = entryFor(subject);
                if (entry != null) {
                    out.add(entry);
                }
            }
            return out;
        }

        @Override
        @SuppressWarnings("unchecked")
        public <E extends BanEntry<? super T>> Set<E> getEntries() {
            Set<E> out = new LinkedHashSet<>();
            for (String subject : MiscNatives.bans(ip)) {
                E entry = entryFor(subject);
                if (entry != null) {
                    out.add(entry);
                }
            }
            return out;
        }

        @Override
        public boolean isBanned(T target) {
            return entryFor(subjectOf(target)) != null;
        }

        @Override
        public boolean isBanned(String target) {
            return entryFor(subjectOf(target)) != null;
        }

        @Override
        public void pardon(T target) {
            String subject = subjectOf(target);
            if (subject != null) {
                MiscNatives.pardon(subject, ip);
            }
        }

        @Override
        public void pardon(String target) {
            String subject = subjectOf(target);
            if (subject != null) {
                MiscNatives.pardon(subject, ip);
            }
        }
    }

    public static final class Profiles extends Base<PlayerProfile> implements ProfileBanList {
        public Profiles() {
            super(false);
        }

        @Override
        String subjectOf(PlayerProfile target) {
            if (target.getId() != null) {
                return target.getId().toString();
            }
            return target.getName() == null ? null : subjectOf(target.getName());
        }

        @Override
        String subjectOf(String target) {
            if (target == null) {
                return null;
            }
            try {
                return UUID.fromString(target).toString();
            } catch (IllegalArgumentException notUuid) {
                Access.Known known = Access.byName(target);
                return (known != null ? known.id() : Access.offlineUuid(target)).toString();
            }
        }

        @Override
        PlayerProfile targetOf(String subject, String name) {
            UUID id = UUID.fromString(subject);
            if (name == null) {
                Access.Known known = Access.byUuid(id);
                name = known == null ? null : known.name();
            }
            return new BasaltPlayerProfile(id, name);
        }

        @Override
        String nameOf(String target) {
            Access.Known known = Access.byName(target);
            return known != null ? known.name() : target;
        }

        @Override
        String nameOfTarget(PlayerProfile target) {
            return target.getName();
        }

        @Override
        public BanEntry<PlayerProfile> addBan(PlayerProfile target, String reason, Date expires, String source) {
            return super.addBan(target, reason, expires, source);
        }

        @Override
        @SuppressWarnings("unchecked")
        public <E extends BanEntry<? super PlayerProfile>> E addBan(org.bukkit.profile.PlayerProfile target,
                String reason, Date expires, String source) {
            return (E) super.addBan(asPaper(target), reason, expires, source);
        }

        @Override
        @SuppressWarnings("unchecked")
        public <E extends BanEntry<? super PlayerProfile>> E addBan(org.bukkit.profile.PlayerProfile target,
                String reason, Instant expires, String source) {
            return (E) super.addBan(asPaper(target), reason, expires, source);
        }

        @Override
        @SuppressWarnings("unchecked")
        public <E extends BanEntry<? super PlayerProfile>> E addBan(org.bukkit.profile.PlayerProfile target,
                String reason, Duration duration, String source) {
            return (E) super.addBan(asPaper(target), reason, duration, source);
        }

        @Override
        @SuppressWarnings("unchecked")
        public <E extends BanEntry<? super PlayerProfile>> E getBanEntry(org.bukkit.profile.PlayerProfile target) {
            return (E) super.getBanEntry(asPaper(target));
        }

        @Override
        public boolean isBanned(org.bukkit.profile.PlayerProfile target) {
            return super.isBanned(asPaper(target));
        }

        @Override
        public void pardon(org.bukkit.profile.PlayerProfile target) {
            super.pardon(asPaper(target));
        }

        private static PlayerProfile asPaper(org.bukkit.profile.PlayerProfile target) {
            return target instanceof PlayerProfile paper ? paper : new BasaltPlayerProfile(target.getUniqueId(), target.getName());
        }
    }

    public static final class Addresses extends Base<InetAddress> implements IpBanList {
        public Addresses() {
            super(true);
        }

        @Override
        String subjectOf(InetAddress target) {
            return target == null ? null : target.getHostAddress();
        }

        @Override
        String subjectOf(String target) {
            return target;
        }

        @Override
        InetAddress targetOf(String subject, String name) {
            try {
                return InetAddress.getByName(subject);
            } catch (UnknownHostException e) {
                throw new IllegalStateException(e);
            }
        }
    }
}
