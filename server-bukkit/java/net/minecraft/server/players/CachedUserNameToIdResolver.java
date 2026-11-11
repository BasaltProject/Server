// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.mojang.authlib.GameProfileRepository;
import java.io.File;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

public class CachedUserNameToIdResolver implements UserNameToIdResolver {
    private final Map<String, NameAndId> byName = new ConcurrentHashMap<>();
    private final Map<UUID, NameAndId> byId = new ConcurrentHashMap<>();
    private volatile boolean offlineFallback;

    public CachedUserNameToIdResolver(GameProfileRepository profileRepository, File file) {}

    public void resolveOfflineUsers(boolean value) {
        offlineFallback = value;
    }

    public void add(NameAndId nameAndId) {
        NameAndId previous = byId.put(nameAndId.id(), nameAndId);
        if (previous != null) {
            byName.remove(previous.name().toLowerCase(Locale.ROOT));
        }
        byName.put(nameAndId.name().toLowerCase(Locale.ROOT), nameAndId);
    }

    public Optional<NameAndId> get(String name) {
        String key = name.toLowerCase(Locale.ROOT);
        NameAndId cached = byName.get(key);
        if (cached != null) {
            return Optional.of(cached);
        }
        org.bukkit.OfflinePlayer known = org.bukkit.Bukkit.getOfflinePlayerIfCached(name);
        if (known != null && known.getName() != null) {
            NameAndId found = new NameAndId(known.getUniqueId(), known.getName());
            add(found);
            return Optional.of(found);
        }
        return offlineFallback ? Optional.of(NameAndId.createOffline(name)) : Optional.empty();
    }

    public Optional<NameAndId> get(UUID id) {
        NameAndId cached = byId.get(id);
        if (cached != null) {
            return Optional.of(cached);
        }
        org.bukkit.OfflinePlayer known = org.bukkit.Bukkit.getOfflinePlayer(id);
        if (known.getName() != null && (known.hasPlayedBefore() || known.isOnline())) {
            NameAndId found = new NameAndId(id, known.getName());
            add(found);
            return Optional.of(found);
        }
        return Optional.empty();
    }

    public void save() {}
}
