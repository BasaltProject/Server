package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import net.kyori.adventure.audience.Audience;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.NamedTextColor;
import net.kyori.adventure.text.format.TextColor;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.Bukkit;
import org.bukkit.ChatColor;
import org.bukkit.OfflinePlayer;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.scoreboard.NameTagVisibility;
import org.bukkit.scoreboard.Scoreboard;
import org.bukkit.scoreboard.Team;

public final class BasaltTeam implements Team {
    private static final int kName = 0;
    private static final int kDisplay = 1;
    private static final int kPrefix = 2;
    private static final int kSuffix = 3;
    private static final int kColor = 4;
    private static final int kFriendlyFire = 5;
    private static final int kSeeInvisible = 6;
    private static final int kNameTag = 7;
    private static final int kDeath = 8;
    private static final int kCollision = 9;
    private static final int kMembers = 10;

    private final BasaltScoreboard board;
    private final String name;

    BasaltTeam(BasaltScoreboard board, String name) {
        this.board = board;
        this.name = name;
    }

    private String[] fields() {
        String[] fields = MiscNatives.sbTeam(board.handle(), name);
        if (fields == null) {
            throw new IllegalStateException("Unregistered scoreboard component");
        }
        return fields;
    }

    private void write(String[] fields) {
        MiscNatives.sbUpdateTeam(board.handle(), fields);
    }

    private static String status(OptionStatus status, boolean collision) {
        return switch (status) {
            case ALWAYS -> "always";
            case NEVER -> "never";
            case FOR_OTHER_TEAMS -> collision ? "pushOtherTeams" : "hideForOtherTeams";
            case FOR_OWN_TEAM -> collision ? "pushOwnTeam" : "hideForOwnTeam";
        };
    }

