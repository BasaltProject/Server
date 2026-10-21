package org.basaltmc.bukkit;

import org.basaltmc.bukkit.gen.UnsafeValuesBase;

public final class BasaltUnsafe extends UnsafeValuesBase {
    static final BasaltUnsafe INSTANCE = new BasaltUnsafe();

    private static final int kDataVersion = 5023;

    @Override
    public int getDataVersion() {
        return kDataVersion;
    }

    @Override
    public boolean isSupportedApiVersion(String apiVersion) {
        return true;
    }

    @Override
    public String getMainLevelName() {
        return "world";
    }
}
