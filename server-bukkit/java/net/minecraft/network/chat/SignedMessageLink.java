// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.security.SignatureException;
import java.util.UUID;
import net.minecraft.core.UUIDUtil;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.SignatureUpdater;
import net.minecraft.util.Util;
public record SignedMessageLink(int index, UUID sender, UUID sessionId) {
    public static final Codec<SignedMessageLink> CODEC = RecordCodecBuilder.create(i -> i.group(
        ExtraCodecs.NON_NEGATIVE_INT.fieldOf("index").forGetter(SignedMessageLink::index),
        UUIDUtil.CODEC.fieldOf("sender").forGetter(SignedMessageLink::sender),
        UUIDUtil.CODEC.fieldOf("session_id").forGetter(SignedMessageLink::sessionId)).apply(i, SignedMessageLink::new));
    public static SignedMessageLink unsigned(UUID sender) {
        return root(sender, Util.NIL_UUID);
    }
    public static SignedMessageLink root(UUID sender, UUID sessionId) {
        return new SignedMessageLink(0, sender, sessionId);
    }
    public void updateSignature(SignatureUpdater.Output output) throws SignatureException {
        output.update(UUIDUtil.uuidToByteArray(sender));
        output.update(UUIDUtil.uuidToByteArray(sessionId));
        output.update(java.nio.ByteBuffer.allocate(4).putInt(index).array());
    }
    public boolean isDescendantOf(SignedMessageLink link) {
        return index > link.index() && sender.equals(link.sender()) && sessionId.equals(link.sessionId());
    }
    public SignedMessageLink advance() {
        return index == Integer.MAX_VALUE ? null : new SignedMessageLink(index + 1, sender, sessionId);
    }
}
