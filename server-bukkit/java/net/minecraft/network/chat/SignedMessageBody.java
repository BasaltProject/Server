// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import java.security.SignatureException;
import java.time.Instant;
import java.util.Optional;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.SignatureUpdater;
public record SignedMessageBody(String content, Instant timeStamp, long salt, LastSeenMessages lastSeen) {
    public static final MapCodec<SignedMessageBody> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.network.chat.SignedMessageBody.class, "MAP_CODEC");
    public static SignedMessageBody unsigned(String content) {
        return new SignedMessageBody(content, Instant.now(), 0L, LastSeenMessages.EMPTY);
    }
    public void updateSignature(SignatureUpdater.Output output) throws SignatureException {
        output.update(java.nio.ByteBuffer.allocate(8).putLong(salt).array());
        output.update(java.nio.ByteBuffer.allocate(8).putLong(timeStamp.getEpochSecond()).array());
        byte[] contentBytes = content.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        output.update(java.nio.ByteBuffer.allocate(4).putInt(contentBytes.length).array());
        output.update(contentBytes);
        lastSeen.updateSignature(output);
    }
    public Packed pack(MessageSignatureCache cache) {
        return new Packed(content, timeStamp, salt, lastSeen.pack(cache));
    }
    public static record Packed(String content, Instant timeStamp, long salt, LastSeenMessages.Packed lastSeen) {
        public static final StreamCodec<ByteBuf, Packed> STREAM_CODEC = StreamCodec.composite(net.minecraft.network.codec.ByteBufCodecs.stringUtf8(256), Packed::content, net.minecraft.network.codec.ByteBufCodecs.INSTANT, Packed::timeStamp, net.minecraft.network.codec.ByteBufCodecs.LONG, Packed::salt, LastSeenMessages.Packed.STREAM_CODEC, Packed::lastSeen, Packed::new);
        public Optional<SignedMessageBody> unpack(MessageSignatureCache cache) {
            return lastSeen.unpack(cache).map(seen -> new SignedMessageBody(content, timeStamp, salt, seen));
        }
    }
}
