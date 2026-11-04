// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.item.ItemEntity;

public class GoToWantedItem {
    public static BehaviorControl<LivingEntity> create(float speedModifier, boolean interruptOngoingWalk, int maxDistToWalk) {
        return create(body -> true, speedModifier, interruptOngoingWalk, maxDistToWalk);
    }

    public static <E extends LivingEntity> BehaviorControl<E> create(Predicate<E> predicate, float speedModifier, boolean interruptOngoingWalk, int maxDistToWalk) {
        return OneShots.<E>of(Set.of(MemoryModuleType.LOOK_TARGET, MemoryModuleType.WALK_TARGET, MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM,
            MemoryModuleType.ITEM_PICKUP_COOLDOWN_TICKS), (level, body, timestamp) -> {
            var brain = body.getBrain();
            if (!interruptOngoingWalk && brain.hasMemoryValue(MemoryModuleType.WALK_TARGET)) {
                return false;
            }
            ItemEntity item = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM).orElse(null);
            if (item == null || brain.hasMemoryValue(MemoryModuleType.ITEM_PICKUP_COOLDOWN_TICKS) || !predicate.test(body)
                || !item.closerThan(body, maxDistToWalk) || !body.level().getWorldBorder().isWithinBounds(item.blockPosition()) || !body.canPickUpLoot()) {
                return false;
            }
            brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(item, true));
            brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(item, false), speedModifier, 0));
            return true;
        });
    }
}
