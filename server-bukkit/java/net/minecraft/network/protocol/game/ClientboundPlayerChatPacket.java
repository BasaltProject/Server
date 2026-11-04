// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.UUIDUtil;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.codec.ByteBufCodecs;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.ChatType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.FilterMask;
import net.minecraft.network.chat.MessageSignature;
import net.minecraft.network.chat.SignedMessageBody;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundPlayerChatPacket(int globalIndex, UUID sender, int index, Optional<MessageSignature> signature, SignedMessageBody.Packed body, Optional<Component> unsignedContent, FilterMask filterMask, ChatType.Bound chatType) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundPlayerChatPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundPlayerChatPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.VAR_INT, ClientboundPlayerChatPacket::globalIndex, UUIDUtil.STREAM_CODEC, ClientboundPlayerChatPacket::sender, ByteBufCodecs.VAR_INT, ClientboundPlayerChatPacket::index, MessageSignature.STREAM_CODEC.apply(ByteBufCodecs::optional), ClientboundPlayerChatPacket::signature, SignedMessageBody.Packed.STREAM_CODEC, ClientboundPlayerChatPacket::body, ComponentSerialization.TRUSTED_OPTIONAL_STREAM_CODEC, ClientboundPlayerChatPacket::unsignedContent, FilterMask.STREAM_CODEC, ClientboundPlayerChatPacket::filterMask, ChatType.Bound.STREAM_CODEC, ClientboundPlayerChatPacket::chatType, ClientboundPlayerChatPacket::new));
    public PacketType<ClientboundPlayerChatPacket> type() {
        return GamePacketTypes.CLIENTBOUND_PLAYER_CHAT;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handlePlayerChat(this);
    }
    public boolean isSkippable() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundPlayerChatPacket#isSkippable");
    }
}
