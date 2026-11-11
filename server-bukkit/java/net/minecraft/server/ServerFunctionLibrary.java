// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import com.mojang.brigadier.CommandDispatcher;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.functions.CommandFunction;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.packs.resources.PreparableReloadListener;
import net.minecraft.server.permissions.PermissionSet;

public class ServerFunctionLibrary implements PreparableReloadListener {
    public static final ResourceKey<Registry<CommandFunction<CommandSourceStack>>> TYPE_KEY = ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("function"));
    private final Map<Identifier, CommandFunction<CommandSourceStack>> functions = new ConcurrentHashMap<>();
    private final Map<Identifier, List<CommandFunction<CommandSourceStack>>> tags = new ConcurrentHashMap<>();

    public ServerFunctionLibrary(PermissionSet functionCompilationPermissions, CommandDispatcher<CommandSourceStack> dispatcher) {}

    public Optional<CommandFunction<CommandSourceStack>> getFunction(Identifier id) {
        return Optional.ofNullable(functions.get(id));
    }

    public Map<Identifier, CommandFunction<CommandSourceStack>> getFunctions() {
        return Map.copyOf(functions);
    }

    public List<CommandFunction<CommandSourceStack>> getTag(Identifier tag) {
        return tags.getOrDefault(tag, List.of());
    }

    public Iterable<Identifier> getAvailableTags() {
        return tags.keySet();
    }

    public CompletableFuture<Void> reload(PreparableReloadListener.SharedState currentReload, Executor taskExecutor, PreparableReloadListener.PreparationBarrier preparationBarrier, Executor reloadExecutor) {
        return preparationBarrier.wait(null).thenAcceptAsync(ignored -> {
            functions.clear();
            tags.clear();
        }, reloadExecutor);
    }
}
