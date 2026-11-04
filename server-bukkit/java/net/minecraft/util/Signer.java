// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import java.security.PrivateKey;
import java.security.Signature;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public interface Signer {
    Logger LOGGER = LoggerFactory.getLogger(Signer.class);
    byte[] sign(SignatureUpdater updater);
    default byte[] sign(byte[] payload) {
        return sign(output -> output.update(payload));
    }
    static Signer from(PrivateKey privateKey, String algorithm) {
        return updater -> {
            try {
                Signature signature = Signature.getInstance(algorithm);
                signature.initSign(privateKey);
                updater.update(signature::update);
                return signature.sign();
            } catch (Exception e) {
                throw new IllegalStateException("Failed to sign message", e);
            }
        };
    }
}
