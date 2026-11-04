// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.Key;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import java.util.Base64;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;
import net.minecraft.network.FriendlyByteBuf;

public class Crypt {
    public static final String SIGNING_ALGORITHM = "SHA256withRSA";
    public static final int SIGNATURE_BYTES = 256;
    public static final String RSA_PUBLIC_KEY_HEADER = "-----BEGIN RSA PUBLIC KEY-----";
    public static final String MIME_LINE_SEPARATOR = "\n";
    public static final Base64.Encoder MIME_ENCODER = Base64.getMimeEncoder(76, MIME_LINE_SEPARATOR.getBytes(StandardCharsets.UTF_8));
    public static final Codec<PublicKey> PUBLIC_KEY_CODEC = Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(stringToRsaPublicKey(text));
        } catch (CryptException e) {
            return DataResult.error(e::getMessage);
        }
    }, Crypt::rsaPublicKeyToString);
    public static final Codec<PrivateKey> PRIVATE_KEY_CODEC = Codec.STRING.comapFlatMap(text -> {
        try {
            return DataResult.success(stringToPemRsaPrivateKey(text));
        } catch (CryptException e) {
            return DataResult.error(e::getMessage);
        }
    }, Crypt::pemRsaPrivateKeyToString);

    public static SecretKey generateSecretKey() throws CryptException {
        try {
            KeyGenerator generator = KeyGenerator.getInstance("AES");
            generator.init(128);
            return generator.generateKey();
        } catch (Exception e) {
            throw new CryptException(e);
        }
    }

    public static KeyPair generateKeyPair() throws CryptException {
        try {
            KeyPairGenerator generator = KeyPairGenerator.getInstance("RSA");
            generator.initialize(1024);
            return generator.generateKeyPair();
        } catch (Exception e) {
            throw new CryptException(e);
        }
    }

    public static byte[] digestData(String serverId, PublicKey publicKey, SecretKey sharedKey) throws CryptException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-1");
            digest.update(serverId.getBytes(StandardCharsets.ISO_8859_1));
            digest.update(sharedKey.getEncoded());
            digest.update(publicKey.getEncoded());
            return digest.digest();
        } catch (Exception e) {
            throw new CryptException(e);
        }
    }

    private static String stripPem(String text, String begin, String end) {
        return text.replace(begin, "").replace(end, "").replaceAll("\\s", "");
    }

    public static PrivateKey stringToPemRsaPrivateKey(String rsaString) throws CryptException {
        try {
            byte[] bytes = Base64.getDecoder().decode(stripPem(rsaString, "-----BEGIN RSA PRIVATE KEY-----", "-----END RSA PRIVATE KEY-----"));
            return KeyFactory.getInstance("RSA").generatePrivate(new PKCS8EncodedKeySpec(bytes));
        } catch (Exception e) {
            throw new CryptException(e);
        }
    }

    public static PublicKey stringToRsaPublicKey(String rsaString) throws CryptException {
        try {
            byte[] bytes = Base64.getDecoder().decode(stripPem(rsaString, RSA_PUBLIC_KEY_HEADER, "-----END RSA PUBLIC KEY-----"));
            return byteToPublicKey(bytes);
        } catch (IllegalArgumentException e) {
            throw new CryptException(e);
        }
    }

    public static String rsaPublicKeyToString(PublicKey publicKey) {
        if (!"RSA".equals(publicKey.getAlgorithm())) {
            throw new IllegalArgumentException("Public key must be RSA");
        }
        return RSA_PUBLIC_KEY_HEADER + MIME_LINE_SEPARATOR + MIME_ENCODER.encodeToString(publicKey.getEncoded())
            + MIME_LINE_SEPARATOR + "-----END RSA PUBLIC KEY-----" + MIME_LINE_SEPARATOR;
    }

    public static String pemRsaPrivateKeyToString(PrivateKey privateKey) {
        if (!"RSA".equals(privateKey.getAlgorithm())) {
            throw new IllegalArgumentException("Private key must be RSA");
        }
        return "-----BEGIN RSA PRIVATE KEY-----" + MIME_LINE_SEPARATOR + MIME_ENCODER.encodeToString(privateKey.getEncoded())
            + MIME_LINE_SEPARATOR + "-----END RSA PRIVATE KEY-----" + MIME_LINE_SEPARATOR;
    }

    public static PublicKey byteToPublicKey(byte[] keyData) throws CryptException {
        try {
            return KeyFactory.getInstance("RSA").generatePublic(new X509EncodedKeySpec(keyData));
        } catch (GeneralSecurityException e) {
            throw new CryptException(e);
        }
    }

    public static SecretKey decryptByteToSecretKey(PrivateKey privateKey, byte[] keyData) throws CryptException {
        byte[] decrypted = decryptUsingKey(privateKey, keyData);
        try {
            return new SecretKeySpec(decrypted, "AES");
        } catch (Exception e) {
            throw new CryptException(e);
        }
    }

    public static byte[] encryptUsingKey(Key key, byte[] input) throws CryptException {
        try {
            return getCipher(1, key).doFinal(input);
        } catch (Exception e) {
            throw new CryptException(e);
        }
    }

    public static byte[] decryptUsingKey(Key key, byte[] input) throws CryptException {
        try {
            return getCipher(2, key).doFinal(input);
        } catch (Exception e) {
            throw new CryptException(e);
        }
    }

    public static Cipher getCipher(int opMode, Key key) throws CryptException {
        try {
            Cipher cipher = Cipher.getInstance(key.getAlgorithm());
            cipher.init(opMode, key);
            return cipher;
        } catch (Exception e) {
            throw new CryptException(e);
        }
    }

    public static class SaltSupplier {
        private static final SecureRandom SECURE_RANDOM = new SecureRandom();

        public static long getLong() {
            return SECURE_RANDOM.nextLong();
        }
    }

    public record SaltSignaturePair(long salt, byte[] signature) {
        public static final SaltSignaturePair EMPTY = new SaltSignaturePair(0L, new byte[0]);

        public SaltSignaturePair(FriendlyByteBuf input) {
            this(input.readLong(), input.readByteArray());
        }

        public boolean isValid() {
            return signature.length > 0;
        }

        public static void write(FriendlyByteBuf output, SaltSignaturePair saltSignaturePair) {
            output.writeLong(saltSignaturePair.salt);
            output.writeByteArray(saltSignaturePair.signature);
        }

        public byte[] saltAsBytes() {
            return java.nio.ByteBuffer.allocate(8).putLong(salt).array();
        }
    }

    interface ByteArrayToKeyFunction<T extends Key> {
        T apply(byte[] input) throws CryptException;
    }
}
