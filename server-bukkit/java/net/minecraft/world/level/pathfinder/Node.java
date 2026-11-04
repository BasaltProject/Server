// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
import com.mojang.datafixers.util.Function3;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.phys.Vec3;
public class Node {
    public static final StreamCodec<ByteBuf, Node> DEBUG_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.pathfinder.Node.class, "DEBUG_STREAM_CODEC");
    public final int x;
    public final int y;
    public final int z;
    public int heapIdx = -1;
    public float g;
    public float h;
    public float f;
    public Node cameFrom;
    public boolean closed;
    public float walkedDistance;
    public float costMalus;
    public PathType type = PathType.BLOCKED;
    protected static <N extends Node> StreamCodec<ByteBuf, N> createDebugStreamCodec(Function3<Integer, Integer, Integer, N> factory) {
        return org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(Node.class, "DEBUG_STREAM_CODEC");
    }
    public Node(int x, int y, int z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }
    public Node cloneAndMove(int x, int y, int z) {
        Node node = new Node(x, y, z);
        node.heapIdx = heapIdx;
        node.g = g;
        node.h = h;
        node.f = f;
        node.cameFrom = cameFrom;
        node.closed = closed;
        node.walkedDistance = walkedDistance;
        node.costMalus = costMalus;
        node.type = type;
        return node;
    }
    public static int createHash(int x, int y, int z) {
        return (y & 0xFF) | ((x & 32767) << 8) | ((z & 32767) << 24) | (x < 0 ? Integer.MIN_VALUE : 0) | (z < 0 ? 32768 : 0);
    }
    public float distanceTo(Node to) {
        return (float) Math.sqrt(distanceToSqr(to));
    }
    public float distanceToXZ(Node to) {
        float dx = to.x - x;
        float dz = to.z - z;
        return (float) Math.sqrt(dx * dx + dz * dz);
    }
    public float distanceTo(BlockPos pos) {
        return (float) Math.sqrt(distanceToSqr(pos));
    }
    public float distanceToSqr(Node to) {
        float dx = to.x - x;
        float dy = to.y - y;
        float dz = to.z - z;
        return dx * dx + dy * dy + dz * dz;
    }
    public float distanceToSqr(BlockPos pos) {
        float dx = pos.getX() - x;
        float dy = pos.getY() - y;
        float dz = pos.getZ() - z;
        return dx * dx + dy * dy + dz * dz;
    }
    public float distanceManhattan(Node to) {
        return Math.abs(to.x - x) + Math.abs(to.y - y) + Math.abs(to.z - z);
    }
    public float distanceManhattan(BlockPos pos) {
        return Math.abs(pos.getX() - x) + Math.abs(pos.getY() - y) + Math.abs(pos.getZ() - z);
    }
    public BlockPos asBlockPos() {
        return new BlockPos(x, y, z);
    }
    public Vec3 asVec3() {
        return new Vec3(x, y, z);
    }
    public boolean equals(Object o) {
        return o instanceof Node node && x == node.x && y == node.y && z == node.z;
    }
    public int hashCode() {
        return createHash(x, y, z);
    }
    public boolean inOpenSet() {
        return heapIdx >= 0;
    }
    public String toString() {
        return "Node{x=" + x + ", y=" + y + ", z=" + z + "}";
    }
}
