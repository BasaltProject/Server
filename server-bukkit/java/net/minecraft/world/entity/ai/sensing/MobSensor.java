// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.sensing;

import java.util.Set;
import java.util.function.BiPredicate;
import java.util.function.Predicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class MobSensor<T extends LivingEntity> extends Sensor<T> {
    private final BiPredicate<T, LivingEntity> mobTest;
    private final Predicate<T> readyTest;
    private final MemoryModuleType<Boolean> toSet;
    private final int memoryTimeToLive;

    public MobSensor(int scanRate, BiPredicate<T, LivingEntity> mobTest, Predicate<T> readyTest, MemoryModuleType<Boolean> toSet, int memoryTimeToLive) {
        super(scanRate);
        this.mobTest = mobTest;
        this.readyTest = readyTest;
        this.toSet = toSet;
        this.memoryTimeToLive = memoryTimeToLive;
    }

    protected void doTick(ServerLevel level, T body) {
        if (readyTest.test(body)) {
            checkForMobsNearby(body);
        } else {
            clearMemory(body);
        }
    }

    public Set<MemoryModuleType<?>> requires() {
        return Set.of(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES);
    }

    public void checkForMobsNearby(T body) {
        boolean found = body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES)
            .map(visible -> visible.contains(other -> mobTest.test(body, other))).orElse(false);
        if (found) {
            mobDetected(body);
        }
    }

    public void mobDetected(T body) {
        body.getBrain().setMemoryWithExpiry(toSet, true, memoryTimeToLive);
    }

    public void clearMemory(T body) {
        body.getBrain().eraseMemory(toSet);
    }
}
