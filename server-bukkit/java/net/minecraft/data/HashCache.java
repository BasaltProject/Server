// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data;

import com.google.common.collect.ImmutableMap;
import com.google.common.hash.HashCode;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import net.minecraft.WorldVersion;

public class HashCache {
    private final Path rootDir;
    private final Path cacheDir;
    private final String versionId;
    private final Map<String, ProviderCache> caches = new HashMap<>();
    private final Map<String, ProviderCache> cachesToWrite = new HashMap<>();
    private final java.util.Set<Path> cachePaths = new java.util.HashSet<>();
    private int writes;

    public HashCache(Path rootDir, Collection<String> providerIds, WorldVersion version) throws IOException {
        this.rootDir = rootDir;
        this.cacheDir = rootDir.resolve(".cache");
        this.versionId = version.id();
        Files.createDirectories(cacheDir);
        for (String providerId : providerIds) {
            Path file = cacheDir.resolve(providerId.replace('/', '_'));
            cachePaths.add(file);
            caches.put(providerId, ProviderCache.load(rootDir, file));
        }
    }

    public boolean shouldRunInThisVersion(String providerId) {
        ProviderCache cache = caches.get(providerId);
        return cache == null || !versionId.equals(cache.version());
    }

    public CompletableFuture<UpdateResult> generateUpdate(String providerId, UpdateFunction function) {
        ProviderCache old = caches.get(providerId);
        CacheUpdater updater = new CacheUpdater(providerId, versionId, old);
        return function.update(updater).thenApply(ignored -> updater.close());
    }

    public void applyUpdate(UpdateResult result) {
        cachesToWrite.put(result.providerId(), result.cache());
        writes += result.writes();
    }

    public void purgeStaleAndWrite() throws IOException {
        for (Map.Entry<String, ProviderCache> entry : cachesToWrite.entrySet()) {
            Path file = cacheDir.resolve(entry.getKey().replace('/', '_'));
            entry.getValue().save(rootDir, file, "");
        }
    }

    static record ProviderCache(String version, ImmutableMap<Path, HashCode> data) {
        public HashCode get(Path path) {
            return data.get(path);
        }

        public int count() {
            return data.size();
        }

        public static ProviderCache load(Path rootDir, Path cacheFile) throws IOException {
            if (!Files.isReadable(cacheFile)) {
                return new ProviderCache("unknown", ImmutableMap.of());
            }
            List<String> lines = Files.readAllLines(cacheFile, StandardCharsets.UTF_8);
            String version = lines.isEmpty() ? "unknown" : lines.get(0).replaceFirst("^// ", "");
            ImmutableMap.Builder<Path, HashCode> map = ImmutableMap.builder();
            for (int index = 1; index < lines.size(); index++) {
                String line = lines.get(index);
                int space = line.indexOf(' ');
                if (space > 0) {
                    map.put(rootDir.resolve(line.substring(space + 1)), HashCode.fromString(line.substring(0, space)));
                }
            }
            return new ProviderCache(version, map.build());
        }

        public void save(Path rootDir, Path cacheFile, String extraHeaderInfo) {
            List<String> lines = new ArrayList<>();
            lines.add("// " + version + (extraHeaderInfo.isEmpty() ? "" : " " + extraHeaderInfo));
            data.forEach((path, hash) -> lines.add(hash + " " + rootDir.relativize(path).toString().replace('\\', '/')));
            try {
                Files.write(cacheFile, lines, StandardCharsets.UTF_8);
            } catch (IOException e) {
                DataProvider.LOGGER.error("Unable to write cache {}", cacheFile, e);
            }
        }
    }

    static record ProviderCacheBuilder(String version, ConcurrentMap<Path, HashCode> data) {
        public ProviderCacheBuilder(String version) {
            this(version, new ConcurrentHashMap<>());
        }

        public void put(Path path, HashCode hash) {
            data.put(path, hash);
        }

        public ProviderCache build() {
            return new ProviderCache(version, ImmutableMap.copyOf(data));
        }
    }

    static class CacheUpdater implements CachedOutput {
        private final String provider;
        private final ProviderCache oldCache;
        private final ProviderCacheBuilder newCache;
        private int writes;

        private CacheUpdater(String provider, String newVersionId, ProviderCache oldCache) {
            this.provider = provider;
            this.oldCache = oldCache == null ? new ProviderCache("unknown", ImmutableMap.of()) : oldCache;
            this.newCache = new ProviderCacheBuilder(newVersionId);
        }

        @Override
        public void writeIfNeeded(Path path, byte[] input, HashCode hash) throws IOException {
            newCache.put(path, hash);
            if (!hash.equals(oldCache.get(path)) || !Files.exists(path)) {
                CachedOutput.NO_CACHE.writeIfNeeded(path, input, hash);
                writes++;
            }
        }

        public UpdateResult close() {
            return new UpdateResult(provider, newCache.build(), writes);
        }
    }

    public static record UpdateResult(String providerId, ProviderCache cache, int writes) {}

    public interface UpdateFunction {
        CompletableFuture<?> update(CachedOutput output);
    }
}
