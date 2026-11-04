// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
public class MessageSignatureCache {
    public static final int NOT_FOUND = -1;
    private final MessageSignature[] entries;
    public MessageSignatureCache(int capacity) {
        this.entries = new MessageSignature[capacity];
    }
    public static MessageSignatureCache createDefault() {
        return new MessageSignatureCache(128);
    }
    public int pack(MessageSignature signature) {
        for (int i = 0; i < entries.length; i++) {
            if (signature.equals(entries[i])) {
                return i;
            }
        }
        return NOT_FOUND;
    }
    public MessageSignature unpack(int id) {
        return entries[id];
    }
    public void push(SignedMessageBody body, MessageSignature signature) {
        ArrayDeque<MessageSignature> queue = new ArrayDeque<>(body.lastSeen().entries());
        if (signature != null) {
            queue.add(signature);
        }
        push(queue);
    }
    void push(List<MessageSignature> entries) {
        push(new ArrayDeque<>(entries));
    }
    private void push(ArrayDeque<MessageSignature> queue) {
        Set<MessageSignature> incoming = new HashSet<>(queue);
        for (int i = 0; !queue.isEmpty() && i < entries.length; i++) {
            MessageSignature displaced = entries[i];
            entries[i] = queue.removeLast();
            if (displaced != null && !incoming.contains(displaced)) {
                queue.addFirst(displaced);
            }
        }
    }
}
