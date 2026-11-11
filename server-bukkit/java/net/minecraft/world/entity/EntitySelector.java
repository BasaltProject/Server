// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.function.Predicate;
public final class EntitySelector {
    public static final Predicate<Entity> ENTITY_STILL_ALIVE = Entity::isAlive;
    public static final Predicate<Entity> LIVING_ENTITY_STILL_ALIVE = entity -> entity.isAlive() && entity instanceof LivingEntity;
    public static final Predicate<Entity> ENTITY_NOT_BEING_RIDDEN = entity -> entity.isAlive() && !entity.isVehicle() && !entity.isPassenger();
    public static final Predicate<Entity> CONTAINER_ENTITY_SELECTOR = entity -> entity instanceof net.minecraft.world.Container && entity.isAlive();
    public static final Predicate<Entity> NO_CREATIVE_OR_SPECTATOR = entity -> !(entity instanceof net.minecraft.world.entity.player.Player p) || !(entity.isSpectator() || p.isCreative());
    public static final Predicate<Entity> NO_SPECTATORS = entity -> !entity.isSpectator();
    public static final Predicate<Entity> CAN_BE_COLLIDED_WITH = NO_SPECTATORS.and(entity -> entity.canBeCollidedWith(null));
    public static final Predicate<Entity> CAN_BE_PICKED = Entity::isPickable;
    private EntitySelector() {
        
    }
    public static Predicate<Entity> withinDistance(double centerX, double centerY, double centerZ, double distance) {
        double limit = distance * distance;
        return candidate -> candidate.distanceToSqr(centerX, centerY, centerZ) <= limit;
    }
    public static Predicate<Entity> pushableBy(Entity entity) {
        net.minecraft.world.scores.Team own = entity.getTeam();
        var ownRule = own == null ? net.minecraft.world.scores.Team.CollisionRule.ALWAYS : own.getCollisionRule();
        if (ownRule == net.minecraft.world.scores.Team.CollisionRule.NEVER) {
            return candidate -> false;
        }
        return NO_SPECTATORS.and(candidate -> {
            if (!candidate.isPushable()) {
                return false;
            }
            net.minecraft.world.scores.Team other = candidate.getTeam();
            var otherRule = other == null ? net.minecraft.world.scores.Team.CollisionRule.ALWAYS : other.getCollisionRule();
            if (otherRule == net.minecraft.world.scores.Team.CollisionRule.NEVER) {
                return false;
            }
            boolean allied = own != null && own.isAlliedTo(other);
            boolean ownTeamOnly = ownRule == net.minecraft.world.scores.Team.CollisionRule.PUSH_OWN_TEAM
                || otherRule == net.minecraft.world.scores.Team.CollisionRule.PUSH_OWN_TEAM;
            boolean otherTeamsOnly = ownRule == net.minecraft.world.scores.Team.CollisionRule.PUSH_OTHER_TEAMS
                || otherRule == net.minecraft.world.scores.Team.CollisionRule.PUSH_OTHER_TEAMS;
            return !(ownTeamOnly && allied) && !(otherTeamsOnly && !allied);
        });
    }
    public static Predicate<Entity> notRiding(Entity entity) {
        return candidate -> {
            for (Entity vehicle = candidate; vehicle.isPassenger(); ) {
                vehicle = vehicle.getVehicle();
                if (vehicle == entity) {
                    return false;
                }
            }
            return true;
        };
    }
}
