// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import com.mojang.authlib.GameProfileRepository;
import com.mojang.authlib.minecraft.SessionService;
import com.mojang.authlib.services.MinecraftServicesDiscoveryService;
import com.mojang.authlib.services.ServicesKeySet;
import java.io.File;
import net.minecraft.server.players.CachedUserNameToIdResolver;
import net.minecraft.server.players.ProfileResolver;
import net.minecraft.server.players.UserNameToIdResolver;
import net.minecraft.util.SignatureValidator;

public record Services(SessionService sessionService, ServicesKeySet servicesKeySet, GameProfileRepository profileRepository,
    UserNameToIdResolver nameToIdCache, ProfileResolver profileResolver) {
    public static Services create(MinecraftServicesDiscoveryService serviceAccess, File nameCacheDir) {
        CachedUserNameToIdResolver cache = new CachedUserNameToIdResolver(null, new File(nameCacheDir, "usercache.json"));
        return new Services(null, null, null, cache, new ProfileResolver.Cached(null, cache));
    }

    public SignatureValidator profileKeySignatureValidator() {
        return null;
    }

    public boolean canValidateProfileKeys() {
        return false;
    }
}
