// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.Container;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.navigation.GroundPathNavigation;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.ChestBlock;
import net.minecraft.world.level.block.entity.BaseContainerBlockEntity;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.ChestType;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.pathfinder.Path;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import org.apache.commons.lang3.function.TriConsumer;

public class TransportItemsBetweenContainers extends Behavior<PathfinderMob> {
    public static final int TARGET_INTERACTION_TIME = 60;
    private static final int VISITED_POSITIONS_MEMORY_TIME = 6000;
    private static final int TRANSPORTED_ITEM_MAX_STACK_SIZE = 16;
    private static final int MAX_VISITED_POSITIONS = 10;
    private static final int MAX_UNREACHABLE_POSITIONS = 50;
    private static final int IDLE_COOLDOWN = 140;
    private static final double QUEUING_DISTANCE = 3.0;
    private static final double CONTINUE_INTERACTING_DISTANCE = 2.0;
    private final float speedModifier;
    private final int horizontalSearchDistance;
    private final int verticalSearchDistance;
    private final Predicate<BlockState> sourceBlockType;
    private final Predicate<BlockState> destinationBlockType;
    private final Predicate<TransportItemTarget> shouldQueueForTarget;
    private final Consumer<PathfinderMob> onStartTravelling;
    private final Map<ContainerInteractionState, OnTargetReachedInteraction> onTargetInteractionActions;
    private TransportItemTarget target;
    private TransportItemState state = TransportItemState.TRAVELLING;
    private ContainerInteractionState interactionState;
    private int ticksSinceReachingTarget;

