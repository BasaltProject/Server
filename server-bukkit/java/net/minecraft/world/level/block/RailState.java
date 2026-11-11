// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.RailShape;

public class RailState {
    private static final int kNorth = 1;
    private static final int kSouth = 2;
    private static final int kWest = 4;
    private static final int kEast = 8;

    private final Level world;
    private final BlockPos origin;
    private final BaseRailBlock block;
    private BlockState current;
    private final boolean straightOnly;
    private final List<BlockPos> links = new ArrayList<>();

    public RailState(Level level, BlockPos pos, BlockState state) {
        this.world = level;
        this.origin = pos;
        this.current = state;
        this.block = (BaseRailBlock) state.getBlock();
        this.straightOnly = !(block instanceof RailBlock);
        refreshLinks(state.getValue(block.getShapeProperty()));
    }

    private static boolean isRailAt(Level level, BlockPos pos) {
        return level.getBlockState(pos).is(BlockTags.RAILS);
    }

    private void refreshLinks(RailShape shape) {
        links.clear();
        BlockPos n = origin.north();
        BlockPos s = origin.south();
        BlockPos w = origin.west();
        BlockPos e = origin.east();
        switch (shape) {
            case NORTH_SOUTH -> addLinks(n, s);
            case EAST_WEST -> addLinks(w, e);
            case ASCENDING_EAST -> addLinks(w, e.above());
            case ASCENDING_WEST -> addLinks(w.above(), e);
            case ASCENDING_NORTH -> addLinks(n.above(), s);
            case ASCENDING_SOUTH -> addLinks(n, s.above());
            case SOUTH_EAST -> addLinks(e, s);
            case SOUTH_WEST -> addLinks(w, s);
            case NORTH_WEST -> addLinks(w, n);
            case NORTH_EAST -> addLinks(e, n);
        }
    }

    private void addLinks(BlockPos first, BlockPos second) {
        links.add(first);
        links.add(second);
    }

    public List<BlockPos> getConnections() {
        return links;
    }

    private boolean railNear(BlockPos pos) {
        return isRailAt(world, pos) || isRailAt(world, pos.above()) || isRailAt(world, pos.below());
    }

    private RailState railAt(BlockPos pos) {
        for (BlockPos candidate : new BlockPos[] {pos, pos.above(), pos.below()}) {
            BlockState found = world.getBlockState(candidate);
            if (found.is(BlockTags.RAILS)) {
                return new RailState(world, candidate, found);
            }
        }
        return null;
    }

    private boolean linkedWith(RailState other) {
        return linksColumn(other.origin);
    }

    private boolean linksColumn(BlockPos target) {
        for (BlockPos link : links) {
            if (link.getX() == target.getX() && link.getZ() == target.getZ()) {
                return true;
            }
        }
        return false;
    }

    private void dropLooseLinks() {
        for (int i = links.size() - 1; i >= 0; i--) {
            RailState other = railAt(links.get(i));
            if (other != null && other.linkedWith(this)) {
                links.set(i, other.origin);
            } else {
                links.remove(i);
            }
        }
    }

    protected int countPotentialConnections() {
        int total = 0;
        for (Direction side : Direction.Plane.HORIZONTAL) {
            total += railNear(origin.relative(side)) ? 1 : 0;
        }
        return total;
    }

    private boolean acceptsLinkFrom(RailState other) {
        return linkedWith(other) || links.size() != 2;
    }

    private int sideMask(java.util.function.Predicate<BlockPos> test) {
        int mask = 0;
        mask |= test.test(origin.north()) ? kNorth : 0;
        mask |= test.test(origin.south()) ? kSouth : 0;
        mask |= test.test(origin.west()) ? kWest : 0;
        mask |= test.test(origin.east()) ? kEast : 0;
        return mask;
    }

    private RailShape cornerFor(int mask) {
        if (straightOnly) {
            return null;
        }
        return switch (mask) {
            case kSouth | kEast -> RailShape.SOUTH_EAST;
            case kSouth | kWest -> RailShape.SOUTH_WEST;
            case kNorth | kWest -> RailShape.NORTH_WEST;
            case kNorth | kEast -> RailShape.NORTH_EAST;
            default -> null;
        };
    }

