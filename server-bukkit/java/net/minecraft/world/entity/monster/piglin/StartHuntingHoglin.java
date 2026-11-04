// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

import java.util.Set;
import net.minecraft.world.entity.ai.behavior.OneShot;
import net.minecraft.world.entity.ai.behavior.OneShots;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.monster.hoglin.Hoglin;

public class StartHuntingHoglin {
    public static OneShot<Piglin> create() {
        return OneShots.<Piglin>of(Set.of(MemoryModuleType.NEAREST_VISIBLE_HUNTABLE_HOGLIN, MemoryModuleType.ANGRY_AT, MemoryModuleType.HUNTED_RECENTLY,
            MemoryModuleType.NEAREST_VISIBLE_ADULT_PIGLINS), (level, body, timestamp) -> {
            var brain = body.getBrain();
            Hoglin target = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_HUNTABLE_HOGLIN).orElse(null);
            if (target == null || brain.hasMemoryValue(MemoryModuleType.ANGRY_AT) || brain.hasMemoryValue(MemoryModuleType.HUNTED_RECENTLY) || body.isBaby()) {
                return false;
            }
            var piglins = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_ADULT_PIGLINS);
            if (piglins.filter(p -> p.stream().anyMatch(StartHuntingHoglin::hasHuntedRecently)).isPresent()) {
                return false;
            }
            PiglinAi.setAngerTarget(level, body, target);
            PiglinAi.dontKillAnyMoreHoglinsForAWhile(body);
            PiglinAi.broadcastAngerTarget(level, body, target);
            piglins.ifPresent(p -> p.forEach(PiglinAi::dontKillAnyMoreHoglinsForAWhile));
            return true;
        });
    }

    private static boolean hasHuntedRecently(AbstractPiglin otherPiglin) {
        return otherPiglin.getBrain().hasMemoryValue(MemoryModuleType.HUNTED_RECENTLY);
    }
}
