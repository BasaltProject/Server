// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;
import java.util.Optional;
import java.util.function.BooleanSupplier;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.PoiTypeTags;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.world.entity.ai.util.GoalUtils;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.level.block.DoorBlock;
import net.minecraft.world.level.pathfinder.Node;
import net.minecraft.world.level.pathfinder.Path;
import net.minecraft.world.phys.Vec3;

public class MoveThroughVillageGoal extends Goal {
    protected final PathfinderMob mob;
    private final double speedModifier;
    private Path path;
    private BlockPos poiPos;
    private final boolean onlyAtNight;
    private final List<BlockPos> visited = new ArrayList<>();
    private final int distanceToPoi;
    private final BooleanSupplier canDealWithDoors;

    public MoveThroughVillageGoal(PathfinderMob mob, double speedModifier, boolean onlyAtNight, int distanceToPoi,
            BooleanSupplier canDealWithDoors) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        this.onlyAtNight = onlyAtNight;
        this.distanceToPoi = distanceToPoi;
        this.canDealWithDoors = canDealWithDoors;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
        if (!GoalUtils.hasGroundPathNavigation(mob)) {
            throw new IllegalArgumentException("Unsupported mob for MoveThroughVillageGoal");
        }
    }

    @Override
    public boolean canUse() {
        if (!GoalUtils.hasGroundPathNavigation(mob)) {
            return false;
        }
        if (visited.size() > 15) {
            visited.remove(0);
        }
        if (onlyAtNight && mob.level().isBrightOutside()) {
            return false;
        }
        ServerLevel level = (ServerLevel) mob.level();
        BlockPos pos = mob.blockPosition();
        if (!level.isCloseToVillage(pos, 6)) {
            return false;
        }
        Vec3 land = LandRandomPos.getPos(mob, 15, 7, candidate -> {
            if (!level.isVillage(candidate)) {
                return Double.NEGATIVE_INFINITY;
            }
            return findUnvisited(level, candidate).map(poi -> -poi.distSqr(pos)).orElse(Double.NEGATIVE_INFINITY);
        });
        if (land == null) {
            return false;
        }
        Optional<BlockPos> target = findUnvisited(level, BlockPos.containing(land));
        if (target.isEmpty()) {
            return false;
        }
        poiPos = target.get().immutable();
        PathNavigation navigation = mob.getNavigation();
        navigation.setCanOpenDoors(canDealWithDoors.getAsBoolean());
        path = navigation.createPath(poiPos, 0);
        navigation.setCanOpenDoors(true);
        if (path == null) {
            Vec3 partial = DefaultRandomPos.getPosTowards(mob, 10, 7, Vec3.atBottomCenterOf(poiPos), Math.PI / 2);
            if (partial == null) {
                return false;
            }
            navigation.setCanOpenDoors(canDealWithDoors.getAsBoolean());
            path = navigation.createPath(partial.x, partial.y, partial.z, 0);
            navigation.setCanOpenDoors(true);
            if (path == null) {
                return false;
            }
        }
        for (int i = 0; i < path.getNodeCount(); i++) {
            Node node = path.getNode(i);
            if (DoorBlock.isWoodenDoor(mob.level(), new BlockPos(node.x, node.y + 1, node.z))) {
                path = navigation.createPath(node.x, node.y, node.z, 0);
                break;
            }
        }
        return path != null;
    }

    private Optional<BlockPos> findUnvisited(ServerLevel level, BlockPos around) {
        return level.getPoiManager().find(type -> type.is(PoiTypeTags.VILLAGE), poi -> !visited.contains(poi), around, 10,
                PoiManager.Occupancy.IS_OCCUPIED);
    }

    @Override
    public boolean canContinueToUse() {
        return !mob.getNavigation().isDone() && !poiPos.closerToCenterThan(mob.position(), mob.getBbWidth() + distanceToPoi);
    }

    @Override
    public void start() {
        mob.getNavigation().moveTo(path, speedModifier);
    }

    @Override
    public void stop() {
        if (mob.getNavigation().isDone() || poiPos.closerToCenterThan(mob.position(), distanceToPoi)) {
            visited.add(poiPos);
        }
    }
}
