package org.basaltmc.bukkit.misc;

import io.papermc.paper.raytracing.BlockCollisionMode;
import io.papermc.paper.raytracing.PositionedRayTraceConfigurationBuilder;
import io.papermc.paper.raytracing.RayTraceTarget;
import java.util.EnumSet;
import java.util.Set;
import java.util.function.Predicate;
import org.bukkit.FluidCollisionMode;
import org.bukkit.Location;
import org.bukkit.block.Block;
import org.bukkit.entity.Entity;
import org.bukkit.util.RayTraceResult;
import org.bukkit.util.Vector;

/** What a plugin sets in World#rayTrace(Consumer): the ray, what it may hit and what it ignores. */
public final class RayTraceBuilder implements PositionedRayTraceConfigurationBuilder {
    private Location start;
    private Vector direction;
    private double maxDistance = -1.0;
    private FluidCollisionMode fluids = FluidCollisionMode.NEVER;
    private boolean ignorePassable;
    private double raySize;
    private Predicate<? super Entity> entityFilter;
    private Predicate<? super Block> blockFilter;
    private Set<RayTraceTarget> targets = EnumSet.allOf(RayTraceTarget.class);

    @Override
    public PositionedRayTraceConfigurationBuilder start(Location start) {
        this.start = start;
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder direction(Vector direction) {
        this.direction = direction;
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder maxDistance(double maxDistance) {
        this.maxDistance = maxDistance;
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder fluidCollisionMode(FluidCollisionMode fluidCollisionMode) {
        this.fluids = fluidCollisionMode;
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder blockCollisionMode(BlockCollisionMode blockCollisionMode) {
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder ignorePassableBlocks(boolean ignorePassableBlocks) {
        this.ignorePassable = ignorePassableBlocks;
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder raySize(double raySize) {
        this.raySize = raySize;
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder entityFilter(Predicate<? super Entity> entityFilter) {
        this.entityFilter = entityFilter;
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder blockFilter(Predicate<? super Block> blockFilter) {
        this.blockFilter = blockFilter;
        return this;
    }

    @Override
    public PositionedRayTraceConfigurationBuilder targets(RayTraceTarget first, RayTraceTarget... others) {
        this.targets = EnumSet.of(first, others);
        return this;
    }

    public RayTraceResult run() {
        if (start == null || start.getWorld() == null || direction == null || maxDistance < 0.0) {
            throw new IllegalStateException("A ray needs a start in a world, a direction and a maximum distance");
        }
        Vector from = start.toVector();
        boolean blocks = targets.contains(RayTraceTarget.BLOCK);
        boolean entities = targets.contains(RayTraceTarget.ENTITY);
        if (blocks && entities) {
            return RayTracing.both(start.getWorld(), from, direction, maxDistance, fluids, ignorePassable, raySize,
                entityFilter, blockFilter);
        }
        return blocks ? RayTracing.blocks(start.getWorld(), from, direction, maxDistance, fluids, ignorePassable,
            blockFilter) : RayTracing.entities(start.getWorld(), from, direction, maxDistance, raySize, entityFilter);
    }
}
