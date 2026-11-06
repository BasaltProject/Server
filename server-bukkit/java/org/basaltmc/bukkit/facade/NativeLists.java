package org.basaltmc.bukkit.facade;

import java.io.File;
import java.util.Date;
import java.util.UUID;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.server.players.IpBanList;
import net.minecraft.server.players.IpBanListEntry;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.ServerOpList;
import net.minecraft.server.players.ServerOpListEntry;
import net.minecraft.server.players.UserBanList;
import net.minecraft.server.players.UserBanListEntry;
import net.minecraft.server.players.UserWhiteList;
import net.minecraft.server.players.UserWhiteListEntry;
import org.basaltmc.bukkit.MiscNatives;
import org.basaltmc.bukkit.misc.Access;

/** Ban, allowlist and operator lists whose contents live in the native runtime and are read on every call. */
public final class NativeLists {
    private static final File kNone = new File("");

    private NativeLists() {}

    private static Date dateOf(long time) {
        return time < 0 ? null : new Date(time);
    }

    private static void refreshBans(UserBanList list) {
        list.clear0();
        for (String subject : MiscNatives.bans(false)) {
            String[] fields = MiscNatives.banOf(subject, false);
            long[] times = MiscNatives.banTimes(subject, false);
            if (fields == null || times == null) {
                continue;
            }
            list.add0(new UserBanListEntry(new NameAndId(UUID.fromString(subject), fields[1]), new Date(times[0]), fields[3], dateOf(times[1]), fields[2]));
        }
    }

    public static UserBanList userBans() {
        return new UserBanList(kNone, null) {
            @Override
            public java.util.Collection<UserBanListEntry> getEntries() {
                refreshBans(this);
                return super.getEntries();
            }

            @Override
            public UserBanListEntry get(NameAndId user) {
                refreshBans(this);
                return super.get(user);
            }

            @Override
            public boolean isBanned(NameAndId user) {
                return MiscNatives.banOf(user.id().toString(), false) != null;
            }

            @Override
            public boolean add(UserBanListEntry entry) {
                MiscNatives.ban(entry.getUser().id().toString(), entry.getUser().name(), entry.getReason(), entry.getSource(),
                    entry.getCreated().getTime(), entry.getExpires() == null ? -1 : entry.getExpires().getTime(), false);
                return true;
            }

            @Override
            public boolean remove(NameAndId user) {
                return MiscNatives.pardon(user.id().toString(), false);
            }
        };
    }

    public static IpBanList ipBans() {
        return new IpBanList(kNone, null) {
            private void refresh() {
                clear0();
                for (String subject : MiscNatives.bans(true)) {
                    String[] fields = MiscNatives.banOf(subject, true);
                    long[] times = MiscNatives.banTimes(subject, true);
                    if (fields != null && times != null) {
                        add0(new IpBanListEntry(subject, new Date(times[0]), fields[3], dateOf(times[1]), fields[2]));
                    }
                }
            }

            @Override
            public java.util.Collection<IpBanListEntry> getEntries() {
                refresh();
                return super.getEntries();
            }

            @Override
            public IpBanListEntry get(String ip) {
                refresh();
                return super.get(ip);
            }

            @Override
            public boolean isBanned(String ip) {
                return MiscNatives.banOf(ip, true) != null;
            }

            @Override
            public boolean add(IpBanListEntry entry) {
                MiscNatives.ban(entry.getUser(), "", entry.getReason(), entry.getSource(), entry.getCreated().getTime(),
                    entry.getExpires() == null ? -1 : entry.getExpires().getTime(), true);
                return true;
            }

            @Override
            public boolean remove(String ip) {
                return MiscNatives.pardon(ip, true);
            }
        };
    }

    public static UserWhiteList whitelist() {
        return new UserWhiteList(kNone, null) {
            private void refresh() {
                clear0();
                for (Access.Known known : Access.whitelisted()) {
                    add0(new UserWhiteListEntry(new NameAndId(known.id(), known.name())));
                }
            }

            @Override
            public java.util.Collection<UserWhiteListEntry> getEntries() {
                refresh();
                return super.getEntries();
            }

            @Override
            public boolean isWhiteListed(NameAndId user) {
                return MiscNatives.isWhitelisted(user.id().toString());
            }

            @Override
            public boolean add(UserWhiteListEntry entry) {
                MiscNatives.setWhitelisted(entry.getUser().id().toString(), entry.getUser().name(), true);
                return true;
            }

            @Override
            public boolean remove(NameAndId user) {
                boolean was = MiscNatives.isWhitelisted(user.id().toString());
                MiscNatives.setWhitelisted(user.id().toString(), user.name(), false);
                return was;
            }
        };
    }

    public static ServerOpList operators() {
        return new ServerOpList(kNone, null) {
            private void refresh() {
                clear0();
                for (Access.Known known : Access.operators()) {
                    add0(new ServerOpListEntry(new NameAndId(known.id(), known.name()), LevelBasedPermissionSet.OWNER, false));
                }
            }

            @Override
            public java.util.Collection<ServerOpListEntry> getEntries() {
                refresh();
                return super.getEntries();
            }

            @Override
            public ServerOpListEntry get(NameAndId user) {
                refresh();
                return super.get(user);
            }

            @Override
            public boolean add(ServerOpListEntry entry) {
                return MiscNatives.setOperator(entry.getUser().id().toString(), true);
            }

            @Override
            public boolean remove(NameAndId user) {
                return MiscNatives.setOperator(user.id().toString(), false);
            }
        };
    }
}
