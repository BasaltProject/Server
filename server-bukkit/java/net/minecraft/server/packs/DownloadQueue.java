// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
import java.util.HashMap;
import java.util.HashSet;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.UUIDUtil;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.FileUtil;
import net.minecraft.util.Util;
import net.minecraft.util.eventlog.JsonEventLog;
import net.minecraft.util.thread.ConsecutiveExecutor;
import com.google.common.hash.HashCode;
import com.google.common.hash.HashFunction;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.nio.file.Path;
import java.time.Instant;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import net.minecraft.util.HttpUtil;
public class DownloadQueue implements AutoCloseable {
    private final Path cache;
    private final JsonEventLog<LogEntry> log;
    private final ConsecutiveExecutor tasks = new ConsecutiveExecutor(Util.nonCriticalIoPool(), "download-queue");

    private BatchResult run(BatchConfig config, Map<UUID, DownloadRequest> requests) {
        BatchResult outcome = new BatchResult();
        requests.forEach((id, request) -> {
            Path file = null;
            try {
                file = HttpUtil.downloadFile(cache.resolve(id.toString()), request.url(), config.headers(), config.hashFunction(), request.hash(),
                    config.maxSize(), config.proxy(), config.listener());
                outcome.downloaded().put(id, file);
            } catch (Exception failure) {
                outcome.failed().add(id);
            }
            try {
                log.write(new LogEntry(id, request.url().toString(), Instant.now(), Optional.ofNullable(request.hash()).map(HashCode::toString),
                    file != null ? describe(file) : Either.left("download_failed")));
            } catch (Exception ignored) {
                return;
            }
        });
        return outcome;
    }

    private Either<String, FileInfoEntry> describe(Path file) {
        try {
            return Either.right(new FileInfoEntry(cache.relativize(file).toString(), java.nio.file.Files.size(file)));
        } catch (IOException unreadable) {
            return Either.left("no_access");
        }
    }

    public DownloadQueue(Path cacheDir) throws IOException {
        cache = cacheDir;
        FileUtil.createDirectoriesSafe(cacheDir);
        log = JsonEventLog.open(LogEntry.CODEC, cacheDir.resolve("log.json"));
        DownloadCacheCleaner.vacuumCacheDir(cacheDir, 20);
        
    }
    public CompletableFuture<BatchResult> downloadBatch(BatchConfig config, Map<UUID, DownloadRequest> requests) {
        return CompletableFuture.supplyAsync(() -> run(config, requests), tasks::schedule);
    }
    public void close() throws IOException {
        tasks.close();
        log.close();
    }
    static record FileInfoEntry(String name, long size) {
        public static final Codec<FileInfoEntry> CODEC = RecordCodecBuilder.create(i -> i.group(Codec.STRING.fieldOf("name").forGetter(FileInfoEntry::name),
            Codec.LONG.fieldOf("size").forGetter(FileInfoEntry::size)).apply(i, FileInfoEntry::new));
    }
    static record LogEntry(UUID id, String url, Instant time, Optional<String> hash, Either<String, FileInfoEntry> errorOrFileInfo) {
        public static final Codec<LogEntry> CODEC = RecordCodecBuilder.create(i -> i.group(UUIDUtil.STRING_CODEC.fieldOf("id").forGetter(LogEntry::id),
            Codec.STRING.fieldOf("url").forGetter(LogEntry::url), ExtraCodecs.INSTANT_ISO8601.fieldOf("time").forGetter(LogEntry::time),
            Codec.STRING.optionalFieldOf("hash").forGetter(LogEntry::hash),
            Codec.mapEither(Codec.STRING.fieldOf("error"), FileInfoEntry.CODEC.fieldOf("file")).forGetter(LogEntry::errorOrFileInfo)).apply(i, LogEntry::new));
    }
    public static record BatchResult(Map<UUID, Path> downloaded, Set<UUID> failed) {
        public BatchResult() {
            this(new HashMap<>(), new HashSet<>());
        }
    }
    public static record DownloadRequest(URL url, HashCode hash) {
    }
    public static record BatchConfig(HashFunction hashFunction, int maxSize, Map<String, String> headers, Proxy proxy, HttpUtil.DownloadProgressListener listener) {
    }
}
