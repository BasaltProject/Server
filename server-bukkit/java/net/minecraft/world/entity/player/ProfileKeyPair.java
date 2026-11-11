// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;
import com.mojang.serialization.Codec;
import java.security.PrivateKey;
import java.time.Instant;
public record ProfileKeyPair(PrivateKey privateKey, ProfilePublicKey publicKey, Instant refreshedAfter) {
    public static final Codec<ProfileKeyPair> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.player.ProfileKeyPair.class, "CODEC");
    public boolean dueRefresh() {
        return refreshedAfter.isBefore(Instant.now());
    }
}
