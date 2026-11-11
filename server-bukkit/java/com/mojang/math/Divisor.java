// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package com.mojang.math;
import it.unimi.dsi.fastutil.ints.IntIterator;
import java.util.NoSuchElementException;
public class Divisor implements IntIterator {
    private final int parts;
    private final int base;
    private final int extra;
    private int emitted;
    private int carry;
    public Divisor(int numerator, int denominator) {
        parts = denominator;
        base = denominator > 0 ? numerator / denominator : 0;
        extra = denominator > 0 ? numerator % denominator : 0;
    }
    public boolean hasNext() {
        return emitted < parts;
    }
    public int nextInt() {
        if (!hasNext()) {
            throw new NoSuchElementException();
        }
        int value = base;
        carry += extra;
        if (carry >= parts) {
            carry -= parts;
            value++;
        }
        emitted++;
        return value;
    }
    public static Iterable<Integer> asIterable(int numerator, int denominator) {
        return () -> new Divisor(numerator, denominator);
    }
}
