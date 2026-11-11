// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import com.google.gson.JsonElement;
import com.google.gson.JsonParser;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Decoder;
import com.mojang.serialization.Lifecycle;
import java.io.BufferedReader;
import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistrationInfo;
import net.minecraft.core.Registry;
import net.minecraft.nbt.Tag;
import net.minecraft.server.packs.resources.Resource;
import net.minecraft.server.packs.resources.ResourceProvider;
import net.minecraft.tags.TagKey;

/** Registry contents are loaded natively, so these tasks only carry the declaration and report an empty load. */
public abstract class RegistryLoadTask<T> {
    protected final RegistryDataLoader.RegistryData<T> data;
    protected final Map<ResourceKey<?>, Exception> loadingErrors;
    private final Lifecycle lifecycle;
    private boolean frozen;

    protected RegistryLoadTask(RegistryDataLoader.RegistryData<T> data, Lifecycle lifecycle, Map<ResourceKey<?>, Exception> loadingErrors) {
        this.data = data;
        this.lifecycle = lifecycle;
        this.loadingErrors = loadingErrors;
    }

    protected ResourceKey<? extends Registry<T>> registryKey() {
        return data.key();
    }

    protected Registry<T> readOnlyRegistry() {
        throw new UnsupportedOperationException("Registry " + data.key() + " is held by the native server");
    }

    public abstract CompletableFuture<?> load(RegistryOps.RegistryInfoLookup context, Executor executor);

    protected void registerElements(Stream<PendingRegistration<T>> elements) {
        elements.forEach(element -> element.value().ifRight(error -> loadingErrors.put(element.key(), error)));
    }

    protected void registerTags(Map<TagKey<T>, List<Holder<T>>> pendingTags) {
    }

    public boolean freezeRegistry(Map<ResourceKey<?>, Exception> loadingErrors) {
        frozen = true;
        return loadingErrors.isEmpty();
    }

    public Optional<Registry<T>> validateRegistry(Map<ResourceKey<?>, Exception> loadingErrors) {
        return Optional.empty();
    }

    protected static record PendingRegistration<T>(ResourceKey<T> key, Either<T, Exception> value, RegistrationInfo registrationInfo) {
        public static <T> Either<T, Exception> loadFromResource(Decoder<T> elementDecoder, RegistryOps<JsonElement> ops, ResourceKey<T> elementKey,
                Resource thunk) {
            try (BufferedReader reader = new BufferedReader(new java.io.InputStreamReader(thunk.open(), java.nio.charset.StandardCharsets.UTF_8))) {
                JsonElement json = JsonParser.parseReader(reader);
                return Either.left(elementDecoder.parse(ops, json).getOrThrow(message -> new IllegalStateException(message)));
            } catch (IOException | RuntimeException e) {
                return Either.right(e);
            }
        }

        public static <T> Either<T, Exception> findAndLoadFromResource(Decoder<T> elementDecoder, RegistryOps<JsonElement> ops, ResourceKey<T> elementKey,
                FileToIdConverter converter, ResourceProvider resourceProvider) {
            Identifier file = converter.idToFile(elementKey.identifier());
            return resourceProvider.getResource(file).map(resource -> loadFromResource(elementDecoder, ops, elementKey, resource))
                .orElseGet(() -> Either.right(new IllegalStateException("Missing resource " + file)));
        }

        public static <T> Either<T, Exception> loadFromNetwork(Decoder<T> elementDecoder, RegistryOps<Tag> ops, ResourceKey<T> elementKey, Tag contents) {
            try {
                return Either.left(elementDecoder.parse(ops, contents).getOrThrow(message -> new IllegalStateException(message)));
            } catch (RuntimeException e) {
                return Either.right(e);
            }
        }
    }
}
