// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.Type;
import com.mojang.datafixers.util.Pair;
import java.util.function.Predicate;
import java.util.function.UnaryOperator;
public abstract class ItemStackTagFix extends DataFix {
    public ItemStackTagFix(Schema outputSchema, String name, Predicate<String> idFilter) {
        super(outputSchema, false);
    }
    public final TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static UnaryOperator<Typed<?>> createFixer(Type<?> itemStackType, Predicate<String> idFilter, UnaryOperator<Typed<?>> fixer) {
        com.mojang.datafixers.OpticFinder<Pair<String, String>> idFinder = com.mojang.datafixers.DSL.fieldFinder("id",
            com.mojang.datafixers.DSL.named(References.ITEM_NAME.typeName(), net.minecraft.util.datafix.schemas.NamespacedSchema.namespacedString()));
        com.mojang.datafixers.OpticFinder<?> tagFinder = itemStackType.findField("tag");
        return stack -> {
            java.util.Optional<Pair<String, String>> id = stack.getOptional(idFinder);
            return id.isPresent() && idFilter.test(id.get().getSecond()) ? stack.updateTyped(tagFinder, fixer) : stack;
        };
    }
    protected abstract Typed<?> fixItemStackTag(Typed<?> tag);
}
