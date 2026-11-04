// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.ListBuilder;
import com.mojang.serialization.MapLike;
import com.mojang.serialization.RecordBuilder;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.Map;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.stream.IntStream;
import java.util.stream.LongStream;
import java.util.stream.Stream;
import com.mojang.serialization.Encoder;
import com.mojang.serialization.Lifecycle;
import java.util.function.UnaryOperator;

public abstract class DelegatingOps<T> implements DynamicOps<T> {
    protected final DynamicOps<T> delegate;

    protected DelegatingOps(DynamicOps<T> delegate) {
        this.delegate = delegate;
    }

    @Override
    public T empty() {
        return delegate.empty();
    }

    @Override
    public T emptyMap() {
        return delegate.emptyMap();
    }

    @Override
    public T emptyList() {
        return delegate.emptyList();
    }

    @Override
    public <U> U convertTo(DynamicOps<U> outOps, T input) {
        return delegate.convertTo(outOps, input);
    }

    @Override
    public DataResult<Number> getNumberValue(T input) {
        return delegate.getNumberValue(input);
    }

    @Override
    public Number getNumberValue(T input, Number defaultValue) {
        return delegate.getNumberValue(input, defaultValue);
    }

    @Override
    public T createNumeric(Number i) {
        return delegate.createNumeric(i);
    }

    @Override
    public T createByte(byte value) {
        return delegate.createByte(value);
    }

    @Override
    public T createShort(short value) {
        return delegate.createShort(value);
    }

    @Override
    public T createInt(int value) {
        return delegate.createInt(value);
    }

    @Override
    public T createLong(long value) {
        return delegate.createLong(value);
    }

    @Override
    public T createFloat(float value) {
        return delegate.createFloat(value);
    }

    @Override
    public T createDouble(double value) {
        return delegate.createDouble(value);
    }

    @Override
    public DataResult<Boolean> getBooleanValue(T input) {
        return delegate.getBooleanValue(input);
    }

    @Override
    public T createBoolean(boolean value) {
        return delegate.createBoolean(value);
    }

    @Override
    public DataResult<String> getStringValue(T input) {
        return delegate.getStringValue(input);
    }

    @Override
    public T createString(String value) {
        return delegate.createString(value);
    }

    @Override
    public DataResult<T> mergeToList(T list, T value) {
        return delegate.mergeToList(list, value);
    }

    @Override
    public DataResult<T> mergeToList(T list, List<T> values) {
        return delegate.mergeToList(list, values);
    }

    @Override
    public DataResult<T> mergeToMap(T map, T key, T value) {
        return delegate.mergeToMap(map, key, value);
    }

    @Override
    public DataResult<T> mergeToMap(T map, Map<T, T> values) {
        return delegate.mergeToMap(map, values);
    }

    @Override
    public DataResult<T> mergeToMap(T map, MapLike<T> values) {
        return delegate.mergeToMap(map, values);
    }

    @Override
    public DataResult<T> mergeToPrimitive(T prefix, T value) {
        return delegate.mergeToPrimitive(prefix, value);
    }

    @Override
    public DataResult<Stream<Pair<T, T>>> getMapValues(T input) {
        return delegate.getMapValues(input);
    }

    @Override
    public DataResult<Consumer<BiConsumer<T, T>>> getMapEntries(T input) {
        return delegate.getMapEntries(input);
    }

    @Override
    public T createMap(Stream<Pair<T, T>> map) {
        return delegate.createMap(map);
    }

    @Override
    public T createMap(Map<T, T> map) {
        return delegate.createMap(map);
    }

    @Override
    public DataResult<MapLike<T>> getMap(T input) {
        return delegate.getMap(input);
    }

    @Override
    public DataResult<Stream<T>> getStream(T input) {
        return delegate.getStream(input);
    }

    @Override
    public DataResult<Consumer<Consumer<T>>> getList(T input) {
        return delegate.getList(input);
    }

    @Override
    public T createList(Stream<T> input) {
        return delegate.createList(input);
    }

    @Override
    public DataResult<ByteBuffer> getByteBuffer(T input) {
        return delegate.getByteBuffer(input);
    }

    @Override
    public T createByteList(ByteBuffer input) {
        return delegate.createByteList(input);
    }

    @Override
    public DataResult<IntStream> getIntStream(T input) {
        return delegate.getIntStream(input);
    }

    @Override
    public T createIntList(IntStream input) {
        return delegate.createIntList(input);
    }

    @Override
    public DataResult<LongStream> getLongStream(T input) {
        return delegate.getLongStream(input);
    }

    @Override
    public T createLongList(LongStream input) {
        return delegate.createLongList(input);
    }

    @Override
    public T remove(T input, String key) {
        return delegate.remove(input, key);
    }

    @Override
    public boolean compressMaps() {
        return delegate.compressMaps();
    }

    @Override
    public ListBuilder<T> listBuilder() {
        return delegate.listBuilder();
    }

    @Override
    public RecordBuilder<T> mapBuilder() {
        return delegate.mapBuilder();
    }
    protected class DelegateListBuilder implements ListBuilder<T> {
        protected DelegateListBuilder(ListBuilder<T> original) {
        }
        public DynamicOps<T> ops() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#ops");
        }
        public DataResult<T> build(T prefix) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#build");
        }
        public ListBuilder<T> add(T value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#add");
        }
        public ListBuilder<T> add(DataResult<T> value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#add");
        }
        public <E> ListBuilder<T> add(E value, Encoder<E> encoder) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#add");
        }
        public <E> ListBuilder<T> addAll(Iterable<E> values, Encoder<E> encoder) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#addAll");
        }
        public ListBuilder<T> withErrorsFrom(DataResult<?> result) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#withErrorsFrom");
        }
        public ListBuilder<T> mapError(UnaryOperator<String> onError) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#mapError");
        }
        public DataResult<T> build(DataResult<T> prefix) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateListBuilder#build");
        }
    }
    protected class DelegateRecordBuilder implements RecordBuilder<T> {
        protected DelegateRecordBuilder(RecordBuilder<T> original) {
        }
        public DynamicOps<T> ops() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#ops");
        }
        public RecordBuilder<T> add(T key, T value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#add");
        }
        public RecordBuilder<T> add(T key, DataResult<T> value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#add");
        }
        public RecordBuilder<T> add(DataResult<T> key, DataResult<T> value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#add");
        }
        public RecordBuilder<T> add(String key, T value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#add");
        }
        public RecordBuilder<T> add(String key, DataResult<T> value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#add");
        }
        public <E> RecordBuilder<T> add(String key, E value, Encoder<E> encoder) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#add");
        }
        public RecordBuilder<T> withErrorsFrom(DataResult<?> result) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#withErrorsFrom");
        }
        public RecordBuilder<T> setLifecycle(Lifecycle lifecycle) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#setLifecycle");
        }
        public RecordBuilder<T> mapError(UnaryOperator<String> onError) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#mapError");
        }
        public DataResult<T> build(T prefix) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#build");
        }
        public DataResult<T> build(DataResult<T> prefix) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.resources.DelegatingOps.DelegateRecordBuilder#build");
        }
    }
}
