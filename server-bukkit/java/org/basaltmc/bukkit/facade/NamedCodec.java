package org.basaltmc.bukkit.facade;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;

/** A codec found by class and field name on first use, so a registry list can name classes it never loads up front. */
public final class NamedCodec<A> implements Codec<A> {
    private final String owner;
    private final String field;
    private volatile Codec<A> resolved;

    public NamedCodec(String owner, String field) {
        this.owner = owner;
        this.field = field;
    }

    @SuppressWarnings("unchecked")
    private Codec<A> codec() {
        Codec<A> found = resolved;
        if (found == null) {
            try {
                found = (Codec<A>) Class.forName(owner).getDeclaredField(field).get(null);
            } catch (ReflectiveOperationException | LinkageError failure) {
                return new Codec<A>() {
                    public <T> DataResult<Pair<A, T>> decode(DynamicOps<T> ops, T input) {
                        return DataResult.error(() -> owner + "." + field + " is not available");
                    }

                    public <T> DataResult<T> encode(A input, DynamicOps<T> ops, T prefix) {
                        return DataResult.error(() -> owner + "." + field + " is not available");
                    }
                };
            }
            resolved = found;
        }
        return found;
    }

    @Override
    public <T> DataResult<Pair<A, T>> decode(DynamicOps<T> ops, T input) {
        return codec().decode(ops, input);
    }

    @Override
    public <T> DataResult<T> encode(A input, DynamicOps<T> ops, T prefix) {
        return codec().encode(input, ops, prefix);
    }
}
