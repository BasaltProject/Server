// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.google.common.collect.Iterators;
import java.util.Iterator;
import java.util.stream.Collectors;
import net.minecraft.TracingExecutor;
import org.basaltmc.bukkit.Unsupported;
import com.google.common.base.Ticker;
import com.google.common.escape.Escaper;
import com.mojang.datafixers.DSL;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.types.Type;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.Dynamic;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import it.unimi.dsi.fastutil.objects.ObjectArrayList;
import java.io.IOException;
import java.lang.management.ThreadInfo;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.file.CopyOption;
import java.nio.file.Path;
import java.nio.file.spi.FileSystemProvider;
import java.time.format.DateTimeFormatter;
import java.time.format.FormatStyle;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Predicate;
import java.util.function.Supplier;
import java.util.function.ToIntFunction;
import java.util.function.UnaryOperator;
import java.util.stream.Collector;
import java.util.stream.IntStream;
import java.util.stream.LongStream;
import java.util.stream.Stream;
import net.minecraft.CharPredicate;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.state.properties.Property;
public class Util {
    protected Util() {
    }
    private static final TracingExecutor kBackgroundExecutor = makeExecutor("Main");
    private static final TracingExecutor kIoPool = makeIoExecutor("IO-Worker-", false);
    private static final TracingExecutor kDownloadPool = makeIoExecutor("Download-", true);
    private static TracingExecutor makeExecutor(String name) {
        java.util.concurrent.atomic.AtomicInteger workers = new java.util.concurrent.atomic.AtomicInteger(1);
        return new TracingExecutor(new java.util.concurrent.ForkJoinPool(maxAllowedExecutorThreads(), pool -> {
            java.util.concurrent.ForkJoinWorkerThread thread = java.util.concurrent.ForkJoinPool.defaultForkJoinWorkerThreadFactory.newThread(pool);
            thread.setName("Worker-" + name + "-" + workers.getAndIncrement());
            return thread;
        }, null, true));
    }
    private static TracingExecutor makeIoExecutor(String prefix, boolean daemon) {
        java.util.concurrent.atomic.AtomicInteger workers = new java.util.concurrent.atomic.AtomicInteger(1);
        return new TracingExecutor(java.util.concurrent.Executors.newCachedThreadPool(runnable -> {
            Thread thread = new Thread(runnable, prefix + workers.getAndIncrement());
            thread.setDaemon(daemon);
            return thread;
        }));
    }
    public static TracingExecutor backgroundExecutor() {
        return kBackgroundExecutor;
    }
    public static long getMillis() {
        return getNanos() / NANOS_PER_MILLI;
    }
    public static final int LINEAR_LOOKUP_THRESHOLD = 8;
    public static final long NANOS_PER_MILLI = 1000000L;
    public static final long MILLIS_PER_SECOND = 1000L;
    public static final Ticker TICKER = Ticker.systemTicker();
    public static final UUID NIL_UUID = new UUID(0L, 0L);
    public static final FileSystemProvider ZIP_FILE_SYSTEM_PROVIDER = FileSystemProvider.installedProviders().stream()
        .filter(provider -> provider.getScheme().equalsIgnoreCase("jar")).findFirst().orElse(null);
    public static final Escaper CONTROL_CHARACTER_ESCAPER = controlEscaper();
    private static Escaper controlEscaper() {
        com.google.common.escape.Escapers.Builder builder = com.google.common.escape.Escapers.builder();
        for (char c = 0; c <= 255; c++) {
            if (!Character.isISOControl(c)) {
                continue;
            }
            String text = switch (c) {
                case 7 -> "\\a";
                case '\b' -> "\\b";
                case '\t' -> "\\t";
                case '\n' -> "\\n";
                case 11 -> "\\v";
                case '\f' -> "\\f";
                case '\r' -> "\\r";
                default -> String.format("\\x%02X", (int) c);
            };
            builder.addEscape(c, text);
        }
        return builder.build();
    }
    public static <K, V> Collector<Map.Entry<? extends K, ? extends V>, ?, Map<K, V>> toMap() {
        return Collectors.toMap(Map.Entry::getKey, Map.Entry::getValue);
    }
    public static <T> Collector<T, ?, List<T>> toMutableList() {
        return java.util.stream.Collectors.toCollection(java.util.ArrayList::new);
    }
    public static <T extends Comparable<T>> String getPropertyName(Property<T> key, Object value) {
        return value instanceof StringRepresentable representable ? representable.getSerializedName() : value.toString();
    }
    public static String makeDescriptionId(String prefix, Identifier location) {
        return location == null ? prefix + ".unregistered_sadface" : prefix + "." + location.getNamespace() + "." + location.getPath().replace('/', '.');
    }
    private static TimeSource.NanoTimeSource kTimeSource = System::nanoTime;
    public static TimeSource.NanoTimeSource timeSource() {
        return kTimeSource;
    }
    public static void setTimeSource(TimeSource.NanoTimeSource timeSource) {
        kTimeSource = timeSource;
    }
    public static void shutdownTimeSource() {
        kTimeSource = System::nanoTime;
    }
    public static long getNanos() {
        return System.nanoTime();
    }
    public static long getEpochMillis() {
        return System.currentTimeMillis();
    }
    public static long toMillis(double seconds) {
        return (long) (seconds * 1000.0);
    }
    public static String getFilenameFormattedDateTime() {
        return java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd_HH.mm.ss").format(java.time.LocalDateTime.now());
    }
    public static int maxAllowedExecutorThreads() {
        return Mth.clamp(Runtime.getRuntime().availableProcessors() - 1, 1, 255);
    }
    public static TracingExecutor ioPool() {
        return kIoPool;
    }
    public static TracingExecutor nonCriticalIoPool() {
        return kDownloadPool;
    }
    public static void shutdownExecutors() {
        kBackgroundExecutor.shutdownAndAwait(3L, java.util.concurrent.TimeUnit.SECONDS);
        kIoPool.shutdownAndAwait(3L, java.util.concurrent.TimeUnit.SECONDS);
    }
    public static void throwAsRuntime(Throwable throwable) {
        if (throwable instanceof RuntimeException runtime) {
            throw runtime;
        }
        throw new RuntimeException(throwable);
    }
    public static Type<?> fetchChoiceType(DSL.TypeReference reference, String name) {
        return net.minecraft.util.datafix.DataFixers.getDataFixer().getSchema(com.mojang.datafixers.DataFixUtils.makeKey(org.basaltmc.bukkit.WorldNatives.dataVersion())).getChoiceType(reference, name);
    }
    public static void runNamed(Runnable runnable, String name) {
        Thread thread = Thread.currentThread();
        String previous = thread.getName();
        thread.setName(name);
        try {
            runnable.run();
        } finally {
            thread.setName(previous);
        }
    }
    public static <T> String getRegisteredName(Registry<T> registry, T entry) {
        Identifier key = registry.getKey(entry);
        return key == null ? "[unregistered]" : key.toString();
    }
    public static <T> Predicate<T> allOf() {
        return value -> true;
    }
    public static <T> Predicate<T> allOf(Predicate<? super T> condition) {
        return condition::test;
    }
    public static <T> Predicate<T> allOf(Predicate<? super T> condition1, Predicate<? super T> condition2) {
        return value -> condition1.test(value) && condition2.test(value);
    }
    public static <T> Predicate<T> allOf(Predicate<? super T> condition1, Predicate<? super T> condition2, Predicate<? super T> condition3) {
        return value -> condition1.test(value) && condition2.test(value) && condition3.test(value);
    }
    public static <T> Predicate<T> allOf(Predicate<? super T> condition1, Predicate<? super T> condition2, Predicate<? super T> condition3, Predicate<? super T> condition4) {
        return value -> condition1.test(value) && condition2.test(value) && condition3.test(value) && condition4.test(value);
    }
    public static <T> Predicate<T> allOf(Predicate<? super T> condition1, Predicate<? super T> condition2, Predicate<? super T> condition3, Predicate<? super T> condition4, Predicate<? super T> condition5) {
        return value -> condition1.test(value) && condition2.test(value) && condition3.test(value) && condition4.test(value) && condition5.test(value);
    }
    public static <T> Predicate<T> allOf(Predicate<? super T>... conditions) {
        return value -> {
            for (Predicate<? super T> condition : conditions) {
        if (!condition.test(value)) {
            return false;
        }
            }
            return true;
        };
    }
    public static <T> Predicate<T> allOf(List<? extends Predicate<? super T>> conditions) {
        return value -> {
            for (Predicate<? super T> condition : conditions) {
        if (!condition.test(value)) {
            return false;
        }
            }
            return true;
        };
    }
    public static <T> Predicate<T> anyOf() {
        return value -> false;
    }
    public static <T> Predicate<T> anyOf(Predicate<? super T> condition1) {
        return condition1::test;
    }
    public static <T> Predicate<T> anyOf(Predicate<? super T> condition1, Predicate<? super T> condition2) {
        return value -> condition1.test(value) || condition2.test(value);
    }
    public static <T> Predicate<T> anyOf(Predicate<? super T> condition1, Predicate<? super T> condition2, Predicate<? super T> condition3) {
        return value -> condition1.test(value) || condition2.test(value) || condition3.test(value);
    }
    public static <T> Predicate<T> anyOf(Predicate<? super T> condition1, Predicate<? super T> condition2, Predicate<? super T> condition3, Predicate<? super T> condition4) {
        return value -> condition1.test(value) || condition2.test(value) || condition3.test(value) || condition4.test(value);
    }
    public static <T> Predicate<T> anyOf(Predicate<? super T> condition1, Predicate<? super T> condition2, Predicate<? super T> condition3, Predicate<? super T> condition4, Predicate<? super T> condition5) {
        return value -> condition1.test(value) || condition2.test(value) || condition3.test(value) || condition4.test(value) || condition5.test(value);
    }
    public static <T> Predicate<T> anyOf(Predicate<? super T>... conditions) {
        return value -> {
            for (Predicate<? super T> condition : conditions) {
        if (condition.test(value)) {
            return true;
        }
            }
            return false;
        };
    }
    public static <T> Predicate<T> anyOf(List<? extends Predicate<? super T>> conditions) {
        return value -> {
            for (Predicate<? super T> condition : conditions) {
        if (condition.test(value)) {
            return true;
        }
            }
            return false;
        };
    }
    public static <T> boolean isSymmetrical(int width, int height, List<T> ingredients) {
        if (width == 1) {
            return true;
        }
        int middle = width / 2;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < middle; x++) {
        if (!java.util.Objects.equals(ingredients.get(x + y * width), ingredients.get(width - 1 - x + y * width))) {
            return false;
        }
            }
        }
        return true;
    }
    public static int growByHalf(int currentSize, int minimalNewSize) {
        return (int) Math.max(Math.min((long) currentSize + (currentSize >> 1), 2147483639L), minimalNewSize);
    }
    public static DateTimeFormatter localizedDateFormatter(FormatStyle formatStyle) {
        return DateTimeFormatter.ofLocalizedDateTime(formatStyle).withZone(java.time.ZoneId.systemDefault());
    }
    public static ThreadInfo[] dumpThreadInfo() {
        return java.lang.management.ManagementFactory.getThreadMXBean().dumpAllThreads(true, true);
    }
    public static OS getPlatform() {
        String name = System.getProperty("os.name", "").toLowerCase(java.util.Locale.ROOT);
        if (name.contains("win")) {
            return OS.WINDOWS;
        }
        if (name.contains("mac")) {
            return OS.OSX;
        }
        if (name.contains("solaris") || name.contains("sunos")) {
            return OS.SOLARIS;
        }
        return name.contains("linux") || name.contains("unix") ? OS.LINUX : OS.UNKNOWN;
    }
    public static boolean isAarch64() {
        String arch = System.getProperty("os.arch", "").toLowerCase(java.util.Locale.ROOT);
        return arch.equals("aarch64");
    }
    public static boolean isAppleSiliconMac(String renderer) {
        return getPlatform() == OS.OSX && renderer.toLowerCase(java.util.Locale.ROOT).contains("apple");
    }
    public static URI parseAndValidateUntrustedUri(String uri) throws URISyntaxException {
        URI parsed = new URI(uri);
        String scheme = parsed.getScheme();
        if (scheme == null) {
            throw new URISyntaxException(uri, "Missing protocol in URI: " + uri);
        }
        String lower = scheme.toLowerCase(java.util.Locale.ROOT);
        if (!lower.equals("http") && !lower.equals("https")) {
            throw new URISyntaxException(uri, "Unsupported protocol in URI: " + uri);
        }
        return parsed;
    }
    public static <T> T findNextInIterable(Iterable<T> collection, T current) {
        java.util.Iterator<T> it = collection.iterator();
        T first = it.next();
        if (current != null) {
            T seen = first;
            while (true) {
                if (seen == current) {
                    return it.hasNext() ? it.next() : first;
                }
                if (it.hasNext()) {
                    seen = it.next();
                }
                else {
                    break;
                }
            }
        }
        return first;
    }
    public static <T> T findPreviousInIterable(Iterable<T> collection, T current) {
        Iterator<T> iterator = collection.iterator();
        T previous = null;
        while (iterator.hasNext()) {
            T next = iterator.next();
            if (next == current) {
        if (previous == null) {
            previous = iterator.hasNext() ? Iterators.getLast(iterator) : current;
        }
        break;
            }
            previous = next;
        }
        return previous;
    }
    public static <T> T make(Supplier<T> factory) {
        return factory.get();
    }
    public static <T> T make(T t, Consumer<? super T> consumer) {
        consumer.accept(t);
        return t;
    }
    public static <K extends Enum<K>, V> Map<K, V> makeEnumMap(Class<K> keyType, Function<K, V> function) {
        java.util.EnumMap<K, V> map = new java.util.EnumMap<>(keyType);
        for (K key : keyType.getEnumConstants()) {
            map.put(key, function.apply(key));
        }
        return map;
    }
    public static <K, V1, V2> Map<K, V2> mapValues(Map<K, V1> map, Function<? super V1, V2> valueMapper) {
        Map<K, V2> result = new java.util.HashMap<>();
        map.forEach((key, value) -> result.put(key, valueMapper.apply(value)));
        return result;
    }
    public static <K, V1, V2> Map<K, V2> mapValuesLazy(Map<K, V1> map, com.google.common.base.Function<V1, V2> valueMapper) {
        return com.google.common.collect.Maps.transformValues(map, valueMapper);
    }
    public static <T extends Enum<T>> Set<T> allOfEnumExcept(T value) {
        return java.util.EnumSet.complementOf(java.util.EnumSet.of(value));
    }
    public static <V extends Object> CompletableFuture<List<V>> sequence(List<? extends CompletableFuture<V>> futures) {
        return sequenceFailFast(futures).thenApply(list -> (List<V>) list);
    }
    public static <V extends Object> CompletableFuture<List<V>> sequenceFailFast(List<? extends CompletableFuture<? extends V>> futures) {
        CompletableFuture<List<V>> result = new CompletableFuture<>();
        List<V> values = new java.util.ArrayList<>(java.util.Collections.nCopies(futures.size(), null));
        java.util.concurrent.atomic.AtomicInteger remaining = new java.util.concurrent.atomic.AtomicInteger(futures.size());
        if (futures.isEmpty()) {
            result.complete(values);
            return result;
        }
        for (int i = 0; i < futures.size(); i++) {
            int index = i;
            futures.get(i).whenComplete((value, error) -> {
        if (error != null) {
            result.completeExceptionally(error);
            return;
        }
        synchronized (values) {
            values.set(index, value);
        }
        if (remaining.decrementAndGet() == 0) {
            result.complete(values);
        }
            });
        }
        return result;
    }
    public static <V extends Object> CompletableFuture<List<V>> sequenceFailFastAndCancel(List<? extends CompletableFuture<? extends V>> futures) {
        CompletableFuture<List<V>> result = sequenceFailFast(futures);
        result.exceptionally(error -> {
            for (CompletableFuture<? extends V> future : futures) {
        future.cancel(true);
            }
            return null;
        });
        return result;
    }
    public static <T> Optional<T> ifElse(Optional<T> input, Consumer<T> onTrue, Runnable onFalse) {
        input.ifPresentOrElse(onTrue, onFalse);
        return input;
    }
    public static <T> Supplier<T> name(Supplier<T> task, Supplier<String> nameGetter) {
        return () -> {
            Thread thread = Thread.currentThread();
            String previous = thread.getName();
            thread.setName(nameGetter.get());
            try {
        return task.get();
            } finally {
        thread.setName(previous);
            }
        };
    }
    public static Runnable name(Runnable task, Supplier<String> nameGetter) {
        return () -> {
            Thread thread = Thread.currentThread();
            String previous = thread.getName();
            thread.setName(nameGetter.get());
            try {
        task.run();
            } finally {
        thread.setName(previous);
            }
        };
    }
    public static void logAndPauseIfInIde(String message) {
        org.slf4j.LoggerFactory.getLogger(Util.class).error(message);
    }
    public static void logAndPauseIfInIde(String message, Throwable throwable) {
        org.slf4j.LoggerFactory.getLogger(Util.class).error(message, throwable);
    }
    public static <T extends Throwable> T pauseInIde(T t) {
        return t;
    }
    public static void setPause(Consumer<String> pauseFunction) {
    }
    public static String describeError(Throwable err) {
        if (err.getCause() != null) {
            return describeError(err.getCause());
        }
        return err.getMessage() != null ? err.getMessage() : err.toString();
    }
    public static <T> T getRandom(T[] array, RandomSource random) {
        return array[random.nextInt(array.length)];
    }
    public static int getRandom(int[] array, RandomSource random) {
        return array[random.nextInt(array.length)];
    }
    public static <T> T getRandom(List<T> list, RandomSource random) {
        return list.get(random.nextInt(list.size()));
    }
    public static <T> Optional<T> getRandomSafe(List<T> list, RandomSource random) {
        return list.isEmpty() ? Optional.empty() : Optional.of(getRandom(list, random));
    }
    public static boolean safeMoveFile(Path fromPath, Path toPath, CopyOption... options) {
        try {
            java.nio.file.Files.move(fromPath, toPath, options);
            return true;
        } catch (IOException e) {
            return false;
        }
    }
    public static void safeReplaceFile(Path targetPath, Path newPath, Path backupPath) {
        safeReplaceOrMoveFile(targetPath, newPath, backupPath, false);
    }
    public static boolean safeReplaceOrMoveFile(Path targetPath, Path newPath, Path backupPath, boolean noRollback) {
        try {
            if (java.nio.file.Files.exists(targetPath)) {
        java.nio.file.Files.move(targetPath, backupPath, java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            }
            java.nio.file.Files.move(newPath, targetPath);
            return true;
        } catch (IOException e) {
            if (!noRollback && java.nio.file.Files.exists(backupPath) && !java.nio.file.Files.exists(targetPath)) {
        try {
            java.nio.file.Files.move(backupPath, targetPath);
        } catch (IOException ignored) {
            // keep the failure
        }
            }
            return false;
        }
    }
    public static int offsetByCodepoints(String input, int pos, int offset) {
        int length = input.length();
        int result = pos;
        if (offset >= 0) {
            for (int i = 0; result < length && i < offset; i++) {
        if (Character.isHighSurrogate(input.charAt(result++)) && result < length && Character.isLowSurrogate(input.charAt(result))) {
            result++;
        }
            }
        } else {
            for (int i = offset; result > 0 && i < 0; i++) {
        result--;
        if (Character.isLowSurrogate(input.charAt(result)) && result > 0 && Character.isHighSurrogate(input.charAt(result - 1))) {
            result--;
        }
            }
        }
        return result;
    }
    public static Consumer<String> prefix(String prefix, Consumer<String> consumer) {
        return line -> consumer.accept(prefix + line);
    }
    public static DataResult<int[]> fixedSize(IntStream stream, int size) {
        int[] taken = stream.limit(size + 1L).toArray();
        if (taken.length != size) {
            return DataResult.error(() -> "Input is not a list of " + size + " elements");
        }
        return DataResult.success(taken);
    }
    public static DataResult<long[]> fixedSize(LongStream stream, int size) {
        long[] taken = stream.limit(size + 1L).toArray();
        if (taken.length != size) {
            return DataResult.error(() -> "Input is not a list of " + size + " elements");
        }
        return DataResult.success(taken);
    }
    public static <T> DataResult<List<T>> fixedSize(List<T> list, int size) {
        if (list.size() == size) {
            return DataResult.success(list);
        }
        String message = "Input is not a list of " + size + " elements";
        return list.size() < size ? DataResult.error(() -> message, list) : DataResult.error(() -> message, list.subList(0, size));
    }
    public static void startTimerHackThread() {
    }
    public static void copyBetweenDirs(Path sourceDir, Path targetDir, Path sourcePath) throws IOException {
        Path relative = sourceDir.relativize(sourcePath);
        Path target = targetDir.resolve(relative.toString());
        java.nio.file.Files.copy(sourcePath, target, java.nio.file.StandardCopyOption.REPLACE_EXISTING);
    }
    public static String sanitizeName(String value, CharPredicate isAllowedChar) {
        StringBuilder result = new StringBuilder();
        value.codePoints().forEach(codePoint -> result.append(isAllowedChar.test((char) codePoint) ? String.valueOf(Character.toChars(codePoint)) : "_"));
        return result.toString();
    }
    public static <K, V> SingleKeyCache<K, V> singleKeyCache(Function<K, V> computeValueFunction) {
        return new SingleKeyCache<>(computeValueFunction);
    }
    public static <T, R> Function<T, R> memoize(Function<T, R> function) {
        java.util.concurrent.ConcurrentHashMap<T, R> cache = new java.util.concurrent.ConcurrentHashMap<>();
        return key -> cache.computeIfAbsent(key, function);
    }
    public static <T, U, R> BiFunction<T, U, R> memoize(BiFunction<T, U, R> function) {
        Map<java.util.AbstractMap.SimpleEntry<T, U>, R> cache = new java.util.concurrent.ConcurrentHashMap<>();
        return (a, b) -> cache.computeIfAbsent(new java.util.AbstractMap.SimpleEntry<>(a, b), key -> function.apply(key.getKey(), key.getValue()));
    }
    public static <T> List<T> toShuffledList(Stream<T> stream, RandomSource random) {
        ObjectArrayList<T> collected = stream.collect(ObjectArrayList.toList());
        shuffle(collected, random);
        return collected;
    }
    public static IntArrayList toShuffledList(IntStream stream, RandomSource random) {
        IntArrayList list = new IntArrayList(stream.toArray());
        int size = list.size();
        for (int i = size; i > 1; i--) {
            int swap = random.nextInt(i);
            int tmp = list.getInt(i - 1);
            list.set(i - 1, list.getInt(swap));
            list.set(swap, tmp);
        }
        return list;
    }
    public static <T> List<T> shuffledCopy(T[] array, RandomSource random) {
        java.util.ArrayList<T> copy = new java.util.ArrayList<>(java.util.Arrays.asList(array));
        shuffle(copy, random);
        return copy;
    }
    public static <T> List<T> shuffledCopy(ObjectArrayList<T> list, RandomSource random) {
        ObjectArrayList<T> copy = new ObjectArrayList<>(list);
        shuffle(copy, random);
        return copy;
    }
    public static <T> void shuffle(List<T> list, RandomSource random) {
        for (int i = list.size(); i > 1; i--) {
            int swapWith = random.nextInt(i);
            list.set(i - 1, list.set(swapWith, list.get(i - 1)));
        }
    }
    public static <T> CompletableFuture<T> blockUntilDone(Function<Executor, CompletableFuture<T>> task) {
        return task.apply(Runnable::run);
    }
    public static <T> T blockUntilDone(Function<Executor, T> task, Predicate<T> completionCheck) {
        java.util.concurrent.LinkedBlockingQueue<Runnable> queue = new java.util.concurrent.LinkedBlockingQueue<>();
        T result = task.apply(queue::add);
        while (!completionCheck.test(result)) {
            try {
        Runnable next = queue.poll(100, java.util.concurrent.TimeUnit.MILLISECONDS);
        if (next != null) {
            next.run();
        }
            } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
        break;
            }
        }
        return result;
    }
    public static <T> ToIntFunction<T> createIndexLookup(List<T> values) {
        int size = values.size();
        if (size < LINEAR_LOOKUP_THRESHOLD) {
            return values::indexOf;
        }
        Map<T, Integer> map = new java.util.HashMap<>(size);
        for (int i = 0; i < size; i++) {
            map.put(values.get(i), i);
        }
        return value -> map.getOrDefault(value, -1);
    }
    public static <T> ToIntFunction<T> createIndexIdentityLookup(List<T> values) {
        Map<T, Integer> map = new java.util.IdentityHashMap<>(values.size());
        for (int i = 0; i < values.size(); i++) {
            map.put(values.get(i), i);
        }
        return value -> map.getOrDefault(value, -1);
    }
    public static <A, B> Typed<B> writeAndReadTypedOrThrow(Typed<A> typed, Type<B> newType, UnaryOperator<Dynamic<?>> function) {
        Dynamic<?> written = typed.write().getOrThrow(IllegalStateException::new);
        return readTypedOrThrow(newType, function.apply(written), true);
    }
    public static <T> Typed<T> readTypedOrThrow(Type<T> type, Dynamic<?> dynamic) {
        return readTypedOrThrow(type, dynamic, false);
    }
    public static <T> Typed<T> readTypedOrThrow(Type<T> type, Dynamic<?> dynamic, boolean acceptPartial) {
        com.mojang.serialization.DataResult<? extends com.mojang.datafixers.util.Pair<Typed<T>, ?>> result = type.readTyped(dynamic);
        return result.getOrThrow(IllegalStateException::new).getFirst();
    }
    public static <T> List<T> copyAndAdd(List<T> list, T element) {
        java.util.ArrayList<T> copy = new java.util.ArrayList<>(list);
        copy.add(element);
        return copy;
    }
    public static <T> List<T> copyAndAdd(List<T> list, T... elements) {
        java.util.ArrayList<T> copy = new java.util.ArrayList<>(list);
        java.util.Collections.addAll(copy, elements);
        return copy;
    }
    public static <T> List<T> copyAndAdd(T element, List<T> list) {
        java.util.ArrayList<T> copy = new java.util.ArrayList<>(list.size() + 1);
        copy.add(element);
        copy.addAll(list);
        return copy;
    }
    public static <T> List<T> join(Collection<T> first, Collection<T> second) {
        java.util.ArrayList<T> joined = new java.util.ArrayList<>(first.size() + second.size());
        joined.addAll(first);
        joined.addAll(second);
        return joined;
    }
    public static <T> List<T> join(Collection<T>... collections) {
        List<T> result = new java.util.ArrayList<>();
        for (Collection<T> collection : collections) {
            result.addAll(collection);
        }
        return result;
    }
    public static <K, V> Map<K, V> copyAndPut(Map<K, V> map, K key, V value) {
        Map<K, V> copy = new java.util.HashMap<>(map);
        copy.put(key, value);
        return copy;
    }
    public static enum OS {
        LINUX, SOLARIS, WINDOWS, OSX, UNKNOWN;
        public String telemetryName() {
        return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
