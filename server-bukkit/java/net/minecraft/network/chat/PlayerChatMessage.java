// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.datafixers.util.Function5;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.security.SignatureException;
import java.time.Duration;
import java.time.Instant;
import java.util.Objects;
import java.util.Optional;
import java.util.UUID;
import java.util.stream.Collectors;
import net.minecraft.util.SignatureUpdater;
import net.minecraft.util.SignatureValidator;
import net.minecraft.util.Util;
public record PlayerChatMessage(SignedMessageLink link, MessageSignature signature, SignedMessageBody signedBody, Component unsignedContent, FilterMask filterMask) {
    public static final MapCodec<PlayerChatMessage> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        SignedMessageLink.CODEC.fieldOf("link").forGetter(PlayerChatMessage::link),
        MessageSignature.CODEC.optionalFieldOf("signature").forGetter(m -> Optional.ofNullable(m.signature)),
        SignedMessageBody.MAP_CODEC.forGetter(PlayerChatMessage::signedBody),
        ComponentSerialization.CODEC.optionalFieldOf("unsigned_content").forGetter(m -> Optional.ofNullable(m.unsignedContent)),
        FilterMask.CODEC.optionalFieldOf("filter_mask", FilterMask.PASS_THROUGH).forGetter(PlayerChatMessage::filterMask))
        .apply(i, (Function5<SignedMessageLink, Optional<MessageSignature>, SignedMessageBody, Optional<Component>, FilterMask, PlayerChatMessage>) (link, signature, body, unsigned, mask) ->
            new PlayerChatMessage(link, signature.orElse(null), body, unsigned.orElse(null), mask)));
    private static final UUID SYSTEM_SENDER = Util.NIL_UUID;
    public static final Duration MESSAGE_EXPIRES_AFTER_SERVER = Duration.ofMinutes(5L);
    public static final Duration MESSAGE_EXPIRES_AFTER_CLIENT = MESSAGE_EXPIRES_AFTER_SERVER.plus(Duration.ofMinutes(2L));
    public static PlayerChatMessage system(String content) {
        return unsigned(SYSTEM_SENDER, content);
    }
    public static PlayerChatMessage unsigned(UUID sender, String content) {
        return new PlayerChatMessage(SignedMessageLink.unsigned(sender), null, SignedMessageBody.unsigned(content), null, FilterMask.PASS_THROUGH);
    }
    public PlayerChatMessage withUnsignedContent(Component content) {
        Component unsigned = !content.equals(Component.literal(signedContent())) ? content : null;
        return new PlayerChatMessage(link, signature, signedBody, unsigned, filterMask);
    }
    public PlayerChatMessage removeUnsignedContent() {
        return unsignedContent != null ? new PlayerChatMessage(link, signature, signedBody, null, filterMask) : this;
    }
    public PlayerChatMessage filter(FilterMask filterMask) {
        return this.filterMask.equals(filterMask) ? this : new PlayerChatMessage(link, signature, signedBody, unsignedContent, filterMask);
    }
    public PlayerChatMessage filter(boolean filtered) {
        return filter(filtered ? filterMask : FilterMask.PASS_THROUGH);
    }
    public PlayerChatMessage removeSignature() {
        return new PlayerChatMessage(SignedMessageLink.unsigned(sender()), null, SignedMessageBody.unsigned(signedContent()), unsignedContent, filterMask);
    }
    public static void updateSignature(SignatureUpdater.Output output, SignedMessageLink link, SignedMessageBody body) throws SignatureException {
        output.update(java.nio.ByteBuffer.allocate(4).putInt(1).array());
        link.updateSignature(output);
        body.updateSignature(output);
    }
    public boolean verify(SignatureValidator signatureValidator) {
        return signature != null && signature.verify(signatureValidator, output -> updateSignature(output, link, signedBody));
    }
    public String signedContent() {
        return signedBody.content();
    }
    public Component decoratedContent() {
        return Objects.requireNonNullElseGet(unsignedContent, () -> Component.literal(signedContent()));
    }
    public Instant timeStamp() {
        return signedBody.timeStamp();
    }
    public long salt() {
        return signedBody.salt();
    }
    public boolean hasExpiredServer(Instant now) {
        return now.isAfter(timeStamp().plus(MESSAGE_EXPIRES_AFTER_SERVER));
    }
    public boolean hasExpiredClient(Instant now) {
        return now.isAfter(timeStamp().plus(MESSAGE_EXPIRES_AFTER_CLIENT));
    }
    public UUID sender() {
        return link.sender();
    }
    public boolean isSystem() {
        return sender().equals(SYSTEM_SENDER);
    }
    public boolean hasSignature() {
        return signature != null;
    }
    public boolean hasSignatureFrom(UUID profileId) {
        return hasSignature() && link.sender().equals(profileId);
    }
    public boolean isFullyFiltered() {
        return filterMask.isFullyFiltered();
    }
    public static String describeSigned(PlayerChatMessage message) {
        return "'" + message.signedBody.content() + "' @ " + message.signedBody.timeStamp() + "\n - From: " + message.link.sender() + "/"
            + message.link.sessionId() + ", message #" + message.link.index() + "\n - Salt: " + message.signedBody.salt()
            + "\n - Signature: " + MessageSignature.describe(message.signature) + "\n - Last Seen: [\n"
            + message.signedBody.lastSeen().entries().stream().map(s -> "     " + MessageSignature.describe(s) + "\n").collect(Collectors.joining()) + " ]\n";
    }
}
