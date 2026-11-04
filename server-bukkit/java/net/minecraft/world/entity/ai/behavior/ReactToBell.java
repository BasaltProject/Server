// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.phys.Vec3;

public class ReactToBell {
    public static BehaviorControl<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.HEARD_BELL_TIME), (level, body, timestamp) -> {
            if (!body.getBrain().hasMemoryValue(MemoryModuleType.HEARD_BELL_TIME)) {
                return false;
            }
            if (level.getRaidAt(body.blockPosition()) == null) {
                body.getBrain().setActiveActivityIfPossible(Activity.HIDE);
            }
            return true;
        });
    }
}
