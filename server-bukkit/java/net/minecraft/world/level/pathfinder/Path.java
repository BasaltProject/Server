// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;
public final class Path {
    public static final StreamCodec<ByteBuf, Path> DEBUG_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.pathfinder.Path.class, "DEBUG_STREAM_CODEC");
    private final List<Node> nodes;
    private DebugData debugData;
    private int nextNodeIndex;
    private final BlockPos target;
    private final float distToTarget;
    private final boolean reached;
    public Path(List<Node> nodes, BlockPos target, boolean reached) {
        this.nodes = nodes;
        this.target = target;
        this.distToTarget = nodes.isEmpty() ? Float.MAX_VALUE : nodes.get(nodes.size() - 1).distanceManhattan(target);
        this.reached = reached;
    }
    public void advance() {
        nextNodeIndex++;
    }
    public boolean notStarted() {
        return nextNodeIndex <= 0;
    }
    public boolean isDone() {
        return nextNodeIndex >= nodes.size();
    }
    public Node getEndNode() {
        return nodes.isEmpty() ? null : nodes.get(nodes.size() - 1);
    }
    public Node getNode(int i) {
        return nodes.get(i);
    }
    public void truncateNodes(int index) {
        if (nodes.size() > index) {
            nodes.subList(index, nodes.size()).clear();
        }
    }
    public void replaceNode(int index, Node replaceWith) {
        nodes.set(index, replaceWith);
    }
    public int getNodeCount() {
        return nodes.size();
    }
    public int getNextNodeIndex() {
        return nextNodeIndex;
    }
    public void setNextNodeIndex(int nextNodeIndex) {
        this.nextNodeIndex = nextNodeIndex;
    }
    public Vec3 getEntityPosAtNode(Entity entity, int index) {
        Node node = nodes.get(index);
        double x = node.x + (int) (entity.getBbWidth() + 1.0F) * 0.5;
        return new Vec3(x, node.y, node.z + (int) (entity.getBbWidth() + 1.0F) * 0.5);
    }
    public BlockPos getNodePos(int index) {
        return nodes.get(index).asBlockPos();
    }
    public Vec3 getNextEntityPos(Entity entity) {
        return getEntityPosAtNode(entity, nextNodeIndex);
    }
    public BlockPos getNextNodePos() {
        return nodes.get(nextNodeIndex).asBlockPos();
    }
    public Node getNextNode() {
        return nodes.get(nextNodeIndex);
    }
    public Node getPreviousNode() {
        return nextNodeIndex > 0 ? nodes.get(nextNodeIndex - 1) : null;
    }
    public boolean sameAs(Path path) {
        if (path == null || path.nodes.size() != nodes.size()) {
            return false;
        }
        for (int i = 0; i < nodes.size(); i++) {
            Node a = nodes.get(i);
            Node b = path.nodes.get(i);
            if (a.x != b.x || a.y != b.y || a.z != b.z) {
                return false;
            }
        }
        return true;
    }
    public boolean equals(Object obj) {
        return obj instanceof Path path && nodes.equals(path.nodes);
    }
    public int hashCode() {
        return nodes.hashCode();
    }
    public boolean canReach() {
        return reached;
    }
    void setDebug(List<Node> openSet, List<Node> closedSet, Set<Target> targets) {
        debugData = new DebugData(List.copyOf(openSet), List.copyOf(closedSet), Set.copyOf(targets));
    }
    public DebugData debugData() {
        return debugData;
    }
    public String toString() {
        return "Path(length=" + nodes.size() + ")";
    }
    public BlockPos getTarget() {
        return target;
    }
    public float getDistToTarget() {
        return distToTarget;
    }
    public Path copy() {
        Path copy = new Path(nodes, target, reached);
        copy.debugData = debugData;
        copy.nextNodeIndex = nextNodeIndex;
        return copy;
    }
    public static record DebugData(List<Node> openSet, List<Node> closedSet, Set<Target> targetNodes) {
        public static final StreamCodec<ByteBuf, DebugData> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.pathfinder.Path.DebugData.class, "STREAM_CODEC");
    }
}
