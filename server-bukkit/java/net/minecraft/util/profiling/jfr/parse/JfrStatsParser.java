// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling.jfr.parse;
import com.mojang.datafixers.util.Pair;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Path;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Spliterator;
import java.util.Spliterators;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;
import net.minecraft.util.profiling.jfr.stats.ChunkGenStat;
import net.minecraft.util.profiling.jfr.stats.ChunkIdentification;
import net.minecraft.util.profiling.jfr.stats.CpuLoadStat;
import net.minecraft.util.profiling.jfr.stats.FileIOStat;
import net.minecraft.util.profiling.jfr.stats.FpsStat;
import net.minecraft.util.profiling.jfr.stats.GcHeapStat;
import net.minecraft.util.profiling.jfr.stats.IoSummary;
import net.minecraft.util.profiling.jfr.stats.PacketIdentification;
import net.minecraft.util.profiling.jfr.stats.StructureGenStat;
import net.minecraft.util.profiling.jfr.stats.ThreadAllocationStat;
import net.minecraft.util.profiling.jfr.stats.TickTimeStat;
public class JfrStatsParser {
    private Instant started = Instant.EPOCH;
    private Instant ended = Instant.EPOCH;
    private final List<ChunkGenStat> chunkGen = new ArrayList<>();
    private final List<StructureGenStat> structureGen = new ArrayList<>();
    private final List<CpuLoadStat> cpuLoad = new ArrayList<>();
    private final Map<PacketIdentification, MutableCountAndSize> received = new HashMap<>();
    private final Map<PacketIdentification, MutableCountAndSize> sent = new HashMap<>();
    private final Map<ChunkIdentification, MutableCountAndSize> chunksRead = new HashMap<>();
    private final Map<ChunkIdentification, MutableCountAndSize> chunksWritten = new HashMap<>();
    private final List<FileIOStat> fileWrites = new ArrayList<>();
    private final List<FileIOStat> fileReads = new ArrayList<>();
    private int collections;
    private Duration gcTotal = Duration.ZERO;
    private final List<GcHeapStat> heap = new ArrayList<>();
    private final List<ThreadAllocationStat> allocations = new ArrayList<>();
    private final List<FpsStat> fps = new ArrayList<>();
    private final List<TickTimeStat> tickTimes = new ArrayList<>();
    private Duration worldCreation;
    private JfrStatsParser(Stream<RecordedEvent> events) {
        events.forEach(this::record);
    }
    public static JfrStatsResult parse(Path path) {
        try (RecordingFile file = new RecordingFile(path)) {
            Iterator<RecordedEvent> iterator = new Iterator<>() {
                public boolean hasNext() {
                    return file.hasMoreEvents();
                }
                public RecordedEvent next() {
                    if (!hasNext()) {
                        throw new NoSuchElementException();
                    }
                    try {
                        return file.readEvent();
                    } catch (IOException e) {
                        throw new UncheckedIOException(e);
                    }
                }
            };
            Stream<RecordedEvent> stream = StreamSupport.stream(Spliterators.spliteratorUnknownSize(iterator, Spliterator.ORDERED | Spliterator.NONNULL), false);
            return new JfrStatsParser(stream).results();
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }
    private JfrStatsResult results() {
        Duration span = Duration.between(started, ended);
        return new JfrStatsResult(started, ended, span, worldCreation, fps, tickTimes, cpuLoad,
                GcHeapStat.summary(span, heap, gcTotal, collections), ThreadAllocationStat.summary(allocations),
                summarize(span, received), summarize(span, sent), summarize(span, chunksWritten), summarize(span, chunksRead),
                FileIOStat.summary(span, fileWrites), FileIOStat.summary(span, fileReads), chunkGen, structureGen);
    }
    private void record(RecordedEvent event) {
        if (event.getEndTime().isAfter(ended) || ended.equals(Instant.EPOCH)) {
            ended = event.getEndTime();
        }
        if (event.getStartTime().isBefore(started) || started.equals(Instant.EPOCH)) {
            started = event.getStartTime();
        }
        switch (event.getEventType().getName()) {
            case "minecraft.ChunkGeneration" -> chunkGen.add(ChunkGenStat.from(event));
            case "minecraft.StructureGeneration" -> structureGen.add(StructureGenStat.from(event));
            case "minecraft.LoadWorld" -> worldCreation = event.getDuration();
            case "minecraft.ClientFps" -> fps.add(FpsStat.from(event, "fps"));
            case "minecraft.ServerTickTime" -> tickTimes.add(TickTimeStat.from(event));
            case "minecraft.PacketReceived" -> count(received, PacketIdentification.from(event), event.getInt("bytes"));
            case "minecraft.PacketSent" -> count(sent, PacketIdentification.from(event), event.getInt("bytes"));
            case "minecraft.ChunkRegionRead" -> count(chunksRead, ChunkIdentification.from(event), event.getInt("bytes"));
            case "minecraft.ChunkRegionWrite" -> count(chunksWritten, ChunkIdentification.from(event), event.getInt("bytes"));
            case "jdk.ThreadAllocationStatistics" -> allocations.add(ThreadAllocationStat.from(event));
            case "jdk.GCHeapSummary" -> heap.add(GcHeapStat.from(event));
            case "jdk.CPULoad" -> cpuLoad.add(CpuLoadStat.from(event));
            case "jdk.FileWrite" -> fileWrites.add(new FileIOStat(event.getDuration(), event.getString("path"), event.getLong("bytesWritten")));
            case "jdk.FileRead" -> fileReads.add(new FileIOStat(event.getDuration(), event.getString("path"), event.getLong("bytesRead")));
            case "jdk.GarbageCollection" -> {
                collections++;
                gcTotal = gcTotal.plus(event.getDuration());
            }
            default -> {
            }
        }
    }
    private static <K> void count(Map<K, MutableCountAndSize> into, K key, int bytes) {
        into.computeIfAbsent(key, ignored -> new MutableCountAndSize()).increment(bytes);
    }
    private static <T> IoSummary<T> summarize(Duration span, Map<T, MutableCountAndSize> stats) {
        List<Pair<T, IoSummary.CountAndSize>> rows = stats.entrySet().stream().map(e -> Pair.of(e.getKey(), e.getValue().toCountAndSize())).toList();
        return new IoSummary<>(span, rows);
    }
    public static final class MutableCountAndSize {
        private long count;
        private long totalSize;
        public void increment(int bytes) {
            totalSize += bytes;
            count++;
        }
        public IoSummary.CountAndSize toCountAndSize() {
            return new IoSummary.CountAndSize(count, totalSize);
        }
    }
}
