// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;
import net.minecraft.network.codec.ByteBufCodecs;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.security.PublicKey;
import java.time.Duration;
import java.time.Instant;
import java.util.UUID;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ThrowingComponent;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.SignatureValidator;
public record ProfilePublicKey(Data data) {
    public static final Component EXPIRED_PROFILE_PUBLIC_KEY = Component.translatable("multiplayer.disconnect.expired_public_key");
    private static final Component INVALID_SIGNATURE = Component.translatable("multiplayer.disconnect.invalid_public_key_signature");
    public static final Duration EXPIRY_GRACE_PERIOD = Duration.ofHours(8L);
    public static final Codec<ProfilePublicKey> TRUSTED_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.player.ProfilePublicKey.class, "TRUSTED_CODEC");
    public static ProfilePublicKey createValidated(SignatureValidator validator, UUID profileId, Data data) throws ValidationException {
        if (!data.validateSignature(validator, profileId)) {
            throw new ValidationException(INVALID_SIGNATURE);
        }
        return new ProfilePublicKey(data);
    }
    public SignatureValidator createSignatureValidator() {
        return SignatureValidator.from(data.key, "SHA256withRSA");
    }
    public static record Data(Instant expiresAt, PublicKey key, byte[] keySignature) {
        public static final Codec<Data> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.player.ProfilePublicKey.Data.class, "CODEC");
        public static final StreamCodec<ByteBuf, Data> STREAM_CODEC = StreamCodec.composite(ByteBufCodecs.INSTANT, Data::expiresAt, ByteBufCodecs.PUBLIC_KEY, Data::key,
            ByteBufCodecs.byteArray(4096), Data::keySignature, Data::new);
        private boolean validateSignature(SignatureValidator validator, UUID profileId) {
            byte[] keyBytes = key.getEncoded();
            java.nio.ByteBuffer payload = java.nio.ByteBuffer.allocate(24 + keyBytes.length);
            payload.putLong(profileId.getMostSignificantBits()).putLong(profileId.getLeastSignificantBits()).putLong(expiresAt.toEpochMilli()).put(keyBytes);
            return validator.validate(payload.array(), keySignature);
        }
        public boolean hasExpired() {
            return expiresAt.isBefore(Instant.now());
        }
        public boolean hasExpired(Duration gracePeriod) {
            return expiresAt.plus(gracePeriod).isBefore(Instant.now());
        }
        @Override
        public boolean equals(Object o) {
            return o instanceof Data data && expiresAt.equals(data.expiresAt) && key.equals(data.key) && java.util.Arrays.equals(keySignature, data.keySignature);
        }
        @Override
        public int hashCode() {
            return java.util.Objects.hash(expiresAt, key, java.util.Arrays.hashCode(keySignature));
        }
    }
    public static class ValidationException extends ThrowingComponent {
        public ValidationException(Component component) {
            super(component);
        }
    }
}
