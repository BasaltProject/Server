// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.event;
import jdk.jfr.Category;
import jdk.jfr.DataAmount;
import jdk.jfr.Enabled;
import jdk.jfr.Event;
import jdk.jfr.EventType;
import jdk.jfr.Label;
import jdk.jfr.Name;
import jdk.jfr.Period;
import jdk.jfr.StackTrace;
@Name("minecraft.NetworkSummary")
@Label("Network Summary")
@Category({"Minecraft", "Network"})
@StackTrace(false)
@Enabled(false)
@Period("10 s")
public class NetworkSummaryEvent extends Event {
    public static final String EVENT_NAME = "minecraft.NetworkSummary";
    public static final EventType TYPE = EventType.getEventType(NetworkSummaryEvent.class);
    @Name(Fields.REMOTE_ADDRESS)
    @Label("Remote Address")
    public final String remoteAddress;
    @Name(Fields.SENT_BYTES)
    @Label("Sent Bytes")
    @DataAmount
    public long sentBytes;
    @Name("sentPackets")
    @Label("Sent Packets")
    public int sentPackets;
    @Name(Fields.RECEIVED_BYTES)
    @Label("Received Bytes")
    @DataAmount
    public long receivedBytes;
    @Name("receivedPackets")
    @Label("Received Packets")
    public int receivedPackets;
    public NetworkSummaryEvent(String remoteAddress) {
        this.remoteAddress = remoteAddress;
    }
    public static final class Fields {
        public static final String REMOTE_ADDRESS = "remoteAddress";
        public static final String SENT_BYTES = "sentBytes";
        public static final String RECEIVED_BYTES = "receivedBytes";
        private Fields() {
        }
    }
    public static final class SumAggregation {
        private final NetworkSummaryEvent event;
        private final java.util.concurrent.atomic.AtomicLong sentBytes = new java.util.concurrent.atomic.AtomicLong();
        private final java.util.concurrent.atomic.AtomicInteger sentPackets = new java.util.concurrent.atomic.AtomicInteger();
        private final java.util.concurrent.atomic.AtomicLong receivedBytes = new java.util.concurrent.atomic.AtomicLong();
        private final java.util.concurrent.atomic.AtomicInteger receivedPackets = new java.util.concurrent.atomic.AtomicInteger();
        public SumAggregation(String remoteAddress) {
            this.event = new NetworkSummaryEvent(remoteAddress);
            event.begin();
        }
        public void trackSentPacket(int size) {
            sentPackets.incrementAndGet();
            sentBytes.addAndGet(size);
        }
        public void trackReceivedPacket(int size) {
            receivedPackets.incrementAndGet();
            receivedBytes.addAndGet(size);
        }
        public void commitEvent() {
            event.sentBytes = sentBytes.get();
            event.sentPackets = sentPackets.get();
            event.receivedBytes = receivedBytes.get();
            event.receivedPackets = receivedPackets.get();
            event.commit();
        }
    }
}
