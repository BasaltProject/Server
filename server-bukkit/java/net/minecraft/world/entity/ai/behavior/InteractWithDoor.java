// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import com.google.common.collect.Sets;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.level.block.DoorBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.pathfinder.Node;
import net.minecraft.world.level.pathfinder.Path;

public class InteractWithDoor {
    private static final int COOLDOWN_BEFORE_RERUNNING_IN_SAME_NODE = 20;
    private static final double SKIP_CLOSING_DOOR_IF_FURTHER_AWAY_THAN = 3.0;
    private static final double MAX_DISTANCE_TO_HOLD_DOOR_OPEN_FOR_OTHER_MOBS = 2.0;

    public static BehaviorControl<LivingEntity> create() {
        Node[] lastSeen = {null};
        int[] wait = {0};
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.PATH, MemoryModuleType.DOORS_TO_CLOSE, MemoryModuleType.NEAREST_LIVING_ENTITIES), (level, body, timestamp) -> {
            Brain<?> mind = body.getBrain();
            Path route = mind.getMemory(MemoryModuleType.PATH).orElse(null);
            if (route == null || route.notStarted() || route.isDone()) {
                return false;
            }
            Node toNode = route.getNextNode();
            if (Objects.equals(lastSeen[0], toNode)) {
                wait[0] = COOLDOWN_BEFORE_RERUNNING_IN_SAME_NODE;
            } else if (--wait[0] > 0) {
                return false;
            }
            lastSeen[0] = toNode;
            Node fromNode = route.getPreviousNode();
            Optional<Set<GlobalPos>> pending = mind.getMemory(MemoryModuleType.DOORS_TO_CLOSE);
            DoorBlock behind = doorAt(level, fromNode.asBlockPos());
            if (behind != null) {
                BlockPos at = fromNode.asBlockPos();
                BlockState state = level.getBlockState(at);
                if (!behind.isOpen(state)) {
                    behind.setOpen(body, level, state, at, true);
                }
                pending = rememberDoorToClose(mind, pending, level, at);
            }
            DoorBlock ahead = doorAt(level, toNode.asBlockPos());
            if (ahead != null) {
                BlockPos at = toNode.asBlockPos();
                BlockState state = level.getBlockState(at);
                if (!ahead.isOpen(state)) {
                    ahead.setOpen(body, level, state, at, true);
                    pending = rememberDoorToClose(mind, pending, level, at);
                }
            }
            Optional<List<LivingEntity>> crowd = mind.getMemory(MemoryModuleType.NEAREST_LIVING_ENTITIES);
            pending.ifPresent(doors -> closeDoorsThatIHaveOpenedOrPassedThrough(level, body, fromNode, toNode, doors, crowd));
            return true;
        });
    }

    private static DoorBlock doorAt(ServerLevel level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        boolean door = state.is(BlockTags.MOB_INTERACTABLE_DOORS, s -> s.getBlock() instanceof DoorBlock);
        return door ? (DoorBlock) state.getBlock() : null;
    }

    public static void closeDoorsThatIHaveOpenedOrPassedThrough(ServerLevel level, LivingEntity body, Node movingFromNode, Node movingToNode, Set<GlobalPos> doors,
            Optional<List<LivingEntity>> nearestEntities) {
        doors.removeIf(remembered -> {
            BlockPos at = remembered.pos();
            boolean onRoute = movingFromNode != null && movingFromNode.asBlockPos().equals(at)
                || movingToNode != null && movingToNode.asBlockPos().equals(at);
            if (onRoute) {
                return false;
            }
            if (isDoorTooFarAway(level, body, remembered)) {
                return true;
            }
            DoorBlock door = doorAt(level, at);
            if (door == null) {
                return true;
            }
            BlockState state = level.getBlockState(at);
            if (door.isOpen(state) && !areOtherMobsComingThroughDoor(body, at, nearestEntities)) {
                door.setOpen(body, level, state, at, false);
            }
            return true;
        });
    }

    private static boolean areOtherMobsComingThroughDoor(LivingEntity body, BlockPos doorPos, Optional<List<LivingEntity>> nearestEntities) {
        return nearestEntities.isPresent() && nearestEntities.get().stream().filter(other -> other.getType() == body.getType())
            .filter(other -> doorPos.closerToCenterThan(other.position(), MAX_DISTANCE_TO_HOLD_DOOR_OPEN_FOR_OTHER_MOBS))
            .anyMatch(other -> isMobComingThroughDoor(other.getBrain(), doorPos));
    }

    private static boolean isMobComingThroughDoor(Brain<?> otherBrain, BlockPos doorPos) {
        if (!otherBrain.hasMemoryValue(MemoryModuleType.PATH)) {
            return false;
        }
        Path path = otherBrain.getMemory(MemoryModuleType.PATH).get();
        if (path.isDone()) {
            return false;
        }
        Node from = path.getPreviousNode();
        return from != null && (doorPos.equals(from.asBlockPos()) || doorPos.equals(path.getNextNode().asBlockPos()));
    }

    private static boolean isDoorTooFarAway(ServerLevel level, LivingEntity body, GlobalPos doorGlobalPos) {
        return doorGlobalPos.dimension() != level.dimension() || !doorGlobalPos.pos().closerToCenterThan(body.position(), SKIP_CLOSING_DOOR_IF_FURTHER_AWAY_THAN);
    }

    private static Optional<Set<GlobalPos>> rememberDoorToClose(Brain<?> brain, Optional<Set<GlobalPos>> doors, ServerLevel level, BlockPos doorPos) {
        GlobalPos globalDoorPos = GlobalPos.of(level.dimension(), doorPos);
        if (doors.isPresent()) {
            doors.get().add(globalDoorPos);
            return doors;
        }
        Set<GlobalPos> set = Sets.newHashSet(globalDoorPos);
        brain.setMemory(MemoryModuleType.DOORS_TO_CLOSE, set);
        return Optional.of(set);
    }
}
