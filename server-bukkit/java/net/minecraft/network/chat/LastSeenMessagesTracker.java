// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import java.util.ArrayList;
import java.util.BitSet;
import java.util.List;
import java.util.Objects;
public class LastSeenMessagesTracker {
    private final LastSeenTrackedEntry[] trackedMessages;
    private int tail;
    private int offset;
    private MessageSignature lastTrackedMessage;
    public LastSeenMessagesTracker(int lastSeenCount) {
        this.trackedMessages = new LastSeenTrackedEntry[lastSeenCount];
    }
    public boolean addPending(MessageSignature message, boolean wasShown) {
        if (Objects.equals(message, lastTrackedMessage)) {
            return false;
        }
        lastTrackedMessage = message;
        addEntry(wasShown ? new LastSeenTrackedEntry(message, true) : null);
        return true;
    }
    private void addEntry(LastSeenTrackedEntry entry) {
        int index = tail;
        tail = (index + 1) % trackedMessages.length;
        offset++;
        trackedMessages[index] = entry;
    }
    public void ignorePending(MessageSignature pendingMessage) {
        for (int i = 0; i < trackedMessages.length; i++) {
            LastSeenTrackedEntry entry = trackedMessages[i];
            if (entry != null && entry.pending() && pendingMessage.equals(entry.signature())) {
                trackedMessages[i] = null;
                break;
            }
        }
    }
    public int getAndClearOffset() {
        int original = offset;
        offset = 0;
        return original;
    }
    public Update generateAndApplyUpdate() {
        int originalOffset = getAndClearOffset();
        BitSet acknowledged = new BitSet(trackedMessages.length);
        List<MessageSignature> seen = new ArrayList<>(trackedMessages.length);
        for (int i = 0; i < trackedMessages.length; i++) {
            int index = (tail + i) % trackedMessages.length;
            LastSeenTrackedEntry message = trackedMessages[index];
            if (message != null) {
                acknowledged.set(i, true);
                seen.add(message.signature());
                trackedMessages[index] = message.acknowledge();
            }
        }
        LastSeenMessages lastSeen = new LastSeenMessages(seen);
        return new Update(lastSeen, new LastSeenMessages.Update(originalOffset, acknowledged, lastSeen.computeChecksum()));
    }
    public int offset() {
        return offset;
    }
    public static record Update(LastSeenMessages lastSeen, LastSeenMessages.Update update) {
    }
}
