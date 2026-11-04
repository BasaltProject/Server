package org.basaltmc.bukkit.entity;

import com.destroystokyo.paper.entity.Pathfinder;
import java.util.List;
import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Mob;

public final class BasaltPathfinder implements Pathfinder {
    private static final class Result implements Pathfinder.PathResult {
        private final Location destination;
        private final boolean reachable;
        private final List<Location> points;

        Result(Location destination, boolean reachable) {
            this(destination, reachable, List.of(destination));
        }

        Result(Location destination, boolean reachable, List<Location> points) {
            this.destination = destination;
            this.reachable = reachable;
            this.points = points;
        }

        @Override
        public List<Location> getPoints() {
            return points;
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
        if (!hasPath() || destination == null) {
            return null;
        }
        List<Location> nodes = nodesOf(core.call("path_nodes"));
        return nodes.isEmpty() ? new Result(destination, true) : new Result(destination, true, nodes);
    }

    @Override
    public PathResult findPath(Location location, int range) {
        if (location == null || !core.world.equals(location.getWorld())) {
            return null;
        }
        List<Location> nodes = nodesOf(core.call("find_path", location.getX(), location.getY(), location.getZ()));
        if (nodes.isEmpty()) {
            return new Result(location.clone(), false);
        }
        return new Result(location.clone(), true, nodes);
    }

    private List<Location> nodesOf(Object stored) {
        List<Location> nodes = new java.util.ArrayList<>();
        if (stored instanceof String[] flat) {
            for (int i = 0; i + 2 < flat.length; i += 3) {
                nodes.add(new Location(core.world, Integer.parseInt(flat[i]) + 0.5, Integer.parseInt(flat[i + 1]),
                    Integer.parseInt(flat[i + 2]) + 0.5));
            }
        }
        return nodes;
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
        return core.flag("nav_pass_doors");
    }

    @Override
    public void setCanPassDoors(boolean canPassDoors) {
        core.set("nav_pass_doors", canPassDoors);
    }

    @Override
    public boolean canFloat() {
        return core.flag("nav_can_float");
    }

    @Override
    public void setCanFloat(boolean canFloat) {
        core.set("nav_can_float", canFloat);
    }
}
