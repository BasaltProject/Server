// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior.warden;

import java.util.Set;
import net.minecraft.util.Unit;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class TryToSniff {
    private static final IntProvider SNIFF_COOLDOWN = UniformInt.of(100, 200);

    public static BehaviorControl<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.IS_SNIFFING, MemoryModuleType.WALK_TARGET, MemoryModuleType.SNIFF_COOLDOWN,
            MemoryModuleType.NEAREST_ATTACKABLE, MemoryModuleType.DISTURBANCE_LOCATION), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (brain.hasMemoryValue(MemoryModuleType.SNIFF_COOLDOWN) || !brain.hasMemoryValue(MemoryModuleType.NEAREST_ATTACKABLE)
                || brain.hasMemoryValue(MemoryModuleType.DISTURBANCE_LOCATION)) {
                return false;
            }
            brain.setMemory(MemoryModuleType.IS_SNIFFING, Unit.INSTANCE);
            brain.setMemoryWithExpiry(MemoryModuleType.SNIFF_COOLDOWN, Unit.INSTANCE, SNIFF_COOLDOWN.sample(level.getRandom()));
            brain.eraseMemory(MemoryModuleType.WALK_TARGET);
            body.setPose(Pose.SNIFFING);
            return true;
        });
    }
}
