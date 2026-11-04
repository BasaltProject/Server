// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import net.minecraft.network.codec.ByteBufCodecs;
import com.mojang.authlib.GameProfile;
import io.netty.buffer.ByteBuf;
import java.time.Duration;
import java.util.UUID;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.SignatureValidator;
import net.minecraft.world.entity.player.ProfilePublicKey;
public record RemoteChatSession(UUID sessionId, ProfilePublicKey profilePublicKey) {
    public SignedMessageValidator createMessageValidator(Duration gracePeriod) {
        return new SignedMessageValidator.KeyBased(profilePublicKey.createSignatureValidator(), () -> profilePublicKey.data().hasExpired(gracePeriod));
    }
    public SignedMessageChain.Decoder createMessageDecoder(UUID profileId) {
        return new SignedMessageChain(profileId, sessionId).decoder(profilePublicKey);
    }
    public Data asData() {
        return new Data(sessionId, profilePublicKey.data());
    }
    public boolean hasExpired() {
        return profilePublicKey.data().hasExpired();
    }
    public static record Data(UUID sessionId, ProfilePublicKey.Data profilePublicKey) {
        public static final StreamCodec<ByteBuf, Data> STREAM_CODEC = StreamCodec.composite(net.minecraft.core.UUIDUtil.STREAM_CODEC, Data::sessionId, ProfilePublicKey.Data.STREAM_CODEC, Data::profilePublicKey, Data::new);
        public RemoteChatSession validate(GameProfile profile, SignatureValidator serviceSignatureValidator) throws ProfilePublicKey.ValidationException {
            return new RemoteChatSession(sessionId, ProfilePublicKey.createValidated(serviceSignatureValidator, profile.id(), profilePublicKey));
        }
    }
}
