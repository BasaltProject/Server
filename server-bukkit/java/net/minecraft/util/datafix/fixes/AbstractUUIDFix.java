// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DSL;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.Optional;
import java.util.function.Function;
public abstract class AbstractUUIDFix extends DataFix {
    protected final DSL.TypeReference typeReference;
    public AbstractUUIDFix(Schema outputSchema, DSL.TypeReference typeReference) {
        super(outputSchema, false);
        this.typeReference = typeReference;
    }
    protected Typed<?> updateNamedChoice(Typed<?> input, String name, Function<Dynamic<?>, Dynamic<?>> function) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), input);
    }
    protected static Optional<Dynamic<?>> replaceUUIDString(Dynamic<?> tag, String oldKey, String newKey) {
        return createUUIDFromString(tag, oldKey).map(made -> tag.remove(oldKey).set(newKey, made));
    }
    protected static Optional<Dynamic<?>> replaceUUIDMLTag(Dynamic<?> tag, String oldKey, String newKey) {
        return tag.get(oldKey).result().flatMap(AbstractUUIDFix::createUUIDFromML).map(made -> tag.remove(oldKey).set(newKey, made));
    }
    protected static Optional<Dynamic<?>> replaceUUIDLeastMost(Dynamic<?> tag, String oldKey, String newKey) {
        String most = oldKey + "Most";
        String least = oldKey + "Least";
        return createUUIDFromLongs(tag, most, least).map(made -> tag.remove(most).remove(least).set(newKey, made));
    }
    protected static Optional<Dynamic<?>> createUUIDFromString(Dynamic<?> tag, String oldKey) {
        String text = tag.get(oldKey).result().map(value -> value.asString((String) null)).orElse(null);
        if (text == null) {
            return Optional.empty();
        }
        try {
            java.util.UUID uuid = java.util.UUID.fromString(text);
            return createUUIDTag(tag, uuid.getMostSignificantBits(), uuid.getLeastSignificantBits());
        } catch (IllegalArgumentException invalid) {
            return Optional.empty();
        }
    }
    protected static Optional<Dynamic<?>> createUUIDFromML(Dynamic<?> tag) {
        return createUUIDFromLongs(tag, "M", "L");
    }
    protected static Optional<Dynamic<?>> createUUIDFromLongs(Dynamic<?> tag, String mostKey, String leastKey) {
        long most = tag.get(mostKey).asLong(0L);
        long least = tag.get(leastKey).asLong(0L);
        return most != 0L && least != 0L ? createUUIDTag(tag, most, least) : Optional.empty();
    }
    protected static Optional<Dynamic<?>> createUUIDTag(Dynamic<?> tag, long mostSignificantBits, long leastSignificantBits) {
        return Optional.of(tag.createIntList(java.util.stream.IntStream.of((int) (mostSignificantBits >> 32), (int) mostSignificantBits,
            (int) (leastSignificantBits >> 32), (int) leastSignificantBits)));
    }
}
