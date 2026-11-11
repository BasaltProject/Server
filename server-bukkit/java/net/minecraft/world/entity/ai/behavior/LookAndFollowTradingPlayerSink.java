// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.npc.villager.Villager;
public class LookAndFollowTradingPlayerSink extends Behavior<Villager> {
    public LookAndFollowTradingPlayerSink(float speedModifier) {
        super((java.util.Map) null);
    }
    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        return false;
    }
    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return false;
    }
    protected void start(ServerLevel level, Villager body, long timestamp) {
        
    }
    protected void stop(ServerLevel level, Villager body, long timestamp) {
        
    }
    protected void tick(ServerLevel level, Villager body, long timestamp) {
        
    }
    protected boolean timedOut(long timestamp) {
        return false;
    }
}
