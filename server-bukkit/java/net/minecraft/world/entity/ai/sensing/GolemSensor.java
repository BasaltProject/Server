// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;
import net.minecraft.world.entity.EntityTypes;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
public class GolemSensor extends Sensor<LivingEntity> {
    public GolemSensor() {
        super();
    }
    public GolemSensor(int scanRate) {
        super(scanRate);
    }
    protected void doTick(ServerLevel level, LivingEntity body) {
        
    }
    public Set<MemoryModuleType<?>> requires() {
        return java.util.Set.of();
    }
    public static void checkForNearbyGolem(LivingEntity body) {
        body.getBrain().getMemory(MemoryModuleType.NEAREST_LIVING_ENTITIES).ifPresent(seen -> {
            if (seen.stream().anyMatch(entity -> entity.is(EntityTypes.IRON_GOLEM))) {
                golemDetected(body);
            }
        });
    }
    public static void golemDetected(LivingEntity body) {
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.GOLEM_DETECTED_RECENTLY, true, 599L);


    }
}
