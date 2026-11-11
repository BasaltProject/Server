// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix;
import com.mojang.datafixers.DataFixUtils;
import com.mojang.datafixers.OpticFinder;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.functions.PointFreeRule;
import com.mojang.datafixers.types.Type;
import com.mojang.datafixers.View;
import com.mojang.datafixers.RewriteResult;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
import java.util.BitSet;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.UnaryOperator;
import java.util.stream.IntStream;
import net.minecraft.util.Util;
public class ExtraDataFixUtils {
    private static final String[] kDyes = {"white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
            "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black"};
    private static Optional<Integer> intAt(Dynamic<?> source, String field) {
        return source.get(field).asNumber().result().map(Number::intValue);
    }
    public static Dynamic<?> fixBlockPos(Dynamic<?> pos) {
        Optional<Integer> x = intAt(pos, "X");
        Optional<Integer> y = intAt(pos, "Y");
        Optional<Integer> z = intAt(pos, "Z");
        if (x.isEmpty() || y.isEmpty() || z.isEmpty()) {
            return pos;
        }
        return createBlockPos(pos, x.get(), y.get(), z.get());
    }
    public static Dynamic<?> fixInlineBlockPos(Dynamic<?> input, String fieldX, String fieldY, String fieldZ, String newField) {
        Optional<Integer> x = intAt(input, fieldX);
        Optional<Integer> y = intAt(input, fieldY);
        Optional<Integer> z = intAt(input, fieldZ);
        if (x.isEmpty() || y.isEmpty() || z.isEmpty()) {
            return input;
        }
        return input.remove(fieldX).remove(fieldY).remove(fieldZ).set(newField, createBlockPos(input, x.get(), y.get(), z.get()));
    }
    public static Dynamic<?> createBlockPos(Dynamic<?> dynamic, int x, int y, int z) {
        return dynamic.createIntList(IntStream.of(x, y, z));
    }
    @SuppressWarnings("unchecked")
    public static <T, R> Typed<R> cast(Type<R> type, Typed<T> typed) {
        return new Typed<R>(type, typed.getOps(), (R) typed.getValue());
    }
    @SuppressWarnings("unchecked")
    public static <T> Typed<T> cast(Type<T> type, Object value, DynamicOps<?> ops) {
        return new Typed<T>(type, ops, (T) value);
    }
    public static Type<?> patchSubType(Type<?> type, Type<?> find, Type<?> replace) {
        return type.all(patcher(find, replace), true, false).view().newType();
    }
    private static <A, B> TypeRewriteRule patcher(Type<A> from, Type<B> to) {
        View<A, B> view = View.create("Patcher", from, to, ops -> a -> {
            throw new UnsupportedOperationException();
        });
        return TypeRewriteRule.everywhere(TypeRewriteRule.ifSame(from, RewriteResult.create(view, new BitSet())), PointFreeRule.nop(), true, true);
    }
    @SafeVarargs
    public static <T> Function<Typed<?>, Typed<?>> chainAllFilters(Function<Typed<?>, Typed<?>>... fixers) {
        return typed -> {
            Typed<?> current = typed;
            for (Function<Typed<?>, Typed<?>> fixer : fixers) {
                current = fixer.apply(current);
            }
            return current;
        };
    }
    public static Dynamic<?> fixStringField(Dynamic<?> dynamic, String fieldName, UnaryOperator<String> fix) {
        return dynamic.update(fieldName, field -> DataFixUtils.orElse(field.asString().map(fix).map(dynamic::createString).result(), field));
    }
    public static Optional<String> getPlainTranslationKey(Dynamic<?> textComponent) {
        return textComponent.getMapValues().result().filter(fields -> fields.size() == 1).flatMap(fields -> textComponent.get("translate").asString().result());
    }
    public static String dyeColorIdToName(int id) {
        return id >= 0 && id < kDyes.length ? kDyes[id] : kDyes[0];
    }
    public static <T> Typed<?> readAndSet(Typed<?> target, OpticFinder<T> optic, Dynamic<?> value) {
        return target.set(optic, Util.readTypedOrThrow(optic.type(), value, true));
    }
}
