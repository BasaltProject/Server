// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import com.mojang.authlib.services.ServicesKeySet;
import com.mojang.authlib.services.ServicesKeyType;
import java.security.PublicKey;
import java.security.Signature;
import java.security.SignatureException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public interface SignatureValidator {
    SignatureValidator NO_VALIDATION = (payload, signature) -> true;
    Logger LOGGER = LoggerFactory.getLogger(SignatureValidator.class);
    boolean validate(SignatureUpdater updater, byte[] signature);
    default boolean validate(byte[] payload, byte[] signature) {
        return validate(output -> output.update(payload), signature);
    }
    private static boolean verifySignature(SignatureUpdater updater, byte[] signature, Signature verifier) throws SignatureException {
        updater.update(verifier::update);
        return verifier.verify(signature);
    }
    static SignatureValidator from(PublicKey publicKey, String algorithm) {
        return (updater, signature) -> {
            try {
                Signature verifier = Signature.getInstance(algorithm);
                verifier.initVerify(publicKey);
                return verifySignature(updater, signature, verifier);
            } catch (Exception e) {
                LOGGER.error("Failed to verify signature", e);
                return false;
            }
        };
    }
    static SignatureValidator from(ServicesKeySet keySet, ServicesKeyType type) {
        var keys = keySet.keys(type);
        if (keys.isEmpty()) {
            return null;
        }
        return (updater, signature) -> keys.stream().anyMatch(key -> from(key, "SHA1withRSA").validate(updater, signature));
    }
}
