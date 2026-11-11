// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;

import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.players.NameAndId;

public interface MinecraftPlayerListService {
    List<ServerPlayer> getPlayers();

    ServerPlayer getPlayer(UUID uuid);

    default CompletableFuture<Optional<NameAndId>> getUser(Optional<UUID> id, Optional<String> name) {
        if (id.isPresent()) {
            Optional<NameAndId> known = getCachedUserById(id.get());
            return CompletableFuture.completedFuture(known.isPresent() ? known : fetchUserById(id.get()));
        }
        return CompletableFuture.supplyAsync(() -> name.flatMap(this::fetchUserByName));
    }

    Optional<NameAndId> fetchUserByName(String name);

    Optional<NameAndId> fetchUserById(UUID id);

    Optional<NameAndId> getCachedUserById(UUID id);

    Optional<ServerPlayer> getPlayer(Optional<UUID> id, Optional<String> name);

    List<ServerPlayer> getPlayersWithAddress(String ip);

    ServerPlayer getPlayerByName(String name);

    void remove(ServerPlayer player, ClientInfo clientInfo);
}