    private RailShape raise(RailShape flat) {
        if (flat == RailShape.NORTH_SOUTH) {
            RailShape up = isRailAt(world, origin.south().above()) ? RailShape.ASCENDING_SOUTH : flat;
            return isRailAt(world, origin.north().above()) && up == flat ? RailShape.ASCENDING_NORTH : up;
        }
        if (flat == RailShape.EAST_WEST) {
            RailShape up = isRailAt(world, origin.west().above()) ? RailShape.ASCENDING_WEST : flat;
            return isRailAt(world, origin.east().above()) && up == flat ? RailShape.ASCENDING_EAST : up;
        }
        return flat;
    }

    private void connectTo(RailState other) {
        links.add(other.origin);
        int mask = sideMask(this::linksColumn);
        RailShape shape = null;
        if ((mask & (kNorth | kSouth)) != 0) {
            shape = RailShape.NORTH_SOUTH;
        }
        if ((mask & (kWest | kEast)) != 0) {
            shape = RailShape.EAST_WEST;
        }
        RailShape corner = cornerFor(mask);
        shape = corner != null ? corner : shape;
        shape = shape == null ? RailShape.NORTH_SOUTH : raise(shape);
        current = current.setValue(block.getShapeProperty(), shape);
        world.setBlockAndUpdate(origin, current);
    }

    private boolean neighbourAccepts(BlockPos pos) {
        RailState neighbour = railAt(pos);
        if (neighbour == null) {
            return false;
        }
        neighbour.dropLooseLinks();
        return neighbour.acceptsLinkFrom(this);
    }

    public RailState place(boolean hasSignal, boolean first, RailShape defaultShape) {
        int mask = sideMask(this::neighbourAccepts);
        boolean vertical = (mask & (kNorth | kSouth)) != 0;
        boolean horizontal = (mask & (kWest | kEast)) != 0;
        RailShape shape = null;
        if (vertical != horizontal) {
            shape = vertical ? RailShape.NORTH_SOUTH : RailShape.EAST_WEST;
        }
        RailShape corner = cornerFor(mask);
        shape = corner != null ? corner : shape;
        if (shape == null) {
            shape = pickFallback(mask, vertical, horizontal, hasSignal, defaultShape);
        }
        shape = shape == null ? defaultShape : raise(shape);
        refreshLinks(shape);
        current = current.setValue(block.getShapeProperty(), shape);
        if (first || world.getBlockState(origin) != current) {
            world.setBlockAndUpdate(origin, current);
            for (BlockPos link : new ArrayList<>(links)) {
                RailState other = railAt(link);
                if (other != null) {
                    other.dropLooseLinks();
                    if (other.acceptsLinkFrom(this)) {
                        other.connectTo(this);
                    }
                }
            }
        }
        return this;
    }

    private RailShape pickFallback(int mask, boolean vertical, boolean horizontal, boolean hasSignal, RailShape defaultShape) {
        RailShape shape = null;
        if (vertical && horizontal) {
            shape = defaultShape;
        } else if (vertical) {
            shape = RailShape.NORTH_SOUTH;
        } else if (horizontal) {
            shape = RailShape.EAST_WEST;
        }
        if (straightOnly) {
            return shape;
        }
        RailShape[] order = hasSignal
            ? new RailShape[] {RailShape.SOUTH_EAST, RailShape.SOUTH_WEST, RailShape.NORTH_EAST, RailShape.NORTH_WEST}
            : new RailShape[] {RailShape.NORTH_WEST, RailShape.NORTH_EAST, RailShape.SOUTH_WEST, RailShape.SOUTH_EAST};
        for (RailShape candidate : order) {
            if (cornerFor(maskOf(candidate)) == candidate && (mask & maskOf(candidate)) == maskOf(candidate)) {
                shape = candidate;
            }
        }
        return shape;
    }

    private static int maskOf(RailShape corner) {
        return switch (corner) {
            case SOUTH_EAST -> kSouth | kEast;
            case SOUTH_WEST -> kSouth | kWest;
            case NORTH_WEST -> kNorth | kWest;
            case NORTH_EAST -> kNorth | kEast;
            default -> 0;
        };
    }

    public BlockState getState() {
        return current;
    }
}
