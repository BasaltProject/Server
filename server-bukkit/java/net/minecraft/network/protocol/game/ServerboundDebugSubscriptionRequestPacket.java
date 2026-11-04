// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import java.util.Set;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.util.debug.DebugSubscription;
public record ServerboundDebugSubscriptionRequestPacket(Set<DebugSubscription<?>> subscriptions) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ServerboundDebugSubscriptionRequestPacket> STREAM_CODEC = ByteBufCodecs.registry(net.minecraft.core.registries.Registries.DEBUG_SUBSCRIPTION)
            .apply(ByteBufCodecs.collection(java.util.HashSet::new)).map(ServerboundDebugSubscriptionRequestPacket::new,
                packet -> new java.util.HashSet<>(packet.subscriptions()));
    public PacketType<ServerboundDebugSubscriptionRequestPacket> type() {
        return GamePacketTypes.SERVERBOUND_DEBUG_SUBSCRIPTION_REQUEST;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleDebugSubscriptionRequest(this);
    }
}
