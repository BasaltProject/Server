// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior.warden;
import net.minecraft.util.Unit;
import java.util.Map;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.Behavior;
import net.minecraft.world.entity.monster.warden.Warden;
public class SonicBoom extends Behavior<Warden> {
    public static final int COOLDOWN = 40;
    public SonicBoom() {
        super((java.util.Map) null);
    }
    protected boolean checkExtraStartConditions(ServerLevel level, Warden body) {
        return false;
    }
    protected boolean canStillUse(ServerLevel level, Warden body, long timestamp) {
        return false;
    }
    protected void start(ServerLevel level, Warden body, long timestamp) {
        
    }
    protected void tick(ServerLevel level, Warden body, long timestamp) {
        
    }
    protected void stop(ServerLevel level, Warden body, long timestamp) {
        
    }
    public static void setCooldown(LivingEntity body, int cooldown) {
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.SONIC_BOOM_COOLDOWN, Unit.INSTANCE, cooldown);
    }
}
