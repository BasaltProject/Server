package org.basaltmc.bukkit.player;

import java.util.LinkedList;
import org.bukkit.conversations.Conversation;
import org.bukkit.conversations.ConversationAbandonedEvent;

final class PlayerConversations {
    private final LinkedList<Conversation> queue = new LinkedList<>();

    synchronized boolean begin(Conversation conversation) {
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

    synchronized void abandon(Conversation conversation, ConversationAbandonedEvent details) {
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

    synchronized boolean isConversing() {
        return !queue.isEmpty() && queue.getFirst().isModal();
    }

    synchronized void accept(String input) {
        if (isConversing()) {
            queue.getFirst().acceptInput(input);
        }
    }
}
