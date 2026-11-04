// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;

public class PlayTagWithOtherKids {
    private static final int MAX_FLEE_XZ_DIST = 20;
    private static final int MAX_FLEE_Y_DIST = 8;
    private static final float SPEED_MODIFIER = 0.6F;
    private static final int MAX_CHASERS_PER_TARGET = 5;
    private static final int AVERAGE_WAIT_TIME_BETWEEN_RUNS = 10;

    public static BehaviorControl<PathfinderMob> create() {
        return OneShots.<PathfinderMob>of(Set.of(MemoryModuleType.VISIBLE_VILLAGER_BABIES, MemoryModuleType.WALK_TARGET, MemoryModuleType.LOOK_TARGET,
            MemoryModuleType.INTERACTION_TARGET), (level, me, timestamp) -> {
            Brain<?> brain = me.getBrain();
            List<LivingEntity> friends = brain.getMemory(MemoryModuleType.VISIBLE_VILLAGER_BABIES).orElse(null);
            if (friends == null || brain.hasMemoryValue(MemoryModuleType.WALK_TARGET) || level.getRandom().nextInt(AVERAGE_WAIT_TIME_BETWEEN_RUNS) != 0) {
                return false;
            }
            if (friends.stream().noneMatch(friend -> isFriendChasingMe(me, friend))) {
                Optional<LivingEntity> chased = findSomeoneBeingChased(friends);
                if (chased.isPresent()) {
                    chaseKid(brain, chased.get());
                } else {
                    friends.stream().findAny().ifPresent(entity -> chaseKid(brain, entity));
                }
                return true;
            }
            for (int j = 0; j < 10; ++j) {
                Vec3 pos = net.minecraft.world.entity.ai.util.LandRandomPos.getPos(me, MAX_FLEE_XZ_DIST, MAX_FLEE_Y_DIST);
                if (pos != null && level.isVillage(BlockPos.containing(pos))) {
                    brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(pos, SPEED_MODIFIER, 0));
                    break;
                }
            }
            return true;
        });
    }

    private static void chaseKid(Brain<?> brain, LivingEntity kid) {
        brain.setMemory(MemoryModuleType.INTERACTION_TARGET, kid);
        brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(kid, true));
        brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(kid, false), SPEED_MODIFIER, 1));
    }

    private static Optional<LivingEntity> findSomeoneBeingChased(List<LivingEntity> friends) {
        Map<LivingEntity, Integer> chased = new java.util.HashMap<>();
        for (LivingEntity chaser : friends) {
            Optional<LivingEntity> target = chaser.getBrain().getMemory(MemoryModuleType.INTERACTION_TARGET);
            target.ifPresent(t -> chased.merge(t, 1, Integer::sum));
        }
        return chased.entrySet().stream().sorted(java.util.Comparator.comparingInt(Map.Entry::getValue)).filter(e -> e.getValue() > 0 && e.getValue() <= MAX_CHASERS_PER_TARGET)
            .map(Map.Entry::getKey).findFirst();
    }

    private static boolean isFriendChasingMe(LivingEntity me, LivingEntity friend) {
        return friend.getBrain().getMemory(MemoryModuleType.INTERACTION_TARGET).filter(mob -> mob == me).isPresent();
    }
}
