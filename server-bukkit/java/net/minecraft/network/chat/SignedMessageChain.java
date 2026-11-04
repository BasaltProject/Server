// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import java.time.Instant;
import java.util.UUID;
import java.util.function.BooleanSupplier;
import net.minecraft.util.SignatureValidator;
import net.minecraft.util.Signer;
import net.minecraft.world.entity.player.ProfilePublicKey;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class SignedMessageChain {
    private static final Logger LOGGER = LoggerFactory.getLogger(SignedMessageChain.class);
    private SignedMessageLink nextLink;
    private Instant lastTimeStamp = Instant.EPOCH;
    public SignedMessageChain(UUID profileId, UUID sessionId) {
        this.nextLink = SignedMessageLink.root(profileId, sessionId);
    }
    public Encoder encoder(Signer signer) {
        return body -> {
            SignedMessageLink link = nextLink;
            if (link == null) {
                return null;
            }
            nextLink = link.advance();
            return new MessageSignature(signer.sign(output -> PlayerChatMessage.updateSignature(output, link, body)));
        };
    }
    public Decoder decoder(ProfilePublicKey profilePublicKey) {
        SignatureValidator validator = profilePublicKey.createSignatureValidator();
        return new Decoder() {
            public PlayerChatMessage unpack(MessageSignature signature, SignedMessageBody body) throws DecodeException {
                if (signature == null) {
                    throw new DecodeException(DecodeException.MISSING_PROFILE_KEY);
                }
                if (profilePublicKey.data().hasExpired()) {
                    throw new DecodeException(DecodeException.EXPIRED_PROFILE_KEY);
                }
                SignedMessageLink link = nextLink;
                if (link == null) {
                    throw new DecodeException(DecodeException.CHAIN_BROKEN);
                }
                if (body.timeStamp().isBefore(lastTimeStamp)) {
                    setChainBroken();
                    throw new DecodeException(DecodeException.OUT_OF_ORDER_CHAT);
                }
                lastTimeStamp = body.timeStamp();
                PlayerChatMessage unpacked = new PlayerChatMessage(link, signature, body, null, FilterMask.PASS_THROUGH);
                if (!unpacked.verify(validator)) {
                    setChainBroken();
                    throw new DecodeException(DecodeException.INVALID_SIGNATURE);
                }
                if (unpacked.hasExpiredServer(Instant.now())) {
                    LOGGER.warn("Received expired chat: '{}'. Is the client/server system time unsynchronized?", body.content());
                }
                nextLink = link.advance();
                return unpacked;
            }
            public void setChainBroken() {
                nextLink = null;
            }
        };
    }
    @FunctionalInterface
    public interface Encoder {
        Encoder UNSIGNED = body -> null;
        MessageSignature pack(SignedMessageBody body);
    }
    @FunctionalInterface
    public interface Decoder {
        static Decoder unsigned(UUID profileId, BooleanSupplier enforcesSecureChat) {
            return (signature, body) -> {
                if (enforcesSecureChat.getAsBoolean()) {
                    throw new DecodeException(DecodeException.MISSING_PROFILE_KEY);
                }
                return PlayerChatMessage.unsigned(profileId, body.content());
            };
        }
        PlayerChatMessage unpack(MessageSignature signature, SignedMessageBody body) throws DecodeException;
        default void setChainBroken() {
        }
    }
    public static class DecodeException extends ThrowingComponent {
        static final Component MISSING_PROFILE_KEY = Component.translatable("chat.disabled.missingProfileKey");
        static final Component CHAIN_BROKEN = Component.translatable("chat.disabled.chain_broken");
        static final Component EXPIRED_PROFILE_KEY = Component.translatable("chat.disabled.expiredProfileKey");
        static final Component INVALID_SIGNATURE = Component.translatable("chat.disabled.invalid_signature");
        static final Component OUT_OF_ORDER_CHAT = Component.translatable("chat.disabled.out_of_order_chat");
        public DecodeException(Component component) {
            super(component);
        }
    }
}
