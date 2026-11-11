// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DSL;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.Type;
import com.mojang.datafixers.util.Either;
import com.mojang.datafixers.util.Pair;
import com.mojang.datafixers.util.Unit;
import com.mojang.serialization.Dynamic;
import java.util.List;
public class MissingDimensionFix extends DataFix {
    public MissingDimensionFix(Schema schema, boolean changesType) {
        super(schema, changesType);
    }
    protected static <A> Type<Pair<A, Dynamic<?>>> fields(String name, Type<A> type) {
        return (Type) DSL.and((Type) DSL.field(name, type), (Type) DSL.remainderType());
    }
    protected static <A> Type<Pair<Either<A, Unit>, Dynamic<?>>> optionalFields(String name, Type<A> type) {
        return DSL.and(DSL.optional(DSL.field(name, type)), DSL.remainderType());
    }
    protected static <A1, A2> Type<Pair<Either<A1, Unit>, Pair<Either<A2, Unit>, Dynamic<?>>>> optionalFields(String name1, Type<A1> type1, String name2, Type<A2> type2) {
        return DSL.and(DSL.optional(DSL.field(name1, type1)), DSL.optional(DSL.field(name2, type2)), DSL.remainderType());
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    protected static Type<? extends Pair<? extends Either<? extends Pair<? extends Either<?, Unit>, ? extends Pair<? extends Either<? extends List<? extends Pair<? extends Either<?, Unit>, Dynamic<?>>>, Unit>, Dynamic<?>>>, Unit>, Dynamic<?>>> flatType(Schema schema) {
        return (Type) optionalFields("settings", optionalFields("biome", schema.getType(References.BIOME), "layers",
            DSL.list(optionalFields("block", schema.getType(References.BLOCK_NAME)))));
    }
}
