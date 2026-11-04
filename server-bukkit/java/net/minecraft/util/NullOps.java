// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
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
public class NullOps implements DynamicOps<Unit> {
    public static final NullOps INSTANCE = new NullOps();
    private NullOps() {
    }
    public <U> U convertTo(DynamicOps<U> outOps, Unit input) {
        return outOps.empty();
    }
    public Unit empty() {
        return Unit.INSTANCE;
    }
    public Unit emptyMap() {
        return Unit.INSTANCE;
    }
    public Unit emptyList() {
        return Unit.INSTANCE;
    }
    public Unit createNumeric(Number value) {
        return Unit.INSTANCE;
    }
    public Unit createByte(byte value) {
        return Unit.INSTANCE;
    }
    public Unit createShort(short value) {
        return Unit.INSTANCE;
    }
    public Unit createInt(int value) {
        return Unit.INSTANCE;
    }
    public Unit createLong(long value) {
        return Unit.INSTANCE;
    }
    public Unit createFloat(float value) {
        return Unit.INSTANCE;
    }
    public Unit createDouble(double value) {
        return Unit.INSTANCE;
    }
    public Unit createBoolean(boolean value) {
        return Unit.INSTANCE;
    }
    public Unit createString(String value) {
        return Unit.INSTANCE;
    }
    public DataResult<Number> getNumberValue(Unit input) {
        return DataResult.error(() -> "Not a number");
    }
    public DataResult<Boolean> getBooleanValue(Unit input) {
        return DataResult.error(() -> "Not a boolean");
    }
    public DataResult<String> getStringValue(Unit input) {
        return DataResult.error(() -> "Not a string");
    }
    public DataResult<Unit> mergeToList(Unit input, Unit value) {
        return DataResult.success(Unit.INSTANCE);
    }
    public DataResult<Unit> mergeToList(Unit input, List<Unit> values) {
        return DataResult.success(Unit.INSTANCE);
    }
    public DataResult<Unit> mergeToMap(Unit input, Unit key, Unit value) {
        return DataResult.success(Unit.INSTANCE);
    }
    public DataResult<Unit> mergeToMap(Unit input, Map<Unit, Unit> values) {
        return DataResult.success(Unit.INSTANCE);
    }
    public DataResult<Unit> mergeToMap(Unit input, MapLike<Unit> values) {
        return DataResult.success(Unit.INSTANCE);
    }
    public DataResult<Stream<Pair<Unit, Unit>>> getMapValues(Unit input) {
        return DataResult.error(() -> "Not a map");
    }
    public DataResult<Consumer<BiConsumer<Unit, Unit>>> getMapEntries(Unit input) {
        return DataResult.error(() -> "Not a map");
    }
    public DataResult<MapLike<Unit>> getMap(Unit input) {
        return DataResult.error(() -> "Not a map");
    }
    public DataResult<Stream<Unit>> getStream(Unit input) {
        return DataResult.error(() -> "Not a list");
    }
    public DataResult<Consumer<Consumer<Unit>>> getList(Unit input) {
        return DataResult.error(() -> "Not a list");
    }
    public DataResult<ByteBuffer> getByteBuffer(Unit input) {
        return DataResult.error(() -> "Not a byte list");
    }
    public DataResult<IntStream> getIntStream(Unit input) {
        return DataResult.error(() -> "Not an int list");
    }
    public DataResult<LongStream> getLongStream(Unit input) {
        return DataResult.error(() -> "Not a long list");
    }
    public Unit createMap(Stream<Pair<Unit, Unit>> map) {
        return Unit.INSTANCE;
    }
    public Unit createMap(Map<Unit, Unit> map) {
        return Unit.INSTANCE;
    }
    public Unit createList(Stream<Unit> input) {
        return Unit.INSTANCE;
    }
    public Unit createByteList(ByteBuffer input) {
        return Unit.INSTANCE;
    }
    public Unit createIntList(IntStream input) {
        return Unit.INSTANCE;
    }
    public Unit createLongList(LongStream input) {
        return Unit.INSTANCE;
    }
    public Unit remove(Unit input, String key) {
        return input;
    }
    public RecordBuilder<Unit> mapBuilder() {
        return new RecordBuilder.MapBuilder<>(this);
    }
    public ListBuilder<Unit> listBuilder() {
        return new ListBuilder.Builder<>(this);
    }
    public String toString() {
        return "Null";
    }
}
