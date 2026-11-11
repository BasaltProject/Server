// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;
import java.util.Collection;
import java.util.Objects;
import java.util.function.Function;
import java.util.function.Supplier;
public class Optionull {
    public static <T> T orElse(T t, T defaultValue) {
        return Objects.requireNonNullElse(t, defaultValue);
    }
    public static <T, R> R map(T t, Function<T, R> map) {
        return t == null ? null : map.apply(t);
    }
    public static <T, R> R mapOrDefault(T t, Function<T, R> map, R defaultValue) {
        return t == null ? defaultValue : map.apply(t);
    }
    public static <T, R> R mapOrElse(T t, Function<T, R> map, Supplier<R> elseSupplier) {
        return t == null ? elseSupplier.get() : map.apply(t);
    }
    public static <T> T first(Collection<T> collection) {
        return collection.isEmpty() ? null : collection.iterator().next();
    }
    public static <T> T firstOrDefault(Collection<T> collection, T defaultValue) {
        return collection.isEmpty() ? defaultValue : collection.iterator().next();
    }
    public static <T> T firstOrElse(Collection<T> collection, Supplier<T> elseSupplier) {
        return collection.isEmpty() ? elseSupplier.get() : collection.iterator().next();
    }
    public static <T> boolean isNullOrEmpty(T[] t) {
        return t == null || t.length == 0;
    }
    public static boolean isNullOrEmpty(boolean[] t) {
        return t == null || t.length == 0;
    }
    public static boolean isNullOrEmpty(byte[] t) {
        return t == null || t.length == 0;
    }
    public static boolean isNullOrEmpty(char[] t) {
        return t == null || t.length == 0;
    }
    public static boolean isNullOrEmpty(short[] t) {
        return t == null || t.length == 0;
    }
    public static boolean isNullOrEmpty(int[] t) {
        return t == null || t.length == 0;
    }
    public static boolean isNullOrEmpty(long[] t) {
        return t == null || t.length == 0;
    }
    public static boolean isNullOrEmpty(float[] t) {
        return t == null || t.length == 0;
    }
    public static boolean isNullOrEmpty(double[] t) {
        return t == null || t.length == 0;
    }
}
