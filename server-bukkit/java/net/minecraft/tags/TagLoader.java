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
    private final ElementLookup<T> elementLookup;
    private final String directory;
    public TagLoader(ElementLookup<T> elementLookup, String directory) {
        this.elementLookup = elementLookup;
        this.directory = directory;
    }
    public Map<Identifier, List<EntryWithSource>> load(ResourceManager resourceManager) {
        Map<Identifier, List<EntryWithSource>> builders = new java.util.HashMap<>();
        net.minecraft.resources.FileToIdConverter lister = net.minecraft.resources.FileToIdConverter.json(directory);
        for (Map.Entry<Identifier, List<net.minecraft.server.packs.resources.Resource>> entry : lister.listMatchingResourceStacks(resourceManager).entrySet()) {
            Identifier id = lister.fileToId(entry.getKey());
            for (net.minecraft.server.packs.resources.Resource resource : entry.getValue()) {
                try (java.io.Reader reader = resource.openAsReader()) {
                    com.google.gson.JsonElement element = com.google.gson.JsonParser.parseReader(reader);
                    List<EntryWithSource> contents = builders.computeIfAbsent(id, key -> new java.util.ArrayList<>());
                    TagFile parsed = TagFile.CODEC.parse(new com.mojang.serialization.Dynamic<>(com.mojang.serialization.JsonOps.INSTANCE, element)).getOrThrow();
                    if (parsed.replace()) {
                        contents.clear();
                    }
                    String source = resource.sourcePackId();
                    parsed.entries().forEach(parsedEntry -> contents.add(new EntryWithSource(parsedEntry, source)));
                } catch (Exception e) {
                    System.getLogger("TagLoader").log(System.Logger.Level.WARNING, "Couldn't read tag list " + id + " from " + entry.getKey() + " in data pack " + resource.sourcePackId() + ": " + e);
                }
            }
        }
        return builders;
    }
    private com.mojang.datafixers.util.Either<List<EntryWithSource>, List<T>> tryBuildTag(TagEntry.Lookup<T> lookup, List<EntryWithSource> entries) {
        java.util.LinkedHashSet<T> values = new java.util.LinkedHashSet<>();
        List<EntryWithSource> missing = new java.util.ArrayList<>();
        for (EntryWithSource entry : entries) {
            if (!entry.entry().build(lookup, values::add)) {
                missing.add(entry);
            }
        }
        return missing.isEmpty() ? com.mojang.datafixers.util.Either.right(List.copyOf(values)) : com.mojang.datafixers.util.Either.left(missing);
    }
    public Map<Identifier, List<T>> build(Map<Identifier, List<EntryWithSource>> builders) {
        Map<Identifier, List<T>> newTags = new java.util.HashMap<>();
        TagEntry.Lookup<T> lookup = new TagEntry.Lookup<T>() {
            public T element(Identifier key, boolean required) {
                return elementLookup.get(key, required).orElse(null);
            }

            public java.util.Collection<T> tag(Identifier key) {
                return newTags.get(key);
            }
        };
        DependencySorter<Identifier, SortingEntry> sorter = new DependencySorter<>();
        builders.forEach((id, entries) -> sorter.addEntry(id, new SortingEntry(entries)));
        sorter.orderByDependencies((id, contents) -> tryBuildTag(lookup, contents.entries).ifLeft(missing ->
            System.getLogger("TagLoader").log(System.Logger.Level.WARNING, "Couldn't load tag " + id + " as it is missing following references: "
                + missing.stream().map(Object::toString).collect(java.util.stream.Collectors.joining(", ")))).ifRight(tag -> newTags.put(id, tag)));
        return newTags;
    }
    public static <T> Map<TagKey<T>, List<Holder<T>>> loadTagsFromNetwork(TagNetworkSerialization.NetworkPayload tags, Registry<T> registry) {
        return tags.resolve(registry).tags();
    }
    public static List<Registry.PendingTags<?>> loadTagsForExistingRegistries(ResourceManager manager, RegistryAccess layer) {
        List<Registry.PendingTags<?>> pending = new java.util.ArrayList<>();
        layer.registries().forEach(entry -> loadPendingTags(manager, entry.value()).ifPresent(pending::add));
        return List.copyOf(pending);
    }
    private static <T> Optional<Registry.PendingTags<T>> loadPendingTags(ResourceManager manager, Registry<T> registry) {
        ResourceKey<? extends Registry<T>> key = registry.key();
        TagLoader<Holder<T>> loader = new TagLoader<Holder<T>>((id, required) -> registry.get(id).map(holder -> (Holder<T>) holder), net.minecraft.core.registries.Registries.tagsDirPath(key));
        LoadResult<T> result = new LoadResult<T>(key, wrap(key, loader.build(loader.load(manager))));
        return result.tags().isEmpty() ? Optional.empty() : Optional.of(registry.prepareTagReload(result));
    }
    private static <T> Map<TagKey<T>, List<Holder<T>>> wrap(ResourceKey<? extends Registry<T>> registryKey, Map<Identifier, List<Holder<T>>> tags) {
        Map<TagKey<T>, List<Holder<T>>> wrapped = new java.util.HashMap<>();
        tags.forEach((id, holders) -> wrapped.put(TagKey.create(registryKey, id), holders));
        return Map.copyOf(wrapped);
    }
    public static <T> void loadTagsForRegistry(ResourceManager manager, WritableRegistry<T> registry) {
        Map<TagKey<T>, List<Holder<T>>> tags = loadTagsForRegistry(manager, registry.key(), ElementLookup.fromWritableRegistry(registry));
        if (registry instanceof net.minecraft.core.MappedRegistry<T> mapped) {
            mapped.bindTags(tags);
        }
    }
    public static <T> Map<TagKey<T>, List<Holder<T>>> loadTagsForRegistry(ResourceManager manager, ResourceKey<? extends Registry<T>> registryKey, ElementLookup<Holder<T>> lookup) {
        TagLoader<Holder<T>> loader = new TagLoader<Holder<T>>(lookup, net.minecraft.core.registries.Registries.tagsDirPath(registryKey));
        return wrap(registryKey, loader.build(loader.load(manager)));
    }
    public static List<HolderLookup.RegistryLookup<?>> buildUpdatedLookups(RegistryAccess.Frozen registries, List<Registry.PendingTags<?>> tags) {
        List<HolderLookup.RegistryLookup<?>> lookups = new java.util.ArrayList<>();
        registries.registries().forEach(entry -> {
            Registry.PendingTags<?> found = null;
            for (Registry.PendingTags<?> pending : tags) {
                if (pending.key() == entry.key()) {
                    found = pending;
                }
            }
            lookups.add(found != null ? found.lookup() : entry.value());
        });
        return lookups;
    }
    public static record EntryWithSource(TagEntry entry, String source) {
    }
    static record SortingEntry(List<EntryWithSource> entries) implements DependencySorter.Entry<Identifier> {
        public void visitRequiredDependencies(Consumer<Identifier> output) {
            entries.forEach(entry -> entry.entry().visitRequiredDependencies(output));
        }
        public void visitOptionalDependencies(Consumer<Identifier> output) {
            entries.forEach(entry -> entry.entry().visitOptionalDependencies(output));
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
