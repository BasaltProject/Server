// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.mojang.authlib.GameProfile;
import com.mojang.authlib.minecraft.SessionService;
import com.mojang.datafixers.util.Either;
import java.util.Optional;
import java.util.UUID;

public interface ProfileResolver {
    Optional<GameProfile> fetchByName(String name);

    Optional<GameProfile> fetchById(UUID id);

    default Optional<GameProfile> fetchByNameOrId(Either<String, UUID> nameOrId) {
        return nameOrId.map(this::fetchByName, this::fetchById);
    }

    public static class Cached implements ProfileResolver {
        private final UserNameToIdResolver cache;

        public Cached(SessionService sessionService, UserNameToIdResolver nameToIdCache) {
            this.cache = nameToIdCache;
        }

        public Optional<GameProfile> fetchByName(String name) {
            return cache.get(name).map(entry -> new GameProfile(entry.id(), entry.name()));
        }

        public Optional<GameProfile> fetchById(UUID id) {
            return cache.get(id).map(entry -> new GameProfile(entry.id(), entry.name()));
        }
    }
}
