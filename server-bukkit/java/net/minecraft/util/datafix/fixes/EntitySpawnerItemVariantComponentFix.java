// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.function.Function;
public class EntitySpawnerItemVariantComponentFix extends DataFix {
    public EntitySpawnerItemVariantComponentFix(Schema outputSchema) {
        super(outputSchema, false);
    }
    public final TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Typed<?> fixTyped(Typed<?> components, Fixer fixer) {
        return components.update(com.mojang.datafixers.DSL.remainderFinder(), (java.util.function.Function) (remainder -> fixer.fixRemainder((Dynamic) remainder)));
    }
    interface Fixer extends Function<Typed<?>, Typed<?>> {
        default Typed<?> apply(Typed<?> components) {
            return fixTyped(components, this);
        }
        default <T> Dynamic<T> fixRemainder(Dynamic<T> remainder) {
            return remainder.get("minecraft:bucket_entity_data").result().map(bucket -> fixRemainder(remainder, bucket)).orElse(remainder);
        }
        <T> Dynamic<T> fixRemainder(Dynamic<T> remainder, Dynamic<T> bucketData);
    }
}
