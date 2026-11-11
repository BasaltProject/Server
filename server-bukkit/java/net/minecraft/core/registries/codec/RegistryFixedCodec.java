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
public final class RegistryFixedCodec<E> implements Codec<Holder<E>> {
    private final ResourceKey<? extends Registry<E>> registryKey;
    public static <E> Codec<Holder<E>> create(ResourceKey<? extends Registry<E>> registryKey) {
        return new RegistryFixedCodec<>(registryKey);
    }
    private RegistryFixedCodec(ResourceKey<? extends Registry<E>> registryKey) {
        this.registryKey = registryKey;
    }
    public <T> DataResult<T> encode(Holder<E> input, DynamicOps<T> ops, T prefix) {
        if (ops instanceof RegistryOps<?> registryOps) {
            Optional<? extends HolderOwner<E>> owner = registryOps.getter(registryKey);
            if (owner.isPresent()) {
                if (!input.canSerializeIn(owner.get())) {
                    return DataResult.error(() -> "Element " + input + " is not valid in current registry set");
                }
                return input.unwrap().map(key -> Identifier.CODEC.encode(key.identifier(), ops, prefix),
                        value -> DataResult.<T>error(() -> "Elements from registry " + registryKey + " can't be serialized to a value"));
            }
        }
        return DataResult.error(() -> "Can't access registry " + registryKey.identifier());
    }
    public <T> DataResult<Pair<Holder<E>, T>> decode(DynamicOps<T> ops, T input) {
        if (ops instanceof RegistryOps<?> registryOps) {
            Optional<HolderGetter<E>> getter = registryOps.getter(registryKey);
            if (getter.isPresent()) {
                return Identifier.CODEC.decode(ops, input).flatMap(pair -> {
                    ResourceKey<E> key = ResourceKey.create(registryKey, pair.getFirst());
                    return getter.get().get(key).<DataResult<Holder.Reference<E>>>map(DataResult::success)
                            .orElseGet(() -> DataResult.error(() -> "Failed to get element " + key.identifier() + " from registry " + key.registry()))
                            .<Pair<Holder<E>, T>>map(h -> Pair.of(h, pair.getSecond())).setLifecycle(Lifecycle.stable());
                });
            }
        }
        return DataResult.error(() -> "Can't access registry " + registryKey.identifier());
    }
    public String toString() {
        return "RegistryFixedCodec[" + registryKey + "]";
    }
}
