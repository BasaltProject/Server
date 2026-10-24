package org.basaltmc.bukkit.player;

import java.util.LinkedList;
import org.bukkit.conversations.Conversation;
import org.bukkit.conversations.ConversationAbandonedEvent;

public final class PlayerConversations {
    private final LinkedList<Conversation> queue = new LinkedList<>();

    public synchronized boolean begin(Conversation conversation) {
        if (queue.contains(conversation)) {
            return true;
        }
        queue.addLast(conversation);
        if (queue.getFirst() == conversation) {
            conversation.begin();
            conversation.outputNextPrompt();
            ConversationRouter.watch(conversation);
        }
        return true;
    }

    public synchronized void abandon(Conversation conversation, ConversationAbandonedEvent details) {
        if (queue.isEmpty()) {
            return;
        }
        if (queue.getFirst() == conversation) {
            conversation.abandon(details);
        }
        queue.remove(conversation);
        if (!queue.isEmpty()) {
            queue.getFirst().outputNextPrompt();
        }
    }

    public synchronized boolean isConversing() {
        return !queue.isEmpty() && queue.getFirst().isModal();
    }

    public synchronized void accept(String input) {
        if (isConversing()) {
            queue.getFirst().acceptInput(input);
        }
    }
}
