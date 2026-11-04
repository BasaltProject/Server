// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.nio.ByteBuffer;
import java.util.Optional;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.SignatureUpdater;
import net.minecraft.util.SignatureValidator;
public record MessageSignature(byte[] bytes) {
    public static final Codec<MessageSignature> CODEC = net.minecraft.util.ExtraCodecs.BASE64_STRING.xmap(MessageSignature::new, MessageSignature::bytes);
    public static final StreamCodec<ByteBuf, MessageSignature> STREAM_CODEC = StreamCodec.of((buf, signature) -> buf.writeBytes(signature.bytes()), buf -> {
        byte[] bytes = new byte[256];
        buf.readBytes(bytes);
        return new MessageSignature(bytes);
    });
    public static final int BYTES = 256;
    public MessageSignature {
        if (bytes.length != BYTES) {
            throw new IllegalStateException("Invalid message signature size");
        }
    }
    public boolean verify(SignatureValidator signature, SignatureUpdater updater) {
        return signature.validate(updater, bytes);
    }
    public ByteBuffer asByteBuffer() {
        return ByteBuffer.wrap(bytes);
    }
    public static String describe(MessageSignature signature) {
        return signature == null ? "<no signature>" : signature.toString();
    }
    public Packed pack(MessageSignatureCache cache) {
        int id = cache.pack(this);
        return id != -1 ? new Packed(id) : new Packed(this);
    }
    @Override
    public boolean equals(Object other) {
        return this == other || other instanceof MessageSignature that && java.util.Arrays.equals(bytes, that.bytes);
    }
    @Override
    public int hashCode() {
        return java.util.Arrays.hashCode(bytes);
    }
    @Override
    public String toString() {
        return java.util.Base64.getEncoder().encodeToString(bytes);
    }
    public int checksum() {
        return java.util.Arrays.hashCode(bytes);
    }
    public static record Packed(int id, MessageSignature fullSignature) {
        public static final int FULL_SIGNATURE = -1;
        public static final StreamCodec<ByteBuf, Packed> STREAM_CODEC = StreamCodec.of((buf, packed) -> {
            net.minecraft.network.codec.ByteBufCodecs.VAR_INT.encode(buf, packed.id() + 1);
            if (packed.fullSignature() != null) {
                MessageSignature.STREAM_CODEC.encode(buf, packed.fullSignature());
            }
        }, buf -> {
            int id = net.minecraft.network.codec.ByteBufCodecs.VAR_INT.decode(buf) - 1;
            return id == FULL_SIGNATURE ? new Packed(MessageSignature.STREAM_CODEC.decode(buf)) : new Packed(id);
        });
        public Packed(MessageSignature signature) {
            this(-1, signature);
        }
        public Packed(int id) {
            this(id, null);
        }
        public Optional<MessageSignature> unpack(MessageSignatureCache cache) {
            return fullSignature != null ? Optional.of(fullSignature) : Optional.ofNullable(cache.unpack(id));
        }
    }
}
