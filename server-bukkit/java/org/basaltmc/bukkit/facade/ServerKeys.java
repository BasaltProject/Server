package org.basaltmc.bukkit.facade;

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;

/** The RSA key pair a server offers to clients that authenticate, created on first use. */
public final class ServerKeys {
    private static KeyPair pair;

    private ServerKeys() {}

    public static synchronized KeyPair pair() {
        if (pair == null) {
            try {
                KeyPairGenerator generator = KeyPairGenerator.getInstance("RSA");
                generator.initialize(1024);
                pair = generator.generateKeyPair();
            } catch (NoSuchAlgorithmException e) {
                throw new IllegalStateException(e);
            }
        }
        return pair;
    }
}
