// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.codec;
import io.netty.buffer.ByteBuf;
import io.netty.handler.codec.DecoderException;
import io.netty.handler.codec.EncoderException;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import net.minecraft.network.VarInt;
public class IdDispatchCodec<B extends ByteBuf, V, T> implements StreamCodec<B, V> {
    private static final int kUnknownType = -1;
    private final Function<V, ? extends T> typeGetter;
    private final List<Entry<B, V, T>> byId;
    private final Object2IntMap<T> toId;
    private IdDispatchCodec(Function<V, ? extends T> typeGetter, List<Entry<B, V, T>> byId, Object2IntMap<T> toId) {
        this.typeGetter = typeGetter;
        this.byId = byId;
        this.toId = toId;
    }
    public V decode(B input) {
        int id = VarInt.read(input);
        if (id < 0 || id >= byId.size()) {
            throw new DecoderException("Received unknown packet id " + id);
        }
        Entry<B, V, T> entry = byId.get(id);
        try {
            return entry.serializer.decode(input);
        } catch (Exception e) {
            if (e instanceof DontDecorateException) {
                throw e;
            }
            throw new DecoderException("Failed to decode packet '" + entry.type + "'", e);
        }
    }
    @SuppressWarnings("unchecked")
    public void encode(B output, V value) {
        T type = typeGetter.apply(value);
        int id = toId.getOrDefault(type, kUnknownType);
        if (id == kUnknownType) {
            throw new EncoderException("Sending unknown packet '" + type + "'");
        }
        VarInt.write(output, id);
        try {
            ((StreamCodec<? super B, V>) byId.get(id).serializer).encode(output, value);
        } catch (Exception e) {
            if (e instanceof DontDecorateException) {
                throw e;
            }
            throw new EncoderException("Failed to encode packet '" + type + "'", e);
        }
    }
    public static <B extends ByteBuf, V, T> Builder<B, V, T> builder(Function<V, ? extends T> typeGetter) {
        return new Builder<>(typeGetter);
    }
    public static class Builder<B extends ByteBuf, V, T> {
        private final List<Entry<B, V, T>> entries = new ArrayList<>();
        private final Function<V, ? extends T> typeGetter;
        private Builder(Function<V, ? extends T> typeGetter) {
            this.typeGetter = typeGetter;
        }
        public Builder<B, V, T> add(T type, StreamCodec<? super B, ? extends V> serializer) {
            entries.add(new Entry<>(serializer, type));
            return this;
        }
        public IdDispatchCodec<B, V, T> build() {
            Object2IntOpenHashMap<T> toId = new Object2IntOpenHashMap<>();
            toId.defaultReturnValue(-2);
            for (Entry<B, V, T> entry : entries) {
                if (toId.putIfAbsent(entry.type, toId.size()) != -2) {
                    throw new IllegalStateException("Duplicate registration for type " + entry.type);
                }
            }
            return new IdDispatchCodec<>(typeGetter, List.copyOf(entries), toId);
        }
    }
    static record Entry<B, V, T>(StreamCodec<? super B, ? extends V> serializer, T type) {
    }
    public interface DontDecorateException {
    }
}
