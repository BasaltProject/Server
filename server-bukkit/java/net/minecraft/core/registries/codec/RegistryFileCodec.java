// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.registries.codec;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.Lifecycle;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderOwner;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.RegistryOps;
import net.minecraft.resources.ResourceKey;
public final class RegistryFileCodec<E> implements Codec<Holder<E>> {
    private final ResourceKey<? extends Registry<E>> registryKey;
    private final Codec<E> elementCodec;
    private final boolean allowInline;
    public static <E> Codec<Holder<E>> create(ResourceKey<? extends Registry<E>> registryKey, Codec<E> elementCodec, boolean allowInline) {
        return new RegistryFileCodec<>(registryKey, elementCodec, allowInline);
    }
    private RegistryFileCodec(ResourceKey<? extends Registry<E>> registryKey, Codec<E> elementCodec, boolean allowInline) {
        this.registryKey = registryKey;
        this.elementCodec = elementCodec;
        this.allowInline = allowInline;
    }
    public <T> DataResult<T> encode(Holder<E> input, DynamicOps<T> ops, T prefix) {
        if (ops instanceof RegistryOps<?> registryOps) {
            Optional<? extends HolderOwner<E>> owner = registryOps.getter(registryKey);
            if (owner.isPresent()) {
                if (!input.canSerializeIn(owner.get())) {
                    return DataResult.error(() -> "Element " + input + " is not valid in current registry set");
                }
                return input.unwrap().map(key -> Identifier.CODEC.encode(key.identifier(), ops, prefix), value -> elementCodec.encode(value, ops, prefix));
            }
        }
        return elementCodec.encode(input.value(), ops, prefix);
    }
    public <T> DataResult<Pair<Holder<E>, T>> decode(DynamicOps<T> ops, T input) {
        if (!(ops instanceof RegistryOps<?> registryOps)) {
            return elementCodec.decode(ops, input).map(p -> p.mapFirst(Holder::direct));
        }
        DataResult<Pair<Identifier, T>> named = Identifier.CODEC.decode(ops, input);
        if (named.result().isEmpty()) {
            if (!allowInline) {
                return DataResult.error(() -> "Inline definitions not allowed here");
            }
            return elementCodec.decode(ops, input).map(p -> p.mapFirst(Holder::direct));
        }
        Optional<HolderGetter<E>> getter = registryOps.getter(registryKey);
        if (getter.isEmpty()) {
            return DataResult.error(() -> "Registry does not exist: " + registryKey.identifier());
        }
        Pair<Identifier, T> pair = named.result().get();
        ResourceKey<E> key = ResourceKey.create(registryKey, pair.getFirst());
        return getter.get().get(key).<DataResult<Holder.Reference<E>>>map(DataResult::success)
                .orElseGet(() -> DataResult.error(() -> "Failed to get element " + key.identifier() + " from registry " + key.registry()))
                .<Pair<Holder<E>, T>>map(h -> Pair.of(h, pair.getSecond())).setLifecycle(Lifecycle.stable());
    }
    public String toString() {
        return "RegistryFileCodec[" + registryKey + " " + elementCodec + "]";
    }
}
