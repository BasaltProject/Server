// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;
import java.util.Set;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import com.google.common.collect.Sets;
import java.util.UUID;
import java.util.List;
import java.util.ArrayList;
import net.minecraft.world.entity.animal.frog.Frog;
public class FrogAttackablesSensor extends NearestVisibleLivingEntitySensor {
    public static final float TARGET_DETECTION_DISTANCE = 10.0F;
    @Override
    protected boolean isMatchingEntity(ServerLevel level, LivingEntity body, LivingEntity mob) {
        if (!Sensor.isEntityAttackable(level, body, mob) || !Frog.canEat(mob)) {
            return false;
        }
        List<UUID> unreachable = body.getBrain().getMemory(MemoryModuleType.UNREACHABLE_TONGUE_TARGETS).orElseGet(ArrayList::new);
        return !unreachable.contains(mob.getUUID()) && mob.closerThan(body, 10.0);
    }
    @Override
    protected MemoryModuleType<LivingEntity> getMemoryToSet() {
        return MemoryModuleType.NEAREST_ATTACKABLE;
    }
    @Override
    public Set<MemoryModuleType<?>> requires() {
        return Sets.union(super.requires(), Set.of(MemoryModuleType.UNREACHABLE_TONGUE_TARGETS));
    }
}
