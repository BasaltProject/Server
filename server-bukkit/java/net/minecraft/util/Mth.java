package net.minecraft.util;

public class Mth {
    protected Mth() {
    }

    public static int ceillog2(int input) {
        return input == 0 ? 0 : 32 - Integer.numberOfLeadingZeros(input - 1);
    }
}
