// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.registries;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderOwner;
import net.minecraft.core.HolderSet;
import net.minecraft.tags.TagKey;
public record EmptyTagLookupWrapper<T>(HolderLookup.RegistryLookup<T> parent) implements HolderLookup.RegistryLookup.Delegate<T> {
    public static <T> HolderLookup.RegistryLookup<T> wrap(HolderLookup.RegistryLookup<T> registryLookup) {
        if (registryLookup instanceof EmptyTagLookupWrapper) {
            return registryLookup;
        }
        return new EmptyTagLookupWrapper<>(registryLookup);
    }
    public static HolderLookup.Provider wrap(HolderLookup.Provider provider) {
        return HolderLookup.Provider.create(provider.listRegistries().map(EmptyTagLookupWrapper::wrap));
    }
    public Optional<HolderSet.Named<T>> get(TagKey<T> id) {
        return Optional.of(getOrThrow(id));
    }
    public HolderSet.Named<T> getOrThrow(TagKey<T> id) {
        return HolderSet.emptyNamed(parent, id);
    }
    public boolean canSerialize(HolderOwner<T> owner) {
        return parent.canSerializeIn(owner);
    }
    public Stream<HolderSet.Named<T>> listTags() {
        throw new UnsupportedOperationException("Tags are not available in datagen");
    }
}
