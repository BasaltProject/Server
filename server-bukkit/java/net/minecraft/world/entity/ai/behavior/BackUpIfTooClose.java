// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Set;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;

public class BackUpIfTooClose {
    public static OneShot<Mob> create(int tooCloseDistance, float strafeSpeed) {
        return OneShots.of(Set.of(MemoryModuleType.WALK_TARGET, MemoryModuleType.LOOK_TARGET, MemoryModuleType.ATTACK_TARGET), (level, body, timestamp) -> {
            var target = body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET);
            if (target.isEmpty() || !target.get().closerThan(body, tooCloseDistance) || !body.hasLineOfSight(target.get())) {
                return false;
            }
            body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(target.get(), true));
            body.getMoveControl().strafe(-strafeSpeed, 0.0F);
            body.setYRot(Mth.rotateIfNecessary(body.getYRot(), body.yHeadRot, 0.0F));
            return true;
        });
    }
}
