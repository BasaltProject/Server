// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.HashSet;
import java.util.Set;
import net.minecraft.core.GlobalPos;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import java.util.Map;
import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.Container;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import org.apache.commons.lang3.function.TriConsumer;
public class TransportItemsBetweenContainers extends Behavior<PathfinderMob> {
    public static final int TARGET_INTERACTION_TIME = 60;
    public TransportItemsBetweenContainers(float speedModifier, Predicate<BlockState> sourceBlockType, Predicate<BlockState> destinationBlockType, int horizontalSearchDistance, int verticalSearchDistance, Map<ContainerInteractionState, OnTargetReachedInteraction> onTargetInteractionActions, Consumer<PathfinderMob> onStartTravelling, Predicate<TransportItemTarget> shouldQueueForTarget) {
        super((java.util.Map) null);
    }
    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        
    }
    protected boolean checkExtraStartConditions(ServerLevel level, PathfinderMob body) {
        return false;
    }
    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return false;
    }
    private static Set<GlobalPos> getVisitedPositions(PathfinderMob body) {
        return body.getBrain().getMemory(MemoryModuleType.VISITED_BLOCK_POSITIONS).orElse(java.util.Collections.emptySet());
    }
    private static Set<GlobalPos> getUnreachablePositions(PathfinderMob body) {
        return body.getBrain().getMemory(MemoryModuleType.UNREACHABLE_TRANSPORT_BLOCK_POSITIONS).orElse(java.util.Collections.emptySet());
    }
    protected boolean timedOut(long timestamp) {
        return false;
    }
    protected void tick(ServerLevel level, PathfinderMob body, long timestamp) {
        
    }
    protected void onTravelToTarget(TransportItemTarget target, Level level, PathfinderMob body) {
    }
    protected void onReachedTarget(TransportItemTarget target, Level level, PathfinderMob body) {
    }
    protected void setVisitedBlockPos(PathfinderMob body, Level level, BlockPos target) {
        Set<GlobalPos> seen = new HashSet<>(getVisitedPositions(body));
        seen.add(new GlobalPos(level.dimension(), target));
        if (seen.size() > 10) {
            stopTargetingCurrentTarget(body);
            body.getBrain().setMemory(MemoryModuleType.TRANSPORT_ITEMS_COOLDOWN_TICKS, 140);
        } else {
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.VISITED_BLOCK_POSITIONS, seen, 6000L);
        }
    }
    protected void markVisitedBlockPosAsUnreachable(PathfinderMob body, Level level, BlockPos target) {
        Set<GlobalPos> seen = new HashSet<>(getVisitedPositions(body));
        seen.remove(new GlobalPos(level.dimension(), target));
        Set<GlobalPos> blocked = new HashSet<>(getUnreachablePositions(body));
        blocked.add(new GlobalPos(level.dimension(), target));
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.VISITED_BLOCK_POSITIONS, seen, 6000L);
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.UNREACHABLE_TRANSPORT_BLOCK_POSITIONS, blocked, 6000L);
    }
    protected void stopTargetingCurrentTarget(PathfinderMob body) {
        body.getNavigation().stop();
        body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
    }
    protected void clearMemoriesAfterMatchingTargetFound(PathfinderMob body) {
        stopTargetingCurrentTarget(body);
        body.getBrain().eraseMemory(MemoryModuleType.VISITED_BLOCK_POSITIONS);
        body.getBrain().eraseMemory(MemoryModuleType.UNREACHABLE_TRANSPORT_BLOCK_POSITIONS);
    }
    protected void stop(ServerLevel level, PathfinderMob body, long timestamp) {
        
    }
    public static enum TransportItemState {
        TRAVELLING, QUEUING, INTERACTING;
    }
    public static enum ContainerInteractionState {
        PICKUP_ITEM, PICKUP_NO_ITEM, PLACE_ITEM, PLACE_NO_ITEM;
    }
    public static record TransportItemTarget(BlockPos pos, Container container, BlockEntity blockEntity, BlockState state) {
        public static TransportItemTarget tryCreatePossibleTarget(BlockEntity blockEntity, Level level) {
            return null;
        }
        public static TransportItemTarget tryCreatePossibleTarget(BlockPos blockPos, Level level) {
            BlockEntity entity = level.getBlockEntity(blockPos);
            return entity == null ? null : tryCreatePossibleTarget(entity, level);
        }
    }
    public interface OnTargetReachedInteraction extends TriConsumer<PathfinderMob, TransportItemTarget, Integer> {
    }
}
