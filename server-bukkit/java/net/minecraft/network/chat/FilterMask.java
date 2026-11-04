// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import java.util.BitSet;
import java.util.function.IntFunction;
import java.util.function.Supplier;
import net.minecraft.ChatFormatting;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.ByIdMap;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.StringRepresentable;
public class FilterMask {
    public static final Codec<FilterMask> CODEC = StringRepresentable.fromEnum(Type::values).dispatch(FilterMask::type, Type::codec);
    public static final StreamCodec<ByteBuf, FilterMask> STREAM_CODEC = Type.STREAM_CODEC.dispatch(FilterMask::type, Type::streamCodec);
    public static final FilterMask FULLY_FILTERED = new FilterMask(new BitSet(0), Type.FULLY_FILTERED);
    public static final FilterMask PASS_THROUGH = new FilterMask(new BitSet(0), Type.PASS_THROUGH);
    public static final Style FILTERED_STYLE = Style.EMPTY.withColor(ChatFormatting.DARK_GRAY).withHoverEvent(new HoverEvent.ShowText(Component.translatable("chat.filtered")));
    private final BitSet mask;
    private final Type type;
    private FilterMask(BitSet mask, Type type) {
        this.mask = mask;
        this.type = type;
    }
    private FilterMask(BitSet mask) {
        this(mask, Type.PARTIALLY_FILTERED);
    }
    public FilterMask(int length) {
        this(new BitSet(length), Type.PARTIALLY_FILTERED);
    }
    private Type type() {
        return type;
    }
    private BitSet mask() {
        return mask;
    }
    public void setFiltered(int index) {
        mask.set(index);
    }
    public String apply(String text) {
        return switch (type) {
            case PASS_THROUGH -> text;
            case FULLY_FILTERED -> null;
            case PARTIALLY_FILTERED -> {
                char[] chars = text.toCharArray();
                for (int i = 0; i < chars.length && i < mask.length(); i++) {
                    if (mask.get(i)) {
                        chars[i] = '#';
                    }
                }
                yield new String(chars);
            }
        };
    }
    public Component applyWithFormatting(String text) {
        switch (type) {
            case PASS_THROUGH:
                return Component.literal(text);
            case FULLY_FILTERED:
                return null;
            default:
                MutableComponent result = Component.empty();
                int from = 0;
                boolean filtered = mask.get(0);
                while (true) {
                    int next = filtered ? mask.nextClearBit(from) : mask.nextSetBit(from);
                    next = next < 0 ? text.length() : next;
                    if (next == from) {
                        return result;
                    }
                    if (filtered) {
                        result.append((Component) Component.literal("#".repeat(next - from)).withStyle(FILTERED_STYLE));
                    } else {
                        result.append(text.substring(from, next));
                    }
                    filtered = !filtered;
                    from = next;
                }
        }
    }
    public boolean isEmpty() {
        return type == Type.PASS_THROUGH;
    }
    public boolean isFullyFiltered() {
        return type == Type.FULLY_FILTERED;
    }
    public boolean equals(Object o) {
        return this == o || o != null && getClass() == o.getClass() && mask.equals(((FilterMask) o).mask) && type == ((FilterMask) o).type;
    }
    public int hashCode() {
        return 31 * mask.hashCode() + type.hashCode();
    }
    static enum Type implements StringRepresentable {
        PASS_THROUGH(0, "pass_through", () -> MapCodec.unit(FilterMask.PASS_THROUGH), () -> StreamCodec.unit(FilterMask.PASS_THROUGH)),
        FULLY_FILTERED(1, "fully_filtered", () -> MapCodec.unit(FilterMask.FULLY_FILTERED), () -> StreamCodec.unit(FilterMask.FULLY_FILTERED)),
        PARTIALLY_FILTERED(2, "partially_filtered", () -> ExtraCodecs.BIT_SET.xmap(FilterMask::new, FilterMask::mask).fieldOf("value"), () -> ByteBufCodecs.BIT_SET.map(FilterMask::new, FilterMask::mask));
        private static final IntFunction<Type> ID_MAP = ByIdMap.continuous(t -> t.id, values(), ByIdMap.OutOfBoundsStrategy.ZERO);
        public static final StreamCodec<ByteBuf, Type> STREAM_CODEC = ByteBufCodecs.idMapper(ID_MAP, t -> t.id);
        private final int id;
        private final String serializedName;
        private final Supplier<MapCodec<FilterMask>> codec;
        private final Supplier<StreamCodec<ByteBuf, FilterMask>> streamCodec;
        private Type(int id, String serializedName, Supplier<MapCodec<FilterMask>> codec, Supplier<StreamCodec<ByteBuf, FilterMask>> streamCodec) {
            this.id = id;
            this.serializedName = serializedName;
            this.codec = com.google.common.base.Suppliers.memoize(codec::get);
            this.streamCodec = com.google.common.base.Suppliers.memoize(streamCodec::get);
        }
        public String getSerializedName() {
            return serializedName;
        }
        private MapCodec<FilterMask> codec() {
            return codec.get();
        }
        private StreamCodec<ByteBuf, FilterMask> streamCodec() {
            return streamCodec.get();
        }
    }
}
