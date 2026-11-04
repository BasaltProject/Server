// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import net.minecraft.network.protocol.Packet;
public class PacketProcessor implements AutoCloseable {
    private final Queue<ListenerAndPacket<?>> packetsToBeHandled = new ConcurrentLinkedQueue<>();
    private final Thread runningThread;
    private boolean closed;
    public PacketProcessor(Thread runningThread) {
        this.runningThread = runningThread;
    }
    public boolean isSameThread() {
        return Thread.currentThread() == runningThread;
    }
    public <T extends PacketListener> void scheduleIfPossible(T listener, Packet<T> packet) {
        if (!closed) {
            packetsToBeHandled.add(new ListenerAndPacket<>(listener, packet));
        }
    }
    public void processQueuedPackets() {
        ListenerAndPacket<?> next;
        while ((next = packetsToBeHandled.poll()) != null) {
            next.handle();
        }
    }
    @Override
    public void close() {
        closed = true;
        packetsToBeHandled.clear();
    }
    static record ListenerAndPacket<T extends PacketListener>(T listener, Packet<T> packet) {
        public void handle() {
            packet.handle(listener);
        }
    }
}
