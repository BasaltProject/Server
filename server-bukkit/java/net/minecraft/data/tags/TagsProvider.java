// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import com.google.gson.JsonElement;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.function.Function;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.NativeData;
import net.minecraft.data.PackOutput;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagBuilder;
import net.minecraft.tags.TagEntry;
import net.minecraft.tags.TagFile;
import net.minecraft.tags.TagKey;

public abstract class TagsProvider<T> implements DataProvider {
    protected final PackOutput.PathProvider pathProvider;
    protected final ResourceKey<? extends Registry<T>> registryKey;
    private final CompletableFuture<HolderLookup.Provider> lookupProvider;
    private final CompletableFuture<TagLookup<T>> parentProvider;
    private final CompletableFuture<Void> contentsDone = new CompletableFuture<>();
    private final Map<Identifier, TagBuilder> builders = new LinkedHashMap<>();

    protected TagsProvider(PackOutput output, ResourceKey<? extends Registry<T>> registryKey, CompletableFuture<HolderLookup.Provider> lookupProvider) {
        this(output, registryKey, lookupProvider, CompletableFuture.completedFuture(TagLookup.empty()));
    }

    protected TagsProvider(PackOutput output, ResourceKey<? extends Registry<T>> registryKey, CompletableFuture<HolderLookup.Provider> lookupProvider, CompletableFuture<TagLookup<T>> parentProvider) {
        this.pathProvider = output.createRegistryTagsPathProvider(registryKey);
        this.registryKey = registryKey;
        this.lookupProvider = lookupProvider;
        this.parentProvider = parentProvider;
    }

    @Override
    public final String getName() {
        return "Tags for " + registryKey.identifier();
    }

    protected abstract void addTags(HolderLookup.Provider registries);

    protected final void addNativeTags() {
        addNativeTags("data/minecraft/");
    }

    protected final void addNativeTags(String packRoot) {
        String folder = "tags/" + registryKey.identifier().getPath();
        for (Map.Entry<Identifier, JsonElement> entry : NativeData.json(packRoot, folder).entrySet()) {
            TagFile file = TagFile.CODEC.parse(com.mojang.serialization.JsonOps.INSTANCE, entry.getValue()).getOrThrow();
            TagBuilder builder = getOrCreateRawBuilder(TagKey.create(registryKey, entry.getKey()));
            builder.setReplace(file.replace());
            file.entries().forEach(builder::add);
        }
    }

    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        return lookupProvider.thenApply(registries -> {
            builders.clear();
            addTags(registries);
            contentsDone.complete(null);
            return registries;
        }).thenCombine(parentProvider, (registries, parents) -> {
            Map<Path, TagFile> files = new LinkedHashMap<>();
            builders.forEach((id, builder) -> files.put(pathProvider.json(id), new TagFile(builder.build(), builder.shouldReplace())));
            return files;
        }).thenCompose(files -> {
            CompletableFuture<?>[] writes = files.entrySet().stream()
                .map(entry -> DataProvider.saveStable(cache, TagFile.CODEC, entry.getValue(), entry.getKey())).toArray(CompletableFuture[]::new);
            return CompletableFuture.allOf(writes);
        });
    }

    protected TagBuilder getOrCreateRawBuilder(TagKey<T> tag) {
        return builders.computeIfAbsent(tag.location(), id -> TagBuilder.create());
    }

    public CompletableFuture<TagLookup<T>> contentsGetter() {
        return contentsDone.thenApply(done -> key -> Optional.ofNullable(builders.get(key.location())));
    }

    protected CompletableFuture<HolderLookup.Provider> createContentsProvider() {
        return lookupProvider;
    }

    protected TagAppender<T> tag(TagKey<T> tag) {
        return tag(tag, false);
    }

    protected TagAppender<T> tag(TagKey<T> tag, boolean replace) {
        return TagAppender.forBuilder(getOrCreateRawBuilder(tag).setReplace(replace));
    }

    public interface TagLookup<T> extends Function<TagKey<T>, Optional<TagBuilder>> {
        static <T> TagLookup<T> empty() {
            return key -> Optional.empty();
        }

        default boolean contains(TagKey<T> key) {
            return apply(key).isPresent();
        }
    }
}
