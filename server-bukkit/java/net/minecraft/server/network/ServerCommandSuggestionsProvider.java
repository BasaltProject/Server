// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.suggestion.Suggestions;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.atomic.AtomicReference;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerPlayer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class ServerCommandSuggestionsProvider {
    private static final Logger LOG = LoggerFactory.getLogger(ServerCommandSuggestionsProvider.class);
    private static final int MAX_SUGGESTIONS = 1000;
    private final ServerPlayer player;
    private final AtomicReference<Request> latest = new AtomicReference<>();
    private volatile boolean hasBudget;

    public ServerCommandSuggestionsProvider(ServerPlayer player) {
        this.player = player;
    }

    public void tick() {
        hasBudget = true;
        process();
    }

    private void process() {
        if (!hasBudget) {
            return;
        }
        Request request = latest.getAndSet(null);
        if (request == null) {
            return;
        }
        hasBudget = false;
        try {
            StringReader reader = new StringReader(request.command);
            if (reader.canRead() && reader.peek() == '/') {
                reader.skip();
            }
            var dispatcher = player.level().getServer().getCommands().getDispatcher();
            var parsed = dispatcher.parse(reader, player.createCommandSourceStack());
            if (latest.get() == null) {
                dispatcher.getCompletionSuggestions(parsed).thenAccept(found -> request.future.complete(capped(found)));
            }
        } catch (Exception failure) {
            LOG.error("Failed to resolve command suggestions for {}", player.getGameProfile().name(), failure);
        }
    }

    private static Suggestions capped(Suggestions found) {
        return found.getList().size() <= MAX_SUGGESTIONS ? found : new Suggestions(found.getRange(), found.getList().subList(0, MAX_SUGGESTIONS));
    }

    public CompletableFuture<Suggestions> request(String command) {
        Request request = new Request(command);
        if (latest.getAndSet(request) == null && hasBudget) {
            MinecraftServer.getServer().execute(this::process);
        }
        return request.future;
    }

    static class Request {
        private final String command;
        private final CompletableFuture<Suggestions> future = new CompletableFuture<>();

        private Request(String command) {
            this.command = command;
        }
    }
}
