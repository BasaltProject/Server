// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.Lifecycle;
import com.mojang.serialization.ListBuilder;
import java.util.function.UnaryOperator;

public abstract class AbstractListBuilder<T, B> implements ListBuilder<T> {
    private final DynamicOps<T> ops;
    protected DataResult<B> builder = DataResult.success(initBuilder(), Lifecycle.stable());

    protected AbstractListBuilder(DynamicOps<T> ops) {
        this.ops = ops;
    }

    @Override
    public DynamicOps<T> ops() {
        return ops;
    }

    protected abstract B initBuilder();

    protected abstract B append(B builder, T value);

    protected abstract DataResult<T> build(B builder, T prefix);

    @Override
    public ListBuilder<T> add(T value) {
        builder = builder.map(current -> append(current, value));
        return this;
    }

    @Override
    public ListBuilder<T> add(DataResult<T> value) {
        builder = builder.apply2stable(this::append, value);
        return this;
    }

    @Override
    public ListBuilder<T> withErrorsFrom(DataResult<?> result) {
        builder = builder.flatMap(current -> result.map(ignored -> current));
        return this;
    }

    @Override
    public ListBuilder<T> mapError(UnaryOperator<String> onError) {
        builder = builder.mapError(onError);
        return this;
    }

    @Override
    public DataResult<T> build(T prefix) {
        DataResult<T> result = builder.flatMap(current -> build(current, prefix));
        builder = DataResult.success(initBuilder(), Lifecycle.stable());
        return result;
    }
}
