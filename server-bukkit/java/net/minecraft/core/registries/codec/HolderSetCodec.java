// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.registries.codec;
import com.mojang.datafixers.util.Either;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderOwner;
import net.minecraft.core.HolderSet;
import net.minecraft.core.Registry;
import net.minecraft.resources.RegistryOps;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.util.ExtraCodecs;
public class HolderSetCodec<E> implements Codec<HolderSet<E>> {
    private final ResourceKey<? extends Registry<E>> registryKey;
    private final Codec<Either<TagKey<E>, List<Holder<E>>>> eitherCodec;
    private static <E> Codec<List<Holder<E>>> listOf(Codec<Holder<E>> element, boolean alwaysUseList) {
        Codec<List<Holder<E>>> list = element.listOf();
        return alwaysUseList ? list : ExtraCodecs.compactListCodec(element, list);
    }
    public static <E> Codec<HolderSet<E>> create(ResourceKey<? extends Registry<E>> registryKey, Codec<Holder<E>> elementCodec, boolean alwaysUseList) {
        return new HolderSetCodec<>(registryKey, elementCodec, alwaysUseList);
    }
    private HolderSetCodec(ResourceKey<? extends Registry<E>> registryKey, Codec<Holder<E>> elementCodec, boolean alwaysUseList) {
        this.registryKey = registryKey;
        this.eitherCodec = Codec.either(TagKey.hashedCodec(registryKey), listOf(elementCodec, alwaysUseList));
    }
    public <T> DataResult<Pair<HolderSet<E>, T>> decode(DynamicOps<T> ops, T input) {
        return eitherCodec.decode(ops, input).flatMap(decoded -> {
            DataResult<HolderSet<E>> set = decoded.getFirst().map(tag -> {
                if (!(ops instanceof RegistryOps<T> registryOps)) {
                    return DataResult.<HolderSet<E>>error(() -> "Registries are not available in this context");
                }
                Optional<HolderGetter<E>> getter = registryOps.getter(registryKey);
                if (getter.isEmpty()) {
                    return DataResult.<HolderSet<E>>error(() -> "Registry " + registryKey.identifier() + " is not available in this context");
                }
                return lookupTag(getter.get(), tag);
            }, values -> DataResult.success(HolderSet.direct(values)));
            return set.map(holders -> Pair.of(holders, decoded.getSecond()));
        });
    }
    private static <E> DataResult<HolderSet<E>> lookupTag(HolderGetter<E> registry, TagKey<E> key) {
        return registry.get(key).<DataResult<HolderSet<E>>>map(DataResult::success)
                .orElseGet(() -> DataResult.error(() -> "Missing tag: '" + key.location() + "' in '" + key.registry().identifier() + "'"));
    }
    public <T> DataResult<T> encode(HolderSet<E> input, DynamicOps<T> ops, T prefix) {
        if (input instanceof HolderSet.Named<E> named && ops instanceof RegistryOps<T> registryOps) {
            Optional<? extends HolderOwner<E>> owner = registryOps.getter(registryKey);
            if (owner.isEmpty()) {
                return DataResult.error(() -> "Registry " + registryKey.identifier() + " is not available in this context");
            }
            if (!named.canSerializeIn(owner.get())) {
                return DataResult.error(() -> "HolderSet " + named + " is not valid in current registry set");
            }
            return eitherCodec.encode(Either.left(named.key()), ops, prefix);
        }
        return eitherCodec.encode(Either.right(input.stream().toList()), ops, prefix);
    }
}
