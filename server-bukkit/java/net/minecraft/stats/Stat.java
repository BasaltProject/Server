// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.stats;

import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.scores.criteria.ObjectiveCriteria;

public class Stat<T> extends ObjectiveCriteria {
    public static final StreamCodec<RegistryFriendlyByteBuf, Stat<?>> STREAM_CODEC = StreamCodec.of(
        (buf, stat) -> {
            buf.writeVarInt(net.minecraft.core.registries.BuiltInRegistries.STAT_TYPE.getId(stat.getType()));
            writeValue(buf, stat);
        },
        buf -> {
            Stats.CUSTOM.getDisplayName();
            return readValue(buf, net.minecraft.core.registries.BuiltInRegistries.STAT_TYPE.byId(buf.readVarInt()));
        });
    private final StatFormatter formatter;
    private final T value;
    private final StatType<T> type;

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static void writeValue(RegistryFriendlyByteBuf buf, Stat<?> stat) {
        ((StreamCodec) stat.getType().streamCodec()).encode(buf, stat);
    }

    private static <V> Stat<?> readValue(RegistryFriendlyByteBuf buf, StatType<V> type) {
        return type.streamCodec().decode(buf);
    }

    protected Stat() {
        super("stat");
        this.formatter = StatFormatter.DEFAULT;
        this.value = null;
        this.type = null;
    }

    protected Stat(StatType<T> type, T value, StatFormatter formatter) {
        super(buildName(type, value));
        this.type = type;
        this.formatter = formatter;
        this.value = value;
    }

    public static <T> String buildName(StatType<T> type, T value) {
        net.minecraft.resources.Identifier statKey = type.getRegistry().getKey(value);
        return locationToKey(statKey, type);
    }

    private static <T> String locationToKey(net.minecraft.resources.Identifier location, StatType<T> type) {
        return location == null ? "unknown" : location.toString().replace(':', '.');
    }

    public StatType<T> getType() {
        return type;
    }

    public T getValue() {
        return value;
    }

    public String format(int value) {
        return formatter.format(value);
    }

    @Override
    public boolean equals(Object o) {
        return this == o || o instanceof Stat<?> other && java.util.Objects.equals(value, other.value);
    }

    @Override
    public int hashCode() {
        return java.util.Objects.hashCode(value);
    }

    @Override
    public String toString() {
        return "Stat{name=" + getName() + ", formatter=" + formatter + "}";
    }
}
