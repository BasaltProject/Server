// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import com.google.common.collect.Lists;
import java.util.ArrayList;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.resources.Identifier;
import io.netty.buffer.ByteBuf;
import java.util.Collection;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeInstance;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
public class ClientboundUpdateAttributesPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundUpdateAttributesPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundUpdateAttributesPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.VAR_INT, ClientboundUpdateAttributesPacket::getEntityId, ClientboundUpdateAttributesPacket.AttributeSnapshot.STREAM_CODEC.apply(ByteBufCodecs.list(128)), ClientboundUpdateAttributesPacket::getValues, ClientboundUpdateAttributesPacket::new));
    public ClientboundUpdateAttributesPacket(int entityId, Collection<AttributeInstance> values) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateAttributesPacket#<init>");
    }
    private ClientboundUpdateAttributesPacket(int entityId, List<AttributeSnapshot> attributes) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateAttributesPacket#<init>");
    }
    public PacketType<ClientboundUpdateAttributesPacket> type() {
        return GamePacketTypes.CLIENTBOUND_UPDATE_ATTRIBUTES;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleUpdateAttributes(this);
    }
    public int getEntityId() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateAttributesPacket#getEntityId");
    }
    public List<AttributeSnapshot> getValues() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundUpdateAttributesPacket#getValues");
    }
    public static record AttributeSnapshot(Holder<Attribute> attribute, double base, Collection<AttributeModifier> modifiers) {
        public static final StreamCodec<ByteBuf, AttributeModifier> MODIFIER_STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundUpdateAttributesPacket.AttributeSnapshot.MODIFIER_STREAM_CODEC", () -> StreamCodec.composite(Identifier.STREAM_CODEC, AttributeModifier::id, ByteBufCodecs.DOUBLE, AttributeModifier::amount, AttributeModifier.Operation.STREAM_CODEC, AttributeModifier::operation, AttributeModifier::new));
        public static final StreamCodec<RegistryFriendlyByteBuf, AttributeSnapshot> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundUpdateAttributesPacket.AttributeSnapshot.STREAM_CODEC", () -> StreamCodec.composite(Attribute.STREAM_CODEC, AttributeSnapshot::attribute, ByteBufCodecs.DOUBLE, AttributeSnapshot::base, MODIFIER_STREAM_CODEC.apply(ByteBufCodecs.collection(ArrayList::new)), AttributeSnapshot::modifiers, AttributeSnapshot::new));
    }
}
