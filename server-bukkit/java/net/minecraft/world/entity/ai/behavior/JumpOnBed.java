// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Mob;
public class JumpOnBed extends Behavior<Mob> {
    public JumpOnBed(float speedModifier) {
        super((java.util.Map) null);
    }
    protected boolean checkExtraStartConditions(ServerLevel level, Mob body) {
        return false;
    }
    protected void start(ServerLevel level, Mob body, long timestamp) {
        
    }
    protected void stop(ServerLevel level, Mob body, long timestamp) {
        
    }
    protected boolean canStillUse(ServerLevel level, Mob body, long timestamp) {
        return false;
    }
    protected boolean timedOut(long timestamp) {
        return false;
    }
    protected void tick(ServerLevel level, Mob body, long timestamp) {
        
    }
}
