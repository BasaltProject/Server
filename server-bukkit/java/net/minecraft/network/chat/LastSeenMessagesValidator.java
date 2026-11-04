// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import java.util.ArrayList;
import java.util.List;
public class LastSeenMessagesValidator {
    private final int lastSeenCount;
    private final List<LastSeenTrackedEntry> trackedMessages = new ArrayList<>();
    private MessageSignature lastPendingMessage;
    public LastSeenMessagesValidator(int lastSeenCount) {
        this.lastSeenCount = lastSeenCount;
        for (int i = 0; i < lastSeenCount; i++) {
            trackedMessages.add(null);
        }
    }
    public void addPending(MessageSignature message) {
        if (!message.equals(lastPendingMessage)) {
            trackedMessages.add(new LastSeenTrackedEntry(message, true));
            lastPendingMessage = message;
        }
    }
    public int trackedMessagesCount() {
        return trackedMessages.size();
    }
    public void applyOffset(int offset) throws ValidationException {
        int maxOffset = trackedMessages.size() - lastSeenCount;
        if (offset < 0 || offset > maxOffset) {
            throw new ValidationException("Advanced last seen window by " + offset + " messages, but expected at most " + maxOffset);
        }
        trackedMessages.subList(0, offset).clear();
    }
    public LastSeenMessages applyUpdate(LastSeenMessages.Update update) throws ValidationException {
        applyOffset(update.offset());
        List<MessageSignature> lastSeenEntries = new ArrayList<>(update.acknowledged().cardinality());
        if (update.acknowledged().length() > lastSeenCount) {
            throw new ValidationException("Last seen update contained " + update.acknowledged().length() + " messages, but maximum window size is " + lastSeenCount);
        }
        for (int i = 0; i < lastSeenCount; i++) {
            boolean acknowledged = update.acknowledged().get(i);
            LastSeenTrackedEntry message = trackedMessages.get(i);
            if (acknowledged) {
                if (message == null) {
                    throw new ValidationException("Last seen update acknowledged unknown or previously ignored message at index " + i);
                }
                trackedMessages.set(i, message.acknowledge());
                lastSeenEntries.add(message.signature());
            } else {
                if (message != null && !message.pending()) {
                    throw new ValidationException("Last seen update ignored previously acknowledged message at index " + i + " and signature " + message.signature());
                }
                trackedMessages.set(i, null);
            }
        }
        LastSeenMessages lastSeen = new LastSeenMessages(lastSeenEntries);
        if (!update.verifyChecksum(lastSeen)) {
            throw new ValidationException("Checksum mismatch on last seen update: the client and server must have desynced");
        }
        return lastSeen;
    }
    public static class ValidationException extends Exception {
        public ValidationException(String message) {
            super(message);
        }
    }
}
