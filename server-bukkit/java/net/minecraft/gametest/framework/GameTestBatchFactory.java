// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Rotation;

public class GameTestBatchFactory {
    private static final int kMaxTestsPerBatch = 50;
    public static final TestDecorator DIRECT = (test, level) -> Stream.of(new GameTestInfo(test, Rotation.NONE, level, RetryOptions.noRetries()));

    private static <T> List<List<T>> chunked(List<T> list, int size) {
        List<List<T>> out = new ArrayList<>();
        for (int from = 0; from < list.size(); from += size) {
            out.add(list.subList(from, Math.min(list.size(), from + size)));
        }
        return out;
    }

    public static List<GameTestBatch> divideIntoBatches(Collection<Holder.Reference<GameTestInstance>> allTests, TestDecorator decorator, MinecraftServer server) {
        Map<BatchKey, List<Holder.Reference<GameTestInstance>>> grouped = new LinkedHashMap<>();
        for (Holder.Reference<GameTestInstance> test : allTests) {
            grouped.computeIfAbsent(new BatchKey(test.value().batch(), test.value().dimension()), k -> new ArrayList<>()).add(test);
        }
        List<GameTestBatch> batches = new ArrayList<>();
        for (Map.Entry<BatchKey, List<Holder.Reference<GameTestInstance>>> entry : grouped.entrySet()) {
            ServerLevel level = server.getLevel(entry.getKey().dimension());
            if (level == null) {
                throw new IllegalStateException("Missing level for dimension: " + entry.getKey().dimension().identifier());
            }
            List<GameTestInfo> infos = entry.getValue().stream().flatMap(test -> decorator.decorate(test, level)).toList();
            int index = 0;
            for (List<GameTestInfo> part : chunked(infos, kMaxTestsPerBatch)) {
                batches.add(toGameTestBatch(part, entry.getKey().environment(), index++, entry.getKey().dimension()));
            }
        }
        return batches;
    }

    public static GameTestRunner.GameTestBatcher fromGameTestInfo() {
        return fromGameTestInfo(kMaxTestsPerBatch);
    }

    public static GameTestRunner.GameTestBatcher fromGameTestInfo(int maxTestsPerBatch) {
        return infos -> {
            Map<BatchKey, List<GameTestInfo>> grouped = new LinkedHashMap<>();
            for (GameTestInfo info : infos) {
                if (Objects.nonNull(info)) {
                    grouped.computeIfAbsent(new BatchKey(info.getTest().batch(), info.getTest().dimension()), k -> new ArrayList<>()).add(info);
                }
            }
            List<GameTestBatch> batches = new ArrayList<>();
            for (Map.Entry<BatchKey, List<GameTestInfo>> entry : grouped.entrySet()) {
                int index = 0;
                for (List<GameTestInfo> part : chunked(entry.getValue(), maxTestsPerBatch)) {
                    batches.add(toGameTestBatch(List.copyOf(part), entry.getKey().environment(), index++, entry.getKey().dimension()));
                }
            }
            return batches;
        };
    }

    public static GameTestBatch toGameTestBatch(Collection<GameTestInfo> tests, Holder<TestEnvironmentDefinition<?>> batch, int counter, ResourceKey<Level> dimension) {
        return new GameTestBatch(counter, tests, batch, dimension);
    }

    private static record BatchKey(Holder<TestEnvironmentDefinition<?>> environment, ResourceKey<Level> dimension) {
    }

    @FunctionalInterface
    public interface TestDecorator {
        Stream<GameTestInfo> decorate(Holder.Reference<GameTestInstance> test, ServerLevel level);
    }
}
