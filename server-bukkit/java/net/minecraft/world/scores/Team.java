package net.minecraft.world.scores;
public abstract class Team {
    public enum Visibility { ALWAYS, NEVER, HIDE_FOR_OTHER_TEAMS, HIDE_FOR_OWN_TEAM }
    public enum CollisionRule { ALWAYS, NEVER, PUSH_OTHER_TEAMS, PUSH_OWN_TEAM }
    public abstract String getName();
    public abstract java.util.Collection<String> getPlayers();
}
