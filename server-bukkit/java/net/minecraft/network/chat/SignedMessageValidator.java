// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import java.util.function.BooleanSupplier;
import net.minecraft.util.SignatureValidator;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
@FunctionalInterface
public interface SignedMessageValidator {
    Logger LOGGER = LoggerFactory.getLogger(SignedMessageValidator.class);
    SignedMessageValidator ACCEPT_UNSIGNED = PlayerChatMessage::removeSignature;
    SignedMessageValidator REJECT_ALL = message -> {
        LOGGER.error("Received chat message from {}, but they have no chat session initialized and secure chat is enforced", message.sender());
        return null;
    };
    PlayerChatMessage updateAndValidate(PlayerChatMessage message);
    public static class KeyBased implements SignedMessageValidator {
        private final SignatureValidator validator;
        private final BooleanSupplier expired;
        private PlayerChatMessage lastMessage;
        private boolean isChainValid = true;
        public KeyBased(SignatureValidator validator, BooleanSupplier expired) {
            this.validator = validator;
            this.expired = expired;
        }
        private boolean validateChain(PlayerChatMessage message) {
            if (message.equals(lastMessage)) {
                return true;
            }
            if (lastMessage != null && !message.link().isDescendantOf(lastMessage.link())) {
                LOGGER.error("Received out-of-order chat message from {}: expected index > {} for session {}, but was {} for session {}", message.sender(), lastMessage.link().index(), lastMessage.link().sessionId(), message.link().index(), message.link().sessionId());
                return false;
            }
            return true;
        }
        private boolean validate(PlayerChatMessage message) {
            if (expired.getAsBoolean()) {
                LOGGER.error("Received message with expired profile public key from {} with session {}", message.sender(), message.link().sessionId());
                return false;
            }
            if (!message.verify(validator)) {
                LOGGER.error("Received message with invalid signature (is the session wrong, or signature cache out of sync?): {}", PlayerChatMessage.describeSigned(message));
                return false;
            }
            return validateChain(message);
        }
        public PlayerChatMessage updateAndValidate(PlayerChatMessage message) {
            isChainValid = isChainValid && validate(message);
            if (!isChainValid) {
                return null;
            }
            lastMessage = message;
            return message;
        }
    }
}