    public TransportItemsBetweenContainers(float speedModifier, Predicate<BlockState> sourceBlockType, Predicate<BlockState> destinationBlockType,
            int horizontalSearchDistance, int verticalSearchDistance, Map<ContainerInteractionState, OnTargetReachedInteraction> onTargetInteractionActions,
            Consumer<PathfinderMob> onStartTravelling, Predicate<TransportItemTarget> shouldQueueForTarget) {
        super(Map.of(MemoryModuleType.VISITED_BLOCK_POSITIONS, MemoryStatus.REGISTERED, MemoryModuleType.UNREACHABLE_TRANSPORT_BLOCK_POSITIONS, MemoryStatus.REGISTERED,
            MemoryModuleType.TRANSPORT_ITEMS_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT, MemoryModuleType.IS_PANICKING, MemoryStatus.VALUE_ABSENT));
        this.speedModifier = speedModifier;
        this.sourceBlockType = sourceBlockType;
        this.destinationBlockType = destinationBlockType;
        this.horizontalSearchDistance = horizontalSearchDistance;
        this.verticalSearchDistance = verticalSearchDistance;
        this.onStartTravelling = onStartTravelling;
        this.shouldQueueForTarget = shouldQueueForTarget;
        this.onTargetInteractionActions = onTargetInteractionActions;
    }

    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        if (body.getNavigation() instanceof GroundPathNavigation ground) {
            ground.setCanPathToTargetsBelowSurface(true);
        }
    }

    protected boolean checkExtraStartConditions(ServerLevel level, PathfinderMob body) {
        return !body.isLeashed();
    }

    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return body.getBrain().getMemory(MemoryModuleType.TRANSPORT_ITEMS_COOLDOWN_TICKS).isEmpty() && !body.isPanicking() && !body.isLeashed();
    }

    protected boolean timedOut(long timestamp) {
        return false;
    }

    protected void tick(ServerLevel level, PathfinderMob body, long timestamp) {
        boolean retargeted = updateInvalidTarget(level, body);
        if (target == null) {
            stop(level, body, timestamp);
        } else if (!retargeted) {
            if (state == TransportItemState.QUEUING && !isAnotherMobInteractingWithTarget(target, level)) {
                resumeTravelling(body);
            }
            if (state == TransportItemState.TRAVELLING) {
                onTravelToTarget(target, level, body);
            }
            if (state == TransportItemState.INTERACTING) {
                onReachedTarget(target, level, body);
            }
        }
    }

    private boolean updateInvalidTarget(ServerLevel level, PathfinderMob body) {
        if (hasValidTarget(level, body)) {
            return false;
        }
        stopTargetingCurrentTarget(body);
        Optional<TransportItemTarget> next = getTransportTarget(level, body);
        if (next.isPresent()) {
            target = next.get();
            startTravelling(body);
            setVisitedBlockPos(body, level, target.pos);
        } else {
            enterCooldownAfterNoMatchingTargetFound(body);
        }
        return true;
    }

    protected void onTravelToTarget(TransportItemTarget target, Level level, PathfinderMob body) {
        if (isWithinTargetDistance(QUEUING_DISTANCE, target, level, body, centerPos(body)) && isAnotherMobInteractingWithTarget(target, level)) {
            stopInPlace(body);
            state = TransportItemState.QUEUING;
        } else if (isWithinTargetDistance(interactionRange(body), target, level, body, centerPos(body))) {
            startOnReachedTargetInteraction(target, body);
        } else {
            walkTowardsTarget(body);
        }
    }

    private Vec3 centerPos(PathfinderMob body) {
        return middleY(body, body.position());
    }

    protected void onReachedTarget(TransportItemTarget target, Level level, PathfinderMob body) {
        if (!isWithinTargetDistance(CONTINUE_INTERACTING_DISTANCE, target, level, body, centerPos(body))) {
            startTravelling(body);
            return;
        }
        ticksSinceReachingTarget++;
        onTargetInteraction(target, body);
        if (ticksSinceReachingTarget >= TARGET_INTERACTION_TIME) {
            doReachedTargetInteraction(body, target.container, this::pickUpItems, (mob, container) -> stopTargetingCurrentTarget(body),
                this::putDownItem, (mob, container) -> stopTargetingCurrentTarget(body));
            startTravelling(body);
        }
    }

    private void resumeTravelling(PathfinderMob body) {
        state = TransportItemState.TRAVELLING;
        walkTowardsTarget(body);
    }

    private void walkTowardsTarget(PathfinderMob body) {
        if (target != null) {
            BehaviorUtils.setWalkAndLookTargetMemories(body, target.pos, speedModifier, 0);
        }
    }

    private void startOnReachedTargetInteraction(TransportItemTarget target, PathfinderMob body) {
        doReachedTargetInteraction(body, target.container, interaction(ContainerInteractionState.PICKUP_ITEM), interaction(ContainerInteractionState.PICKUP_NO_ITEM),
            interaction(ContainerInteractionState.PLACE_ITEM), interaction(ContainerInteractionState.PLACE_NO_ITEM));
        state = TransportItemState.INTERACTING;
    }

    private void startTravelling(PathfinderMob body) {
        onStartTravelling.accept(body);
        state = TransportItemState.TRAVELLING;
        interactionState = null;
        ticksSinceReachingTarget = 0;
    }

    private BiConsumer<PathfinderMob, Container> interaction(ContainerInteractionState next) {
        return (mob, container) -> interactionState = next;
    }

    private void onTargetInteraction(TransportItemTarget target, PathfinderMob body) {
        body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(target.pos));
        stopInPlace(body);
        if (interactionState != null) {
            OnTargetReachedInteraction action = onTargetInteractionActions.get(interactionState);
            if (action != null) {
                action.accept(body, target, ticksSinceReachingTarget);
            }
        }
    }

    private void doReachedTargetInteraction(PathfinderMob body, Container container, BiConsumer<PathfinderMob, Container> onPickupSuccess,
            BiConsumer<PathfinderMob, Container> onPickupFailure, BiConsumer<PathfinderMob, Container> onPlaceSuccess, BiConsumer<PathfinderMob, Container> onPlaceFailure) {
        if (isPickingUpItems(body)) {
            (container.isEmpty() ? onPickupFailure : onPickupSuccess).accept(body, container);
        } else if (matchesLeavingItemsRequirement(body, container)) {
            onPlaceSuccess.accept(body, container);
        } else {
            onPlaceFailure.accept(body, container);
        }
    }

    private Optional<TransportItemTarget> getTransportTarget(ServerLevel level, PathfinderMob body) {
        AABB area = targetSearchArea(body);
        Set<GlobalPos> visited = visitedPositions(body);
        Set<GlobalPos> unreachable = unreachablePositions(body);
        TransportItemTarget best = null;
        double closest = Float.MAX_VALUE;
        int chunkRadius = Math.floorDiv(horizontalDistance(body), 16) + 1;
        int centerX = body.blockPosition().getX() >> 4;
        int centerZ = body.blockPosition().getZ() >> 4;
        for (int cx = centerX - chunkRadius; cx <= centerX + chunkRadius; cx++) {
            for (int cz = centerZ - chunkRadius; cz <= centerZ + chunkRadius; cz++) {
                LevelChunk chunk = level.getChunkSource().getChunkNow(cx, cz);
                if (chunk == null) {
                    continue;
                }
                for (Object value : chunk.getBlockEntities().values()) {
                    if (value instanceof ChestBlockEntity chest) {
                        double distance = chest.getBlockPos().distToCenterSqr(body.position());
                        if (distance < closest) {
                            TransportItemTarget candidate = validTarget(body, level, chest, visited, unreachable, area);
                            if (candidate != null) {
                                best = candidate;
                                closest = distance;
                            }
                        }
                    }
                }
            }
        }
        return Optional.ofNullable(best);
    }

    private TransportItemTarget validTarget(PathfinderMob body, Level level, BlockEntity blockEntity, Set<GlobalPos> visited, Set<GlobalPos> unreachable, AABB area) {
        BlockPos pos = blockEntity.getBlockPos();
        if (!area.contains(pos.getX(), pos.getY(), pos.getZ())) {
            return null;
        }
        TransportItemTarget candidate = TransportItemTarget.tryCreatePossibleTarget(blockEntity, level);
        if (candidate == null) {
            return null;
        }
        boolean valid = isWantedBlock(body, candidate.state) && !isPositionAlreadyVisited(visited, unreachable, candidate, level) && !isContainerLocked(candidate);
        return valid ? candidate : null;
    }

    private boolean isContainerLocked(TransportItemTarget candidate) {
        return candidate.blockEntity instanceof BaseContainerBlockEntity container && container.isLocked();
    }

    private boolean hasValidTarget(Level level, PathfinderMob body) {
        boolean valid = target != null && isWantedBlock(body, target.state) && targetHasNotChanged(level, target);
        if (valid && !isTargetBlocked(level, target)) {
            if (state != TransportItemState.TRAVELLING || hasValidTravellingPath(level, target, body)) {
                return true;
            }
            markVisitedBlockPosAsUnreachable(body, level, target.pos);
        }
        return false;
    }

    private boolean hasValidTravellingPath(Level level, TransportItemTarget target, PathfinderMob body) {
        Path path = body.getNavigation().getPath() == null ? body.getNavigation().createPath(target.pos, 0) : body.getNavigation().getPath();
        Vec3 from = positionToReachTargetFrom(path, body);
        boolean canReach = isWithinTargetDistance(interactionRange(body), target, level, body, from);
        return path == null && !canReach || canReach && canSeeAnyTargetSide(target, level, body, from);
    }

    private Vec3 positionToReachTargetFrom(Path path, PathfinderMob body) {
        boolean noPath = path == null || path.getEndNode() == null;
        return middleY(body, noPath ? body.position() : Vec3.atBottomCenterOf(path.getEndNode().asBlockPos()));
    }

    private Vec3 middleY(PathfinderMob body, Vec3 pos) {
        return pos.add(0.0, body.getBoundingBox().getYsize() / 2.0, 0.0);
    }

    private boolean isTargetBlocked(Level level, TransportItemTarget target) {
        return level.getBlockState(target.pos.above()).isRedstoneConductor(level, target.pos.above());
    }

    private boolean targetHasNotChanged(Level level, TransportItemTarget target) {
        return level instanceof ServerLevel server && server.getChunkSource().getChunkNow(target.pos.getX() >> 4, target.pos.getZ() >> 4) != null
            && target.blockEntity.equals(server.getChunkSource().getChunkNow(target.pos.getX() >> 4, target.pos.getZ() >> 4).getBlockEntity(target.pos));
    }

    private Stream<TransportItemTarget> connectedTargets(TransportItemTarget target, Level level) {
        if (target.state.getValueOrElse(ChestBlock.TYPE, ChestType.SINGLE) != ChestType.SINGLE) {
            TransportItemTarget connected = TransportItemTarget.tryCreatePossibleTarget(ChestBlock.getConnectedBlockPos(target.pos, target.state), level);
            return connected != null ? Stream.of(target, connected) : Stream.of(target);
        }
        return Stream.of(target);
    }

    private AABB targetSearchArea(PathfinderMob mob) {
        int horizontal = horizontalDistance(mob);
        return new AABB(mob.blockPosition()).inflate(horizontal, verticalDistance(mob), horizontal);
    }

    private int horizontalDistance(PathfinderMob mob) {
        return mob.isPassenger() ? 1 : horizontalSearchDistance;
    }

    private int verticalDistance(PathfinderMob mob) {
        return mob.isPassenger() ? 1 : verticalSearchDistance;
    }

    private static Set<GlobalPos> visitedPositions(PathfinderMob mob) {
        return mob.getBrain().getMemory(MemoryModuleType.VISITED_BLOCK_POSITIONS).orElse(Set.of());
    }

    private static Set<GlobalPos> unreachablePositions(PathfinderMob mob) {
        return mob.getBrain().getMemory(MemoryModuleType.UNREACHABLE_TRANSPORT_BLOCK_POSITIONS).orElse(Set.of());
    }

    private boolean isPositionAlreadyVisited(Set<GlobalPos> visited, Set<GlobalPos> unreachable, TransportItemTarget target, Level level) {
        return connectedTargets(target, level).map(t -> new GlobalPos(level.dimension(), t.pos)).anyMatch(pos -> visited.contains(pos) || unreachable.contains(pos));
    }

    private static boolean hasFinishedPath(PathfinderMob body) {
        return body.getNavigation().getPath() != null && body.getNavigation().getPath().isDone();
    }

    protected void setVisitedBlockPos(PathfinderMob body, Level level, BlockPos target) {
        Set<GlobalPos> visited = new HashSet<>(visitedPositions(body));
        visited.add(new GlobalPos(level.dimension(), target));
        if (visited.size() > MAX_VISITED_POSITIONS) {
            enterCooldownAfterNoMatchingTargetFound(body);
        } else {
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.VISITED_BLOCK_POSITIONS, visited, VISITED_POSITIONS_MEMORY_TIME);
        }
    }

    protected void markVisitedBlockPosAsUnreachable(PathfinderMob body, Level level, BlockPos target) {
        Set<GlobalPos> visited = new HashSet<>(visitedPositions(body));
        visited.remove(new GlobalPos(level.dimension(), target));
        Set<GlobalPos> unreachable = new HashSet<>(unreachablePositions(body));
        unreachable.add(new GlobalPos(level.dimension(), target));
        if (unreachable.size() > MAX_UNREACHABLE_POSITIONS) {
            enterCooldownAfterNoMatchingTargetFound(body);
        } else {
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.VISITED_BLOCK_POSITIONS, visited, VISITED_POSITIONS_MEMORY_TIME);
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.UNREACHABLE_TRANSPORT_BLOCK_POSITIONS, unreachable, VISITED_POSITIONS_MEMORY_TIME);
        }
    }

    private boolean isWantedBlock(PathfinderMob mob, BlockState block) {
        return isPickingUpItems(mob) ? sourceBlockType.test(block) : destinationBlockType.test(block);
    }

    private static double interactionRange(PathfinderMob body) {
        return hasFinishedPath(body) ? 1.0 : 0.5;
    }

    private boolean isWithinTargetDistance(double distance, TransportItemTarget target, Level level, PathfinderMob body, Vec3 fromPos) {
        AABB box = body.getBoundingBox();
        AABB moved = AABB.ofSize(fromPos, box.getXsize(), box.getYsize(), box.getZsize());
        return target.state.getCollisionShape(level, target.pos).bounds().inflate(distance, 0.5, distance).move(target.pos).intersects(moved);
    }

    private boolean canSeeAnyTargetSide(TransportItemTarget target, Level level, PathfinderMob body, Vec3 eye) {
        Vec3 center = Vec3.atCenterOf(target.pos);
        return net.minecraft.core.Direction.stream().map(d -> center.add(0.5 * d.getStepX(), 0.5 * d.getStepY(), 0.5 * d.getStepZ()))
            .map(hit -> level.clip(new net.minecraft.world.level.ClipContext(eye, hit, net.minecraft.world.level.ClipContext.Block.COLLIDER,
                net.minecraft.world.level.ClipContext.Fluid.NONE, body)))
            .anyMatch(result -> result.getType() == net.minecraft.world.phys.HitResult.Type.BLOCK && result.getBlockPos().equals(target.pos));
    }

    private boolean isAnotherMobInteractingWithTarget(TransportItemTarget target, Level level) {
        return connectedTargets(target, level).anyMatch(shouldQueueForTarget);
    }

    private static boolean isPickingUpItems(PathfinderMob body) {
        return body.getMainHandItem().isEmpty();
    }

    private static boolean matchesLeavingItemsRequirement(PathfinderMob body, Container container) {
        if (container.isEmpty()) {
            return true;
        }
        ItemStack held = body.getMainHandItem();
        for (int slot = 0; slot < container.getContainerSize(); slot++) {
            if (ItemStack.isSameItem(container.getItem(slot), held)) {
                return true;
            }
        }
        return false;
    }

    private void pickUpItems(PathfinderMob body, Container container) {
        body.setItemSlot(EquipmentSlot.MAINHAND, pickupItemFromContainer(container));
        body.setGuaranteedDrop(EquipmentSlot.MAINHAND);
        container.setChanged();
        clearMemoriesAfterMatchingTargetFound(body);
    }

    private void putDownItem(PathfinderMob body, Container container) {
        ItemStack left = addItemsToContainer(body, container);
        container.setChanged();
        body.setItemSlot(EquipmentSlot.MAINHAND, left);
        if (left.isEmpty()) {
            clearMemoriesAfterMatchingTargetFound(body);
        } else {
            stopTargetingCurrentTarget(body);
        }
    }

    private static ItemStack pickupItemFromContainer(Container container) {
        for (int slot = 0; slot < container.getContainerSize(); slot++) {
            ItemStack stack = container.getItem(slot);
            if (!stack.isEmpty()) {
                return container.removeItem(slot, Math.min(stack.getCount(), TRANSPORTED_ITEM_MAX_STACK_SIZE));
            }
        }
        return ItemStack.EMPTY;
    }

    private static ItemStack addItemsToContainer(PathfinderMob body, Container container) {
        ItemStack held = body.getMainHandItem();
        for (int slot = 0; slot < container.getContainerSize(); slot++) {
            ItemStack inSlot = container.getItem(slot);
            if (inSlot.isEmpty()) {
                container.setItem(slot, held);
                return ItemStack.EMPTY;
            }
            if (ItemStack.isSameItemSameComponents(inSlot, held) && inSlot.getCount() < inSlot.getMaxStackSize()) {
                int room = inSlot.getMaxStackSize() - inSlot.getCount();
                inSlot.setCount(inSlot.getCount() + Math.min(room, held.getCount()));
                held.setCount(held.getCount() - room);
                container.setItem(slot, inSlot);
                if (held.isEmpty()) {
                    return ItemStack.EMPTY;
                }
            }
        }
        return held;
    }

    protected void stopTargetingCurrentTarget(PathfinderMob body) {
        ticksSinceReachingTarget = 0;
        target = null;
        body.getNavigation().stop();
        body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
    }

    protected void clearMemoriesAfterMatchingTargetFound(PathfinderMob body) {
        stopTargetingCurrentTarget(body);
        body.getBrain().eraseMemory(MemoryModuleType.VISITED_BLOCK_POSITIONS);
        body.getBrain().eraseMemory(MemoryModuleType.UNREACHABLE_TRANSPORT_BLOCK_POSITIONS);
    }

    private void enterCooldownAfterNoMatchingTargetFound(PathfinderMob body) {
        stopTargetingCurrentTarget(body);
        body.getBrain().setMemory(MemoryModuleType.TRANSPORT_ITEMS_COOLDOWN_TICKS, IDLE_COOLDOWN);
        body.getBrain().eraseMemory(MemoryModuleType.VISITED_BLOCK_POSITIONS);
        body.getBrain().eraseMemory(MemoryModuleType.UNREACHABLE_TRANSPORT_BLOCK_POSITIONS);
    }

    protected void stop(ServerLevel level, PathfinderMob body, long timestamp) {
        startTravelling(body);
        if (body.getNavigation() instanceof GroundPathNavigation ground) {
            ground.setCanPathToTargetsBelowSurface(false);
        }
    }

    private void stopInPlace(PathfinderMob mob) {
        mob.getNavigation().stop();
        mob.setXxa(0.0F);
        mob.setYya(0.0F);
        mob.setSpeed(0.0F);
        mob.setDeltaMovement(0.0, mob.getDeltaMovement().y, 0.0);
    }

    public static enum TransportItemState {
        TRAVELLING, QUEUING, INTERACTING;
    }

    public static enum ContainerInteractionState {
        PICKUP_ITEM, PICKUP_NO_ITEM, PLACE_ITEM, PLACE_NO_ITEM;
    }

    public static record TransportItemTarget(BlockPos pos, Container container, BlockEntity blockEntity, BlockState state) {
        public static TransportItemTarget tryCreatePossibleTarget(BlockEntity blockEntity, Level level) {
            if (blockEntity instanceof Container container) {
                return new TransportItemTarget(blockEntity.getBlockPos(), container, blockEntity, blockEntity.getBlockState());
            }
            return null;
        }

        public static TransportItemTarget tryCreatePossibleTarget(BlockPos blockPos, Level level) {
            if (level instanceof ServerLevel server) {
                LevelChunk chunk = server.getChunkSource().getChunkNow(blockPos.getX() >> 4, blockPos.getZ() >> 4);
                BlockEntity blockEntity = chunk == null ? null : chunk.getBlockEntity(blockPos);
                return blockEntity == null ? null : tryCreatePossibleTarget(blockEntity, level);
            }
            return null;
        }
    }

    @FunctionalInterface
    public interface OnTargetReachedInteraction extends TriConsumer<PathfinderMob, TransportItemTarget, Integer> {
    }
}
