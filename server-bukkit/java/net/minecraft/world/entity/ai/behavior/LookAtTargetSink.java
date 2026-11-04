// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;

public class LookAtTargetSink extends Behavior<Mob> {
    public LookAtTargetSink(int minDuration, int maxDuration) {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.VALUE_PRESENT), minDuration, maxDuration);
    }

    protected boolean canStillUse(ServerLevel level, Mob body, long timestamp) {
        return body.getBrain().getMemory(MemoryModuleType.LOOK_TARGET).filter(target -> target.isVisibleBy(body)).isPresent();
    }

    protected void stop(ServerLevel level, Mob body, long timestamp) {
        if (body.getBrain().hasMemoryValue(MemoryModuleType.LOOK_TARGET)) {
            body.getBrain().eraseMemory(MemoryModuleType.LOOK_TARGET);
        }
    }

    protected void tick(ServerLevel level, Mob body, long timestamp) {
        body.getBrain().getMemory(MemoryModuleType.LOOK_TARGET).ifPresent(target -> body.getLookControl().setLookAt(target.currentPosition()));
    }
}
