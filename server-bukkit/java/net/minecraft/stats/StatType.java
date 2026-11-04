// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.stats;

import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.Registry;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public class StatType<T> implements Iterable<Stat<T>> {
    private final Registry<T> registry;
    private final Map<T, Stat<T>> map = new ConcurrentHashMap<>();
    private final Component displayName;
    private final StreamCodec<RegistryFriendlyByteBuf, Stat<T>> streamCodec;

    public StatType(Registry<T> registry, Component displayName) {
        this.registry = registry;
        this.displayName = displayName;
        this.streamCodec = StreamCodec.of((buf, stat) -> ByteBufCodecs.VAR_INT.encode(buf, registry.getId(stat.getValue())),
            buf -> get(registry.byId(ByteBufCodecs.VAR_INT.decode(buf))));
    }

    public StreamCodec<RegistryFriendlyByteBuf, Stat<T>> streamCodec() {
        return streamCodec;
    }

    public boolean contains(T key) {
        return map.containsKey(key);
    }

    public Stat<T> get(T argument, StatFormatter formatter) {
        return map.computeIfAbsent(argument, key -> new Stat<>(this, key, formatter));
    }

    public Registry<T> getRegistry() {
        return registry;
    }

    @Override
    public Iterator<Stat<T>> iterator() {
        return map.values().iterator();
    }

    public Stat<T> get(T argument) {
        return get(argument, StatFormatter.DEFAULT);
    }

    public Component getDisplayName() {
        return displayName;
    }
}