    private static OptionStatus status(String text) {
        return switch (text) {
            case "never" -> OptionStatus.NEVER;
            case "hideForOtherTeams", "pushOtherTeams" -> OptionStatus.FOR_OTHER_TEAMS;
            case "hideForOwnTeam", "pushOwnTeam" -> OptionStatus.FOR_OWN_TEAM;
            default -> OptionStatus.ALWAYS;
        };
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public Component displayName() {
        return Json.component(fields()[kDisplay]);
    }

    @Override
    public void displayName(Component displayName) {
        String[] fields = fields();
        fields[kDisplay] = Json.of(displayName == null ? Component.empty() : displayName);
        write(fields);
    }

    @Override
    public Component prefix() {
        return Json.component(fields()[kPrefix]);
    }

    @Override
    public void prefix(Component prefix) {
        String[] fields = fields();
        fields[kPrefix] = Json.of(prefix == null ? Component.empty() : prefix);
        write(fields);
    }

    @Override
    public Component suffix() {
        return Json.component(fields()[kSuffix]);
    }

    @Override
    public void suffix(Component suffix) {
        String[] fields = fields();
        fields[kSuffix] = Json.of(suffix == null ? Component.empty() : suffix);
        write(fields);
    }

    @Override
    public boolean hasColor() {
        return !fields()[kColor].isEmpty();
    }

    @Override
    public TextColor color() {
        String color = fields()[kColor];
        if (color.isEmpty()) {
            throw new IllegalStateException("Team does not have a color");
        }
        return NamedTextColor.NAMES.value(color);
    }

    @Override
    public void color(NamedTextColor color) {
        String[] fields = fields();
        fields[kColor] = color == null ? "" : NamedTextColor.NAMES.key(color);
        write(fields);
    }

    @Override
    public String getDisplayName() {
        return Json.legacyOf(displayName());
    }

    @Override
    public void setDisplayName(String displayName) {
        if (displayName == null) {
            throw new IllegalArgumentException("Display name cannot be null");
        }
        displayName(Json.fromLegacy(displayName));
    }

    @Override
    public String getPrefix() {
        return Json.legacyOf(prefix());
    }

    @Override
    public void setPrefix(String prefix) {
        if (prefix == null) {
            throw new IllegalArgumentException("Prefix cannot be null");
        }
        prefix(Json.fromLegacy(prefix));
    }

    @Override
    public String getSuffix() {
        return Json.legacyOf(suffix());
    }

    @Override
    public void setSuffix(String suffix) {
        if (suffix == null) {
            throw new IllegalArgumentException("Suffix cannot be null");
        }
        suffix(Json.fromLegacy(suffix));
    }

    @Override
    public ChatColor getColor() {
        String color = fields()[kColor];
        return color.isEmpty() ? ChatColor.RESET : ChatColor.valueOf(color.toUpperCase(Locale.ROOT));
    }

    @Override
    public void setColor(ChatColor color) {
        if (color == null) {
            throw new IllegalArgumentException("Color cannot be null");
        }
        String[] fields = fields();
        if (color == ChatColor.RESET) {
            fields[kColor] = "";
        } else if (color.isColor()) {
            fields[kColor] = color.name().toLowerCase(Locale.ROOT);
        } else {
            throw new IllegalArgumentException("Color must be a color, not a format");
        }
        write(fields);
    }

    @Override
    public boolean allowFriendlyFire() {
        return "1".equals(fields()[kFriendlyFire]);
    }

    @Override
    public void setAllowFriendlyFire(boolean enabled) {
        String[] fields = fields();
        fields[kFriendlyFire] = enabled ? "1" : "0";
        write(fields);
    }

    @Override
    public boolean canSeeFriendlyInvisibles() {
        return "1".equals(fields()[kSeeInvisible]);
    }

    @Override
    public void setCanSeeFriendlyInvisibles(boolean enabled) {
        String[] fields = fields();
        fields[kSeeInvisible] = enabled ? "1" : "0";
        write(fields);
    }

    @Override
    public NameTagVisibility getNameTagVisibility() {
        return switch (fields()[kNameTag]) {
            case "never" -> NameTagVisibility.NEVER;
            case "hideForOtherTeams" -> NameTagVisibility.HIDE_FOR_OTHER_TEAMS;
            case "hideForOwnTeam" -> NameTagVisibility.HIDE_FOR_OWN_TEAM;
            default -> NameTagVisibility.ALWAYS;
        };
    }

    @Override
    public void setNameTagVisibility(NameTagVisibility visibility) {
        if (visibility == null) {
            throw new IllegalArgumentException("Visibility cannot be null");
        }
        String[] fields = fields();
        fields[kNameTag] = switch (visibility) {
            case NEVER -> "never";
            case HIDE_FOR_OTHER_TEAMS -> "hideForOtherTeams";
            case HIDE_FOR_OWN_TEAM -> "hideForOwnTeam";
            default -> "always";
        };
        write(fields);
    }

    private List<String> members() {
        String[] fields = fields();
        List<String> out = new ArrayList<>(fields.length - kMembers);
        for (int i = kMembers; i < fields.length; i++) {
            out.add(fields[i]);
        }
        return out;
    }

    @Override
    public Set<OfflinePlayer> getPlayers() {
        Set<OfflinePlayer> out = new LinkedHashSet<>();
        for (String member : members()) {
            out.add(BasaltScoreboard.playerOf(member));
        }
        return out;
    }

    @Override
    public Set<String> getEntries() {
        return new LinkedHashSet<>(members());
    }

    @Override
    public int getSize() {
        return fields().length - kMembers;
    }

    @Override
    public Scoreboard getScoreboard() {
        fields();
        return board;
    }

    @Override
    public void addPlayer(OfflinePlayer player) {
        addEntry(BasaltScoreboard.entryOf(player));
    }

    @Override
    public void addEntry(String entry) {
        if (entry == null) {
            throw new IllegalArgumentException("Entry cannot be null");
        }
        fields();
        MiscNatives.sbAddMember(board.handle(), name, entry);
    }

    @Override
    public void addEntities(Collection<Entity> entities) {
        for (Entity entity : entities) {
            addEntry(BasaltScoreboard.entryOf(entity));
        }
    }

    @Override
    public void addEntries(Collection<String> entries) {
        for (String entry : entries) {
            addEntry(entry);
        }
    }

    @Override
    public boolean removePlayer(OfflinePlayer player) {
        return removeEntry(BasaltScoreboard.entryOf(player));
    }

    @Override
    public boolean removeEntry(String entry) {
        if (entry == null) {
            throw new IllegalArgumentException("Entry cannot be null");
        }
        fields();
        return MiscNatives.sbRemoveMember(board.handle(), name, entry);
    }

    @Override
    public boolean removeEntities(Collection<Entity> entities) {
        boolean any = false;
        for (Entity entity : entities) {
            any |= removeEntry(BasaltScoreboard.entryOf(entity));
        }
        return any;
    }

    @Override
    public boolean removeEntries(Collection<String> entries) {
        boolean any = false;
        for (String entry : entries) {
            any |= removeEntry(entry);
        }
        return any;
    }

    @Override
    public void unregister() {
        fields();
        MiscNatives.sbRemoveTeam(board.handle(), name);
    }

    @Override
    public boolean hasPlayer(OfflinePlayer player) {
        return hasEntry(BasaltScoreboard.entryOf(player));
    }

    @Override
    public boolean hasEntry(String entry) {
        if (entry == null) {
            throw new IllegalArgumentException("Entry cannot be null");
        }
        return members().contains(entry);
    }

    @Override
    public OptionStatus getOption(Option option) {
        if (option == null) {
            throw new IllegalArgumentException("Option cannot be null");
        }
        String[] fields = fields();
        return switch (option) {
            case NAME_TAG_VISIBILITY -> status(fields[kNameTag]);
            case DEATH_MESSAGE_VISIBILITY -> status(fields[kDeath]);
            case COLLISION_RULE -> status(fields[kCollision]);
        };
    }

    @Override
    public void setOption(Option option, OptionStatus status) {
        if (option == null) {
            throw new IllegalArgumentException("Option cannot be null");
        }
        if (status == null) {
            throw new IllegalArgumentException("OptionStatus cannot be null");
        }
        String[] fields = fields();
        switch (option) {
            case NAME_TAG_VISIBILITY -> fields[kNameTag] = status(status, false);
            case DEATH_MESSAGE_VISIBILITY -> fields[kDeath] = status(status, false);
            case COLLISION_RULE -> fields[kCollision] = status(status, true);
        }
        write(fields);
    }

    @Override
    public void addEntity(Entity entity) {
        addEntry(BasaltScoreboard.entryOf(entity));
    }

    @Override
    public boolean removeEntity(Entity entity) {
        return removeEntry(BasaltScoreboard.entryOf(entity));
    }

    @Override
    public boolean hasEntity(Entity entity) {
        return hasEntry(BasaltScoreboard.entryOf(entity));
    }

    @Override
    public Iterable<? extends Audience> audiences() {
        List<Audience> out = new ArrayList<>();
        for (String member : members()) {
            Player online = Bukkit.getPlayerExact(member);
            if (online != null) {
                out.add(online);
            }
        }
        return out;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltTeam team && team.board.equals(board) && team.name.equals(name);
    }

    @Override
    public int hashCode() {
        return name.hashCode() * 31 + board.hashCode();
    }
}
