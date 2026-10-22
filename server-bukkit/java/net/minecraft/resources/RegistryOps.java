package net.minecraft.resources;

import com.mojang.serialization.DynamicOps;
import net.minecraft.core.HolderLookup;

public class RegistryOps<T> extends DelegatingOps<T> {
    private final HolderLookup.Provider lookupProvider;

    protected RegistryOps(DynamicOps<T> delegate, HolderLookup.Provider lookupProvider) {
        super(delegate);
        this.lookupProvider = lookupProvider;
    }

    public static <T> RegistryOps<T> create(DynamicOps<T> ops, HolderLookup.Provider lookupProvider) {
        return new RegistryOps<>(ops, lookupProvider);
    }

    public HolderLookup.Provider lookupProvider() {
        return lookupProvider;
    }
}
