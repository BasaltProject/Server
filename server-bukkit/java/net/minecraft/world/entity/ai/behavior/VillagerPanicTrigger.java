// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.npc.villager.Villager;
public class VillagerPanicTrigger extends Behavior<Villager> {
    public VillagerPanicTrigger() {
        super((java.util.Map) null);
    }
    protected boolean canStillUse(ServerLevel level, Villager body, long timestamp) {
        return false;
    }
    protected void start(ServerLevel level, Villager body, long timestamp) {
        
    }
    protected void tick(ServerLevel level, Villager body, long timestamp) {
        
    }
    public static boolean hasHostile(LivingEntity myBody) {
        return myBody.getBrain().hasMemoryValue(MemoryModuleType.NEAREST_HOSTILE);
    }
    public static boolean isHurt(LivingEntity myBody) {
        return myBody.getBrain().hasMemoryValue(MemoryModuleType.HURT_BY);


    }
}
