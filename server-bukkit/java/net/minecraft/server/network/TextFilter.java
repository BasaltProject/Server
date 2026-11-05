// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import java.util.List;
import java.util.concurrent.CompletableFuture;

public interface TextFilter {
    TextFilter DUMMY = new TextFilter() {
        public CompletableFuture<FilteredText> processStreamMessage(String message) {
            return CompletableFuture.completedFuture(FilteredText.passThrough(message));
        }

        public CompletableFuture<List<FilteredText>> processMessageBundle(List<String> messages) {
            return CompletableFuture.completedFuture(messages.stream().map(FilteredText::passThrough).toList());
        }
    };

    default void join() {
    }

    default void leave() {
    }

    CompletableFuture<FilteredText> processStreamMessage(String message);

    CompletableFuture<List<FilteredText>> processMessageBundle(List<String> messages);
}
