// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
public class SleepInBed extends Behavior<LivingEntity> {
    public static final int COOLDOWN_AFTER_BEING_WOKEN = 100;
    public SleepInBed() {
        super((java.util.Map) null);
    }
    protected boolean checkExtraStartConditions(ServerLevel level, LivingEntity body) {
        return false;
    }
    protected boolean canStillUse(ServerLevel level, LivingEntity body, long timestamp) {
        return false;
    }
    protected void start(ServerLevel level, LivingEntity body, long timestamp) {
        
    }
    protected boolean timedOut(long timestamp) {
        return false;
    }
    protected void stop(ServerLevel level, LivingEntity body, long timestamp) {
        
    }
}
