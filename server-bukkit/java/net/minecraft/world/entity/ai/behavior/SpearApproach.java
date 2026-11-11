// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.PathfinderMob;
public class SpearApproach extends Behavior<PathfinderMob> {
    public SpearApproach(double speedModifierWhenRepositioning, float approachDistance) {
        super((java.util.Map) null);
    }
    protected boolean checkExtraStartConditions(ServerLevel level, PathfinderMob body) {
        return false;
    }
    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        
    }
    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return false;
    }
    protected void tick(ServerLevel level, PathfinderMob mob, long timestamp) {
        
    }
    protected void stop(ServerLevel level, PathfinderMob body, long timestamp) {
        
    }
    protected boolean timedOut(long timestamp) {
        return false;
    }
}
