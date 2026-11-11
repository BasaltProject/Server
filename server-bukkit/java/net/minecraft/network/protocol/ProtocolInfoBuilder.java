// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol;
import io.netty.buffer.ByteBuf;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;
import net.minecraft.network.ClientboundPacketListener;
import net.minecraft.network.ConnectionProtocol;
import net.minecraft.network.PacketListener;
import net.minecraft.network.ProtocolInfo;
import net.minecraft.network.ServerboundPacketListener;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.Unit;
public class ProtocolInfoBuilder<T extends PacketListener, B extends ByteBuf, C> {
    private final ConnectionProtocol protocol;
    private final PacketFlow flow;
    private final List<CodecEntry<T, ?, B, C>> entries = new ArrayList<>();
    private BundlerInfo bundlerInfo;
    public ProtocolInfoBuilder(ConnectionProtocol protocol, PacketFlow flow) {
        this.protocol = protocol;
        this.flow = flow;
    }
    public <P extends Packet<? super T>> ProtocolInfoBuilder<T, B, C> addPacket(PacketType<P> type, StreamCodec<? super B, P> serializer) {
        return addPacket(type, serializer, null);
    }
    public <P extends Packet<? super T>> ProtocolInfoBuilder<T, B, C> addPacket(PacketType<P> type, StreamCodec<? super B, P> serializer, CodecModifier<B, P, C> modifier) {
        entries.add(new CodecEntry<>(type, serializer, modifier));
        return this;
    }
    public <P extends BundlePacket<? super T>, D extends BundleDelimiterPacket<? super T>> ProtocolInfoBuilder<T, B, C> withBundlePacket(PacketType<P> bundlerPacket, Function<Iterable<Packet<? super T>>, P> constructor, D delimiterPacket) {
        @SuppressWarnings({"unchecked", "rawtypes"})
        CodecEntry<T, ?, B, C> delimiter = new CodecEntry(delimiterPacket.type(), StreamCodec.unit(delimiterPacket), null);
        entries.add(delimiter);
        bundlerInfo = BundlerInfo.createForPacket(bundlerPacket, constructor, delimiterPacket);
        return this;
    }
    private StreamCodec<ByteBuf, Packet<? super T>> packetCodec(Function<ByteBuf, B> wrapper, List<CodecEntry<T, ?, B, C>> list, C context) {
        ProtocolCodecBuilder<ByteBuf, T> codec = new ProtocolCodecBuilder<>(flow);
        for (CodecEntry<T, ?, B, C> entry : list) {
            entry.addToBuilder(codec, wrapper, context);
        }
        return codec.build();
    }
    private ProtocolInfo.Details details(List<? extends CodecEntry<?, ?, ?, ?>> list) {
        ConnectionProtocol id = protocol;
        PacketFlow direction = flow;
        return new ProtocolInfo.Details() {
            public ConnectionProtocol id() {
                return id;
            }
            public PacketFlow flow() {
                return direction;
            }
            public void listPackets(ProtocolInfo.Details.PacketVisitor output) {
                for (int index = 0; index < list.size(); index++) {
                    output.accept(list.get(index).type(), index);
                }
            }
        };
    }
    public SimpleUnboundProtocol<T, B> buildUnbound(C context) {
        List<CodecEntry<T, ?, B, C>> frozen = List.copyOf(entries);
        BundlerInfo bundler = bundlerInfo;
        ProtocolInfo.Details described = details(frozen);
        return new SimpleUnboundProtocol<T, B>() {
            public ProtocolInfo<T> bind(Function<ByteBuf, B> wrapper) {
                return new Implementation<T>(protocol, flow, packetCodec(wrapper, frozen, context), bundler);
            }
            public ProtocolInfo.Details details() {
                return described;
            }
        };
    }
    public UnboundProtocol<T, B, C> buildUnbound() {
        List<CodecEntry<T, ?, B, C>> frozen = List.copyOf(entries);
        BundlerInfo bundler = bundlerInfo;
        ProtocolInfo.Details described = details(frozen);
        return new UnboundProtocol<T, B, C>() {
            public ProtocolInfo<T> bind(Function<ByteBuf, B> wrapper, C context) {
                return new Implementation<T>(protocol, flow, packetCodec(wrapper, frozen, context), bundler);
            }
            public ProtocolInfo.Details details() {
                return described;
            }
        };
    }
    public static <T extends ServerboundPacketListener, B extends ByteBuf> SimpleUnboundProtocol<T, B> serverboundProtocol(ConnectionProtocol id, Consumer<ProtocolInfoBuilder<T, B, Unit>> config) {
        ProtocolInfoBuilder<T, B, Unit> builder = new ProtocolInfoBuilder<>(id, PacketFlow.SERVERBOUND);
        config.accept(builder);
        return builder.buildUnbound(Unit.INSTANCE);
    }
    public static <T extends ClientboundPacketListener, B extends ByteBuf> SimpleUnboundProtocol<T, B> clientboundProtocol(ConnectionProtocol id, Consumer<ProtocolInfoBuilder<T, B, Unit>> config) {
        ProtocolInfoBuilder<T, B, Unit> builder = new ProtocolInfoBuilder<>(id, PacketFlow.CLIENTBOUND);
        config.accept(builder);
        return builder.buildUnbound(Unit.INSTANCE);
    }
    public static <T extends ServerboundPacketListener, B extends ByteBuf, C> UnboundProtocol<T, B, C> contextServerboundProtocol(ConnectionProtocol id, Consumer<ProtocolInfoBuilder<T, B, C>> config) {
        ProtocolInfoBuilder<T, B, C> builder = new ProtocolInfoBuilder<>(id, PacketFlow.SERVERBOUND);
        config.accept(builder);
        return builder.buildUnbound();
    }
    public static <T extends ClientboundPacketListener, B extends ByteBuf, C> UnboundProtocol<T, B, C> contextClientboundProtocol(ConnectionProtocol id, Consumer<ProtocolInfoBuilder<T, B, C>> config) {
        ProtocolInfoBuilder<T, B, C> builder = new ProtocolInfoBuilder<>(id, PacketFlow.CLIENTBOUND);
        config.accept(builder);
        return builder.buildUnbound();
    }
    static record CodecEntry<T extends PacketListener, P extends Packet<? super T>, B extends ByteBuf, C>(PacketType<P> type, StreamCodec<? super B, P> serializer, CodecModifier<B, P, C> modifier) {
        public void addToBuilder(ProtocolCodecBuilder<ByteBuf, T> codecBuilder, Function<ByteBuf, B> contextWrapper, C context) {
            StreamCodec<? super B, P> effective = modifier == null ? serializer : modifier.apply(serializer, context);
            StreamCodec<ByteBuf, P> wrapped = new StreamCodec<>() {
                public P decode(ByteBuf input) {
                    return effective.decode(contextWrapper.apply(input));
                }
                public void encode(ByteBuf output, P value) {
                    effective.encode(contextWrapper.apply(output), value);
                }
            };
            codecBuilder.add(type, wrapped);
        }
    }
    static record Implementation<L extends PacketListener>(ConnectionProtocol id, PacketFlow flow, StreamCodec<ByteBuf, Packet<? super L>> codec, BundlerInfo bundlerInfo) implements ProtocolInfo<L> {
    }
}
