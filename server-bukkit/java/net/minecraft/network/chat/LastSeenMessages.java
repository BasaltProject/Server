// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.security.SignatureException;
import java.util.BitSet;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.SignatureUpdater;
public record LastSeenMessages(List<MessageSignature> entries) {
    public static final Codec<LastSeenMessages> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.network.chat.LastSeenMessages.class, "CODEC");
    public static final LastSeenMessages EMPTY = new LastSeenMessages(java.util.List.of());
    public static final int LAST_SEEN_MESSAGES_MAX_LENGTH = 20;
    public void updateSignature(SignatureUpdater.Output output) throws SignatureException {
        output.update(java.nio.ByteBuffer.allocate(4).putInt(entries.size()).array());
        for (MessageSignature entry : entries) {
            output.update(entry.bytes());
        }
    }
    public Packed pack(MessageSignatureCache cache) {
        return new Packed(entries.stream().map(entry -> entry.pack(cache)).toList());
    }
    public byte computeChecksum() {
        int checksum = 1;
        for (MessageSignature entry : entries) {
            checksum = 31 * checksum + entry.checksum();
        }
        byte folded = (byte) checksum;
        return folded == 0 ? 1 : folded;
    }
    public static record Packed(List<MessageSignature.Packed> entries) {
        public static final Packed EMPTY = new Packed(java.util.List.of());
        public static final StreamCodec<ByteBuf, Packed> STREAM_CODEC = MessageSignature.Packed.STREAM_CODEC.apply(net.minecraft.network.codec.ByteBufCodecs.list(20)).map(Packed::new, Packed::entries);
        public Optional<LastSeenMessages> unpack(MessageSignatureCache cache) {
            List<MessageSignature> unpacked = new java.util.ArrayList<>(entries.size());
            for (MessageSignature.Packed packed : entries) {
                Optional<MessageSignature> entry = packed.unpack(cache);
                if (entry.isEmpty()) {
                    return Optional.empty();
                }
                unpacked.add(entry.get());
            }
            return Optional.of(new LastSeenMessages(unpacked));
        }
    }
    public static record Update(int offset, BitSet acknowledged, byte checksum) {
        public static final byte IGNORE_CHECKSUM = (byte) 0;
        public static final StreamCodec<ByteBuf, Update> STREAM_CODEC = StreamCodec.of((buf, update) -> {
            net.minecraft.network.codec.ByteBufCodecs.VAR_INT.encode(buf, update.offset());
            byte[] bits = java.util.Arrays.copyOf(update.acknowledged().toByteArray(), 3);
            buf.writeBytes(bits);
            buf.writeByte(update.checksum());
        }, buf -> {
            int offset = net.minecraft.network.codec.ByteBufCodecs.VAR_INT.decode(buf);
            byte[] bits = new byte[3];
            buf.readBytes(bits);
            return new Update(offset, java.util.BitSet.valueOf(bits), buf.readByte());
        });
        public boolean verifyChecksum(LastSeenMessages lastSeen) {
            return checksum == IGNORE_CHECKSUM || checksum == lastSeen.computeChecksum();
        }
    }
}
