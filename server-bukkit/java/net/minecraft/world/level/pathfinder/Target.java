// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
public class Target extends Node {
    public static final StreamCodec<ByteBuf, Target> DEBUG_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.pathfinder.Target.class, "DEBUG_STREAM_CODEC");
    private float bestHeuristic = Float.MAX_VALUE;
    private Node bestNode;
    private boolean reached;
    public Target(Node node) {
        super(node.x, node.y, node.z);
    }
    public Target(int x, int y, int z) {
        super(x, y, z);
    }
    public void updateBest(float heuristic, Node node) {
        if (heuristic < bestHeuristic) {
            bestHeuristic = heuristic;
            bestNode = node;
        }
    }
    public Node getBestNode() {
        return bestNode;
    }
    public void setReached() {
        reached = true;
    }
    public boolean isReached() {
        return reached;
    }
}
