// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.security;

import io.netty.handler.ssl.SslContext;
import io.netty.handler.ssl.SslContextBuilder;
import java.io.File;
import java.io.InputStream;
import java.nio.file.Files;
import java.security.KeyStore;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.TrustManagerFactory;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class JsonRpcSslContextProvider {
    private static final Logger LOG = LoggerFactory.getLogger(JsonRpcSslContextProvider.class);

    public static SslContext createFrom(String keystorePath, String keystorePasswordFromServerProperties) throws Exception {
        if (keystorePath.isEmpty()) {
            throw new IllegalArgumentException("TLS is enabled but keystore is not configured");
        }
        File keystore = new File(keystorePath);
        if (!keystore.isFile()) {
            throw new IllegalArgumentException("Supplied keystore is not a file or does not exist: '" + keystorePath + "'");
        }
        char[] password = resolvePassword(keystorePasswordFromServerProperties).toCharArray();
        KeyStore store = KeyStore.getInstance("PKCS12");
        try (InputStream in = Files.newInputStream(keystore.toPath())) {
            store.load(in, password);
        }
        KeyManagerFactory keys = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        keys.init(store, password);
        TrustManagerFactory trust = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
        trust.init(store);
        return SslContextBuilder.forServer(keys).trustManager(trust).build();
    }

    private static String resolvePassword(String fromProperties) {
        String env = System.getenv("MINECRAFT_MANAGEMENT_TLS_KEYSTORE_PASSWORD");
        return env != null ? env : System.getProperty("management.tls.keystore.password", fromProperties);
    }

    public static void printInstructions() {
        LOG.info("To use TLS for the management server, please follow these steps:");
        LOG.info("1. Set the server property 'management-server-tls-enabled' to 'true' to enable TLS");
        LOG.info("2. Create a keystore file of type PKCS12 containing your server certificate and private key");
        LOG.info("3. Set the server property 'management-server-tls-keystore' to the path of your keystore file");
        LOG.info("4. Set the keystore password through the environment variable 'MINECRAFT_MANAGEMENT_TLS_KEYSTORE_PASSWORD', "
            + "the system property 'management.tls.keystore.password' or the server property 'management-server-tls-keystore-password'");
        LOG.info("5. Restart the server to apply the changes.");
    }
}
