// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.serialization.DynamicOps;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.stream.IntStream;
import java.util.stream.Stream;
import net.minecraft.core.UUIDUtil;
import net.minecraft.network.chat.Component;
import net.minecraft.util.parsing.packrat.DelayedException;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.SuggestionSupplier;
public class SnbtOperations {
    private static final DelayedException<?> BAD_UUID = DelayedException.create(new SimpleCommandExceptionType(Component.translatable("snbt.parser.expected_string_uuid")));
    private static final DelayedException<?> BAD_FLAG = DelayedException.create(new SimpleCommandExceptionType(Component.translatable("snbt.parser.expected_number_or_boolean")));
    public static final String BUILTIN_TRUE = "true";
    public static final String BUILTIN_FALSE = "false";
    public static final Map<BuiltinKey, BuiltinOperation> BUILTIN_OPERATIONS = Map.of(new BuiltinKey("bool", 1), new BuiltinOperation() {
        public <T> T run(DynamicOps<T> ops, List<T> arguments, ParseState<StringReader> state) {
            T arg = arguments.get(0);
            Optional<Boolean> flag = ops.getBooleanValue(arg).result();
            if (flag.isEmpty()) {
                Optional<Number> number = ops.getNumberValue(arg).result();
                if (number.isEmpty()) {
                    state.errorCollector().store(state.mark(), BAD_FLAG);
                    return null;
                }
                return ops.createBoolean(number.get().doubleValue() != 0.0);
            }
            return ops.createBoolean(flag.get());
        }
    }, new BuiltinKey("uuid", 1), new BuiltinOperation() {
        public <T> T run(DynamicOps<T> ops, List<T> arguments, ParseState<StringReader> state) {
            Optional<String> text = ops.getStringValue(arguments.get(0)).result();
            try {
                if (text.isPresent()) {
                    return ops.createIntList(IntStream.of(UUIDUtil.uuidToIntArray(UUID.fromString(text.get()))));
                }
            } catch (IllegalArgumentException ignored) {
            }
            state.errorCollector().store(state.mark(), BAD_UUID);
            return null;
        }
    });
    public static final SuggestionSupplier<StringReader> BUILTIN_IDS = state -> Stream.concat(Stream.of("false", "true"),
        BUILTIN_OPERATIONS.keySet().stream().map(BuiltinKey::id)).distinct();
    public static record BuiltinKey(String id, int argCount) {
        public String toString() {
            return id + "/" + argCount;
        }
    }
    public interface BuiltinOperation {
        <T> T run(DynamicOps<T> ops, List<T> arguments, ParseState<StringReader> state);
    }
}
