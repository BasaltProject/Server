// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.thread;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.BiFunction;

public class ParallelMapTransform {
    public static <K, U, V> CompletableFuture<Map<K, V>> schedule(Map<K, U> input, BiFunction<K, U, V> operation, int maxTaskCount, Executor executor) {
        int size = input.size();
        if (size == 0) {
            return CompletableFuture.completedFuture(Map.of());
        }
        List<Map.Entry<K, U>> entries = new ArrayList<>(input.entrySet());
        Object[] results = new Object[size];
        int tasks = Math.max(1, Math.min(maxTaskCount, size));
        int batch = (size + tasks - 1) / tasks;
        List<CompletableFuture<?>> pending = new ArrayList<>();
        for (int start = 0; start < size; start += batch) {
            int from = start;
            int to = Math.min(size, start + batch);
            pending.add(CompletableFuture.runAsync(() -> {
                for (int i = from; i < to; i++) {
                    results[i] = operation.apply(entries.get(i).getKey(), entries.get(i).getValue());
                }
            }, executor));
        }
        return CompletableFuture.allOf(pending.toArray(CompletableFuture[]::new)).thenApplyAsync(ignored -> {
            Map<K, V> output = new HashMap<>(size * 2);
            for (int i = 0; i < size; i++) {
                @SuppressWarnings("unchecked")
                V value = (V) results[i];
                output.put(entries.get(i).getKey(), value);
            }
            return output;
        }, executor);
    }

    public static <K, U, V> CompletableFuture<Map<K, V>> schedule(Map<K, U> input, BiFunction<K, U, V> operation, Executor executor) {
        return schedule(input, operation, Runtime.getRuntime().availableProcessors(), executor);
    }
}
