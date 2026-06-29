package org.basaltmc.bukkit.entity;

import com.destroystokyo.paper.entity.Pathfinder;
import java.util.List;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Mob;

public final class BasaltPathfinder implements Pathfinder {
    private static final class Result implements Pathfinder.PathResult {
        private final Location destination;
        private final boolean reachable;

        Result(Location destination, boolean reachable) {
            this.destination = destination;
            this.reachable = reachable;
        }

        @Override
        public List<Location> getPoints() {
            return List.of(destination);
        }

        @Override
        public int getNextPointIndex() {
            return 0;
        }

        @Override
        public Location getNextPoint() {
            return destination;
        }

        @Override
        public Location getFinalPoint() {
            return destination;
        }

        @Override
        public boolean canReachFinalPoint() {
            return reachable;
        }
    }

    private final EntityCore core;
    private final Mob mob;
    private Location destination;

    public BasaltPathfinder(EntityCore core, Mob mob) {
        this.core = core;
        this.mob = mob;
    }

    @Override
    public Mob getEntity() {
        return mob;
    }

    @Override
    public void stopPathfinding() {
        destination = null;
        core.call("stop_path");
    }

    @Override
    public boolean hasPath() {
        return core.test("has_path");
    }

    @Override
    public PathResult getCurrentPath() {
        return hasPath() && destination != null ? new Result(destination, true) : null;
    }

    @Override
    public PathResult findPath(Location location, int range) {
        if (location == null || !location.getWorld().equals(core.world)) {
            return null;
        }
        return new Result(location.clone(), core.test("can_reach", location.getX(), location.getY(), location.getZ()));
    }

    @Override
    public PathResult findPath(Entity target, int range) {
        return findPath(target.getLocation(), range);
    }

    @Override
    public boolean moveTo(PathResult path, double speed) {
        if (path == null || !path.canReachFinalPoint()) {
            return false;
        }
        Location to = path.getFinalPoint();
        destination = to;
        return core.test("move_to", to.getX(), to.getY(), to.getZ(), speed);
    }

    @Override
    public boolean canOpenDoors() {
        return core.flag("nav_open_doors");
    }

    @Override
    public void setCanOpenDoors(boolean canOpenDoors) {
        core.set("nav_open_doors", canOpenDoors);
    }

    @Override
    public boolean canPassDoors() {
        throw Unsupported.hit("Pathfinder#canPassDoors");
    }

    @Override
    public void setCanPassDoors(boolean canPassDoors) {
        throw Unsupported.hit("Pathfinder#setCanPassDoors");
    }

    @Override
    public boolean canFloat() {
        throw Unsupported.hit("Pathfinder#canFloat");
    }

    @Override
    public void setCanFloat(boolean canFloat) {
        throw Unsupported.hit("Pathfinder#setCanFloat");
    }
}
