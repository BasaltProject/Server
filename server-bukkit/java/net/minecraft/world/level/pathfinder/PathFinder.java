// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import com.google.common.collect.Lists;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.BooleanSupplier;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.PathNavigationRegion;
public class PathFinder {
    private final Node[] neighbors = new Node[32];
    private int maxVisitedNodes;
    private final NodeEvaluator nodeEvaluator;
    private final BinaryHeap openSet = new BinaryHeap();
    private BooleanSupplier captureDebug = () -> false;
    public PathFinder(NodeEvaluator nodeEvaluator, int maxVisitedNodes) {
        this.nodeEvaluator = nodeEvaluator;
        this.maxVisitedNodes = maxVisitedNodes;
    }
    public void setCaptureDebug(BooleanSupplier captureDebug) {
        this.captureDebug = captureDebug;
    }
    public void setMaxVisitedNodes(int maxVisitedNodes) {
        this.maxVisitedNodes = maxVisitedNodes;
    }
    public Path findPath(PathNavigationRegion level, Mob entity, Set<BlockPos> targets, float maxPathLength, int reachRange, float maxVisitedNodesMultiplier) {
        openSet.clear();
        nodeEvaluator.prepare(level, entity);
        Node start = nodeEvaluator.getStart();
        if (start == null) {
            nodeEvaluator.done();
            return null;
        }
        java.util.Map<Target, BlockPos> goals = new java.util.HashMap<>();
        for (BlockPos pos : targets) {
            goals.put(nodeEvaluator.getTarget(pos.getX(), pos.getY(), pos.getZ()), pos);
        }
        Path path = search(start, goals, maxPathLength, reachRange, maxVisitedNodesMultiplier);
        nodeEvaluator.done();
        return path;
    }
    private Path search(Node start, java.util.Map<Target, BlockPos> goals, float maxPathLength, int reachRange, float multiplier) {
        Set<Target> targetSet = goals.keySet();
        start.g = 0.0F;
        start.h = bestHeuristic(start, targetSet);
        start.f = start.h;
        openSet.clear();
        openSet.insert(start);
        List<Node> closed = new ArrayList<>();
        int budget = (int) (maxVisitedNodes * multiplier);
        int visited = 0;
        while (!openSet.isEmpty() && ++visited < budget) {
            Node current = openSet.pop();
            current.closed = true;
            closed.add(current);
            boolean reachedAny = false;
            for (Target target : targetSet) {
                if (current.distanceManhattan(target) <= reachRange) {
                    target.setReached();
                    reachedAny = true;
                }
            }
            if (reachedAny) {
                break;
            }
            if (current.distanceTo(start) >= maxPathLength) {
                continue;
            }
            int count = nodeEvaluator.getNeighbors(neighbors, current);
            for (int i = 0; i < count; i++) {
                Node next = neighbors[i];
                float step = distance(current, next);
                next.walkedDistance = current.walkedDistance + step;
                float newG = current.g + step + next.costMalus;
                if (next.walkedDistance < maxPathLength && (!next.inOpenSet() || newG < next.g)) {
                    next.cameFrom = current;
                    next.g = newG;
                    next.h = bestHeuristic(next, targetSet) * 1.5F;
                    if (next.inOpenSet()) {
                        openSet.changeCost(next, next.g + next.h);
                    } else {
                        next.f = next.g + next.h;
                        openSet.insert(next);
                    }
                }
            }
        }
        Path best = null;
        boolean anyReached = targetSet.stream().anyMatch(Target::isReached);
        for (Target target : targetSet) {
            if (anyReached && !target.isReached()) {
                continue;
            }
            Path candidate = build(target.getBestNode(), goals.get(target), target.isReached());
            if (candidate != null && (best == null || candidate.getNodeCount() < best.getNodeCount())) {
                best = candidate;
            }
        }
        if (best != null && captureDebug.getAsBoolean()) {
            best.setDebug(Lists.newArrayList(openSet.getHeap()), closed, new HashSet<>(targetSet));
        }
        return best;
    }
    protected float distance(Node from, Node to) {
        return from.distanceTo(to);
    }
    private float bestHeuristic(Node node, Set<Target> targets) {
        float best = Float.MAX_VALUE;
        for (Target target : targets) {
            float h = node.distanceTo(target);
            target.updateBest(h, node);
            best = Math.min(h, best);
        }
        return best;
    }
    private Path build(Node end, BlockPos target, boolean reached) {
        if (end == null) {
            return null;
        }
        List<Node> reversed = Lists.newArrayList();
        for (Node node = end; node != null; node = node.cameFrom) {
            reversed.add(0, node);
        }
        return new Path(reversed, target, reached);
    }
}
