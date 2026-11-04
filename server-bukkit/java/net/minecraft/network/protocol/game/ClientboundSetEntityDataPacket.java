// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import io.netty.buffer.ByteBuf;
import java.util.*;
import net.minecraft.network.syncher.*;
import org.basaltmc.bukkit.net.*;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.network.syncher.SynchedEntityData;
public final class ClientboundSetEntityDataPacket implements WirePacket, Packet<ClientGamePacketListener> {
    private final int id;
    private final List<SynchedEntityData.DataValue<?>> packedItems;
    // Preserve unknown serializer payloads verbatim, rather than dropping future metadata.
    private static final EntityDataSerializer<byte[]> RAW = new EntityDataSerializer<>() {
        public int id() {
            return -1;
        }
        public void write(ByteBuf out, byte[] value) {
            out.writeBytes(value);
        }
        public byte[] read(ByteBuf in) {
            throw new UnsupportedOperationException();
        }
    };
    public ClientboundSetEntityDataPacket(int id, List<SynchedEntityData.DataValue<?>> items) {
        this.id = id;
        this.packedItems = List.copyOf(items);
    }
    public int id() {
        return id;
    }
    public List<SynchedEntityData.DataValue<?>> packedItems() {
        return packedItems;
    }
    public int packetId() {
        return 0x65;
    }
    public void write(ByteBuf out) {
        PacketCodec.varInt(out, id);
        for (var value : packedItems) {
            if (value.serializer() == RAW)
                out.writeBytes((byte[]) value.value());
            else
                value.write(out);
        }
        out.writeByte(255);
    }
    public static ClientboundSetEntityDataPacket read(ByteBuf in) {
        int id = PacketCodec.readVarInt(in);
        List<SynchedEntityData.DataValue<?>> values = new ArrayList<>();
        while (in.isReadable()) {
            int start = in.readerIndex();
            int index = in.readUnsignedByte();
            if (index == 255) {
                if (in.isReadable())
                    throw new IllegalArgumentException("Trailing metadata bytes");
                return new ClientboundSetEntityDataPacket(id, values);
            }
            int type = PacketCodec.readVarInt(in);
            EntityDataSerializer<?> serializer = EntityDataSerializers.getSerializer(type);
            if (serializer == null) {
                in.readerIndex(start);
                int count = in.readableBytes();
                if (in.getUnsignedByte(in.writerIndex() - 1) != 255)
                    throw new IllegalArgumentException("Unterminated metadata");
                byte[] raw = new byte[count - 1];
                in.readBytes(raw);
                in.readByte();
                values.add(new SynchedEntityData.DataValue<>(index, RAW, raw));
                return new ClientboundSetEntityDataPacket(id, values);
            }
            values.add(readValue(index, serializer, in));
        }
        throw new IllegalArgumentException("Unterminated metadata");
    }
    private static <T> SynchedEntityData.DataValue<T> readValue(
        int id, EntityDataSerializer<T> serializer, ByteBuf in) {
        return new SynchedEntityData.DataValue<>(id, serializer, serializer.read(in));
    }
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSetEntityDataPacket> STREAM_CODEC = StreamCodec.of((buf, packet) -> packet.write(buf), ClientboundSetEntityDataPacket::read);
    public static final int EOF_MARKER = 255;
    public PacketType<ClientboundSetEntityDataPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_ENTITY_DATA;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetEntityData(this);
    }
}
