// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Iterator;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.level.pathfinder.Path;
import net.minecraft.world.level.block.DoorBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.tags.BlockTags;
import java.util.Iterator;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.state.BlockState;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.pathfinder.Node;
public class InteractWithDoor {
    private static boolean othersUsing(LivingEntity body, BlockPos door, Optional<List<LivingEntity>> nearby) {
        return nearby.isPresent() && nearby.get().stream().filter(other -> other.getType() == body.getType())
            .filter(other -> door.closerToCenterThan(other.position(), 2.0)).anyMatch(other -> passingThrough(other.getBrain(), door));
    }
    private static boolean passingThrough(Brain<?> brain, BlockPos door) {
        if (!brain.hasMemoryValue(MemoryModuleType.PATH)) {
            return false;
        }
        Path path = brain.getMemory(MemoryModuleType.PATH).get();
        if (path.isDone() || path.getPreviousNode() == null) {
            return false;
        }
        return door.equals(path.getPreviousNode().asBlockPos()) || door.equals(path.getNextNode().asBlockPos());
    }

    public static BehaviorControl<LivingEntity> create() {
        return new net.minecraft.world.entity.ai.behavior.InertBehavior<>();
    }
    public static void closeDoorsThatIHaveOpenedOrPassedThrough(ServerLevel level, LivingEntity body, Node movingFromNode, Node movingToNode, Set<GlobalPos> doors, Optional<List<LivingEntity>> nearestEntities) {
        for (Iterator<GlobalPos> each = doors.iterator(); each.hasNext(); ) {
            GlobalPos door = each.next();
            BlockPos at = door.pos();
            boolean inUse = (movingFromNode != null && movingFromNode.asBlockPos().equals(at)) || (movingToNode != null && movingToNode.asBlockPos().equals(at));
            if (inUse) {
                continue;
            }
            if (door.dimension() != level.dimension() || !at.closerToCenterThan(body.position(), 3.0)) {
                each.remove();
                continue;
            }
            BlockState state = level.getBlockState(at);
            if (!state.is(BlockTags.MOB_INTERACTABLE_DOORS, s -> s.getBlock() instanceof DoorBlock)) {
                each.remove();
                continue;
            }
            DoorBlock block = (DoorBlock) state.getBlock();
            if (block.isOpen(state) && !othersUsing(body, at, nearestEntities)) {
                block.setOpen(body, level, state, at, false);
            }
            each.remove();
        }
    }
}
