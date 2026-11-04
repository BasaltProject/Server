// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.WritableRegistry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.packs.resources.ResourceManager;
import net.minecraft.util.DependencySorter;
public class TagLoader<T> {
    public TagLoader(ElementLookup<T> elementLookup, String directory) {
        
    }
    public Map<Identifier, List<EntryWithSource>> load(ResourceManager resourceManager) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.tags.TagLoader#load");
    }
    public Map<Identifier, List<T>> build(Map<Identifier, List<EntryWithSource>> builders) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.tags.TagLoader#build");
    }
    public static <T> Map<TagKey<T>, List<Holder<T>>> loadTagsFromNetwork(TagNetworkSerialization.NetworkPayload tags, Registry<T> registry) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.tags.TagLoader#loadTagsFromNetwork");
    }
    public static List<Registry.PendingTags<?>> loadTagsForExistingRegistries(ResourceManager manager, RegistryAccess layer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.tags.TagLoader#loadTagsForExistingRegistries");
    }
    public static <T> void loadTagsForRegistry(ResourceManager manager, WritableRegistry<T> registry) {
    }
    public static <T> Map<TagKey<T>, List<Holder<T>>> loadTagsForRegistry(ResourceManager manager, ResourceKey<? extends Registry<T>> registryKey, ElementLookup<Holder<T>> lookup) {
        return new java.util.LinkedHashMap<>();
    }
    public static List<HolderLookup.RegistryLookup<?>> buildUpdatedLookups(RegistryAccess.Frozen registries, List<Registry.PendingTags<?>> tags) {
        List<HolderLookup.RegistryLookup<?>> lookups = new java.util.ArrayList<>();
        registries.registries().forEach(entry -> lookups.add(entry.value()));
        return lookups;
    }
    public static record EntryWithSource(TagEntry entry, String source) {
    }
    static record SortingEntry(List<EntryWithSource> entries) implements DependencySorter.Entry<Identifier> {
        public void visitRequiredDependencies(Consumer<Identifier> output) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.tags.TagLoader.SortingEntry#visitRequiredDependencies");
        }
        public void visitOptionalDependencies(Consumer<Identifier> output) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.tags.TagLoader.SortingEntry#visitOptionalDependencies");
        }
    }
    public static record LoadResult<T>(ResourceKey<? extends Registry<T>> key, Map<TagKey<T>, List<Holder<T>>> tags) {
    }
    public interface ElementLookup<T> {
        Optional<? extends T> get(Identifier id, boolean required);
        static <T> ElementLookup<? extends Holder<T>> fromFrozenRegistry(Registry<T> registry) {
        return (id, required) -> registry.get(id);
        }
        static <T> ElementLookup<Holder<T>> fromWritableRegistry(WritableRegistry<T> registry) {
        return (id, required) -> registry.get(id).map(holder -> (Holder<T>) holder);
        }
        static <T> ElementLookup<Holder<T>> fromGetters(ResourceKey<? extends Registry<T>> registryKey, HolderGetter<T> writable, HolderGetter<T> immutable) {
        return (id, required) -> {
            ResourceKey<T> key = ResourceKey.create(registryKey, id);
            return (required ? writable : immutable).get(key).map(holder -> (Holder<T>) holder);
        };
        }
    }
}
