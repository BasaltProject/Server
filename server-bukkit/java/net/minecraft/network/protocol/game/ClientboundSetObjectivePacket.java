// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.CommonComponents;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.numbers.BlankFormat;
import net.minecraft.network.chat.numbers.FixedFormat;
import net.minecraft.network.chat.numbers.NumberFormat;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.scores.Objective;
import net.minecraft.world.scores.criteria.ObjectiveCriteria;
import org.basaltmc.bukkit.net.PacketCodec;
import org.basaltmc.bukkit.net.WirePacket;

public class ClientboundSetObjectivePacket implements WirePacket, Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSetObjectivePacket> STREAM_CODEC =
        Packet.codec(ClientboundSetObjectivePacket::writeBody, ClientboundSetObjectivePacket::new);
    public static final int METHOD_ADD = 0;
    public static final int METHOD_REMOVE = 1;
    public static final int METHOD_CHANGE = 2;
    private final String objectiveName;
    private final Component displayName;
    private final ObjectiveCriteria.RenderType renderType;
    private final NumberFormat numberFormat;
    private final int method;

    public ClientboundSetObjectivePacket(Objective objective, int method) {
        if (method < METHOD_ADD || method > METHOD_CHANGE) {
            throw new IllegalArgumentException("Invalid objective mode");
        }
        this.objectiveName = objective.getName();
        this.method = method;
        this.displayName = objective.getDisplayName();
        this.renderType = objective.getRenderType();
        this.numberFormat = objective.numberFormat();
    }

    private ClientboundSetObjectivePacket(RegistryFriendlyByteBuf input) {
        this.objectiveName = input.readUtf();
        this.method = input.readByte();
        if (method == METHOD_ADD || method == METHOD_CHANGE) {
            this.displayName = ComponentSerialization.TRUSTED_STREAM_CODEC.decode(input);
            this.renderType = input.readEnum(ObjectiveCriteria.RenderType.class);
            this.numberFormat = input.readBoolean() ? readFormat(input) : null;
        } else {
            this.displayName = CommonComponents.EMPTY;
            this.renderType = ObjectiveCriteria.RenderType.INTEGER;
            this.numberFormat = null;
        }
    }

    private static NumberFormat readFormat(RegistryFriendlyByteBuf input) {
        return switch (input.readVarInt()) {
            case 0 -> BlankFormat.INSTANCE;
            case 2 -> new FixedFormat(ComponentSerialization.TRUSTED_STREAM_CODEC.decode(input));
            default -> throw new UnsupportedOperationException("Styled number formats are not read by the Basalt facade");
        };
    }

    private void writeBody(RegistryFriendlyByteBuf output) {
        write(output);
    }

    @Override
    public int packetId() {
        return 0x6c;
    }

    @Override
    public void write(ByteBuf out) {
        PacketCodec.string(out, objectiveName);
        out.writeByte(method);
        if (method != METHOD_REMOVE) {
            PacketCodec.component(out, displayName);
            PacketCodec.varInt(out, renderType.ordinal());
            PacketCodec.optionalFormat(out, numberFormat);
        }
    }

    public PacketType<ClientboundSetObjectivePacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_OBJECTIVE;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleAddObjective(this);
    }

    public String getObjectiveName() {
        return objectiveName;
    }

    public Component getDisplayName() {
        return displayName;
    }

    public int getMethod() {
        return method;
    }

    public ObjectiveCriteria.RenderType getRenderType() {
        return renderType;
    }

    public Optional<NumberFormat> getNumberFormat() {
        return Optional.ofNullable(numberFormat);
    }
}
