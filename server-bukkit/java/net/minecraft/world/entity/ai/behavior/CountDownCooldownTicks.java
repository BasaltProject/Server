// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;

public class CountDownCooldownTicks extends Behavior<LivingEntity> {
    private final MemoryModuleType<Integer> cooldownTicks;

    public CountDownCooldownTicks(MemoryModuleType<Integer> cooldownTicks) {
        super(Map.of(cooldownTicks, MemoryStatus.VALUE_PRESENT));
        this.cooldownTicks = cooldownTicks;
    }

    protected boolean timedOut(long timestamp) {
        return false;
    }

    protected boolean canStillUse(ServerLevel level, LivingEntity body, long timestamp) {
        return body.getBrain().getMemory(cooldownTicks).isPresent();
    }

    protected void tick(ServerLevel level, LivingEntity body, long timestamp) {
        body.getBrain().getMemory(cooldownTicks).ifPresent(left -> body.getBrain().setMemory(cooldownTicks, left - 1));
    }

    protected void stop(ServerLevel level, LivingEntity body, long timestamp) {
        body.getBrain().eraseMemory(cooldownTicks);
    }
}
