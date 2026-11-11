// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.npc.villager.Villager;
public class WorkAtPoi extends Behavior<Villager> {
    public WorkAtPoi() {
        super((java.util.Map) null);
    }
    protected boolean checkExtraStartConditions(ServerLevel level, Villager body) {
        return false;
    }
    protected void start(ServerLevel level, Villager body, long timestamp) {
        
    }
    protected void useWorkstation(ServerLevel level, Villager body) {
        return;
    }
    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return false;
    }
}
