// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.BiPredicate;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class DismountOrSkipMounting {
    public static <E extends LivingEntity> BehaviorControl<E> create(int maxWalkDistToRideTarget, BiPredicate<E, Entity> dontRideIf) {
        return OneShots.<E>of(Set.of(MemoryModuleType.RIDE_TARGET), (level, body, timestamp) -> {
            Entity current = body.getVehicle();
            Entity target = body.getBrain().getMemory(MemoryModuleType.RIDE_TARGET).orElse(null);
            if (current == null && target == null) {
                return false;
            }
            Entity vehicle = current == null ? target : current;
            if (isVehicleValid(body, vehicle, maxWalkDistToRideTarget) && !dontRideIf.test(body, vehicle)) {
                return false;
            }
            body.stopRiding();
            body.getBrain().eraseMemory(MemoryModuleType.RIDE_TARGET);
            return true;
        });
    }

    private static boolean isVehicleValid(LivingEntity body, Entity vehicle, int maxWalkDistToRideTarget) {
        return vehicle.isAlive() && vehicle.closerThan(body, maxWalkDistToRideTarget) && vehicle.level() == body.level();
    }
}
