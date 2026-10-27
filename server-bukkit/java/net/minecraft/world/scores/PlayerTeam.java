package net.minecraft.world.scores;
import net.minecraft.network.chat.Component;
public class PlayerTeam extends Team {
    private final String name;
    private Component displayName;
    private Component playerPrefix = Component.empty();
    private Component playerSuffix = Component.empty();
    private final Scoreboard scoreboard;
    private final java.util.Set<String> players = new java.util.LinkedHashSet<>();
    private boolean friendlyFire = true, seeFriendlyInvisibles = true;
    private Team.Visibility nameTagVisibility = Team.Visibility.ALWAYS;
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
}
