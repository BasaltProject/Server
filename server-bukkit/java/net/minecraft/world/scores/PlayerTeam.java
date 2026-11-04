// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;
import net.minecraft.network.chat.Component;
import com.mojang.serialization.Codec;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.chat.MutableComponent;
public class PlayerTeam extends Team {
    private final String name;
    private Component displayName;
    private Component playerPrefix = Component.empty();
    private Component playerSuffix = Component.empty();
    private final Scoreboard scoreboard;
    private final java.util.Set<String> players = new java.util.LinkedHashSet<>();
    private boolean friendlyFire = true, seeFriendlyInvisibles = true;
    private Team.Visibility nameTagVisibility = Team.Visibility.ALWAYS;
    private Team.Visibility deathMessageVisibility = Team.Visibility.ALWAYS;
    private Team.CollisionRule collisionRule = Team.CollisionRule.ALWAYS;
    private java.util.Optional<TeamColor> color = java.util.Optional.empty();
    public Team.Visibility getNameTagVisibility() {
        return nameTagVisibility;
    }
    public void setNameTagVisibility(Team.Visibility value) {
        nameTagVisibility = java.util.Objects.requireNonNull(value);
    }
    public Team.CollisionRule getCollisionRule() {
        return collisionRule;
    }
    public void setCollisionRule(Team.CollisionRule value) {
        collisionRule = java.util.Objects.requireNonNull(value);
    }
    public java.util.Optional<TeamColor> getColor() {
        return color;
    }
    public void setColor(java.util.Optional<TeamColor> value) {
        color = java.util.Objects.requireNonNull(value);
    }
    public void setColor(net.minecraft.ChatFormatting value) {
        java.util.Objects.requireNonNull(value);
        if (value != net.minecraft.ChatFormatting.RESET && !value.isColor())
            throw new IllegalArgumentException("Not a team color");
        color = value.isColor() ? java.util.Optional.of(TeamColor.values()[value.getId()]) : java.util.Optional.empty();
    }
    public PlayerTeam(Scoreboard scoreboard, String name) {
        this.scoreboard = scoreboard;
        this.name = java.util.Objects.requireNonNull(name);
        displayName = Component.literal(name);
    }
    public String getName() {
        return name;
    }
    public Component getDisplayName() {
        return displayName;
    }
    public void setDisplayName(Component value) {
        displayName = java.util.Objects.requireNonNull(value);
    }
    public Component getPlayerPrefix() {
        return playerPrefix;
    }
    public void setPlayerPrefix(Component value) {
        playerPrefix = java.util.Objects.requireNonNull(value);
    }
    public Component getPlayerSuffix() {
        return playerSuffix;
    }
    public void setPlayerSuffix(Component value) {
        playerSuffix = java.util.Objects.requireNonNull(value);
    }
    public java.util.Collection<String> getPlayers() {
        return players;
    }
    public boolean isAllowFriendlyFire() {
        return friendlyFire;
    }
    public void setAllowFriendlyFire(boolean value) {
        friendlyFire = value;
    }
    public boolean canSeeFriendlyInvisibles() {
        return seeFriendlyInvisibles;
    }
    public void setSeeFriendlyInvisibles(boolean value) {
        seeFriendlyInvisibles = value;
    }
    public int packOptions() {
        return (friendlyFire ? 1 : 0) | (seeFriendlyInvisibles ? 2 : 0);
    }
    public Packed pack() {
        return new Packed(name, Optional.of(displayName), color, friendlyFire, seeFriendlyInvisibles, playerPrefix,
            playerSuffix, nameTagVisibility, deathMessageVisibility, collisionRule, List.copyOf(players));
    }
    public Scoreboard getScoreboard() {
        return scoreboard;
    }
    public MutableComponent getFormattedDisplayName() {
        return Component.literal("").append(displayName);
    }
    public MutableComponent getFormattedName(Component teamMemberName) {
        return Component.literal("").append(playerPrefix).append(teamMemberName).append(playerSuffix);
    }
    public static MutableComponent formatNameForTeam(Team team, Component name) {
        return team == null ? Component.literal("").append(name) : team.getFormattedName(name);
    }
    public Team.Visibility getDeathMessageVisibility() {
        return deathMessageVisibility;
    }
    public void setDeathMessageVisibility(Team.Visibility visibility) {
        this.deathMessageVisibility = java.util.Objects.requireNonNull(visibility);
    }
    public void unpackOptions(byte options) {
        friendlyFire = (options & 1) != 0;
        seeFriendlyInvisibles = (options & 2) != 0;
    }
    public static record Packed(String name, Optional<Component> displayName, Optional<TeamColor> color, boolean allowFriendlyFire, boolean seeFriendlyInvisibles, Component memberNamePrefix, Component memberNameSuffix, Team.Visibility nameTagVisibility, Team.Visibility deathMessageVisibility, Team.CollisionRule collisionRule, List<String> players) {
        public static final Codec<Packed> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.scores.PlayerTeam.Packed.class, "CODEC");
    }
    public @interface OptionFlags {
    }
}
