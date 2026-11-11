// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import net.minecraft.util.ProblemReporter;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import com.mojang.serialization.DataResult;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.context.ContextKeySet;
public interface Validatable {
    private static void validateElements(ValidationContext context, HolderSet<? extends Validatable> holderSet) {
        if (!holderSet.isBound()) {
            return;
        }
        ValidationContext scope = context;
        if (holderSet instanceof HolderSet.Named<?> named) {
            TagKey<?> id = named.key();
            if (context.hasVisitedTag(id)) {
                context.reportProblem(new ValidationContext.RecursiveTagReferenceProblem(id));
                return;
            }
            scope = context.enterTag(new ProblemReporter.CollectionReferencePathElement(id), id);
        }
        for (int i = 0; i < holderSet.size(); i++) {
            validateHolder(scope.forChild(new ProblemReporter.IndexedPathElement(i)), holderSet.get(i));
        }
    }
    void validate(ValidationContext context);
    static void validate(ValidationContext context, String name, Validatable v) {
        v.validate(context.forField(name));
    }
    static void validate(ValidationContext context, String name, Optional<? extends Validatable> optional) {
        optional.ifPresent(inner -> inner.validate(context.forField(name)));
    }
    static void validateHolder(ValidationContext context, Holder<? extends Validatable> holder) {
        ValidationContext scope = context;
        if (holder instanceof Holder.Reference<?> reference) {
            ResourceKey<?> id = reference.key();
            if (context.hasVisitedElement(id)) {
                context.reportProblem(new ValidationContext.RecursiveElementReferenceProblem(id));
                return;
            }
            scope = context.enterElement(new ProblemReporter.ElementReferencePathElement(id), id);
        }
        holder.value().validate(scope);
    }
    static void validateHolder(ValidationContext context, String name, Holder<? extends Validatable> holder) {
        validateHolder(context.forField(name), holder);
    }
    static void validateHolder(ValidationContext context, String name, Optional<? extends Holder<? extends Validatable>> optional) {
        optional.ifPresent(inner -> validateHolder(context, name, inner));
    }
    static void validateHolder(ValidationContext context, String name, List<? extends Holder<? extends Validatable>> list) {
        for (int i = 0; i < list.size(); i++) {
            list.get(i).value().validate(context.forIndexedField(name, i));
        }
    }
    static void validateHolderSet(ValidationContext context, String name, HolderSet<? extends Validatable> holderSet, int minSize) {
        ValidationContext scope = context.forField(name);
        validateElements(scope, holderSet);
        if (holderSet.size() < minSize) {
            scope.reportProblem(new ValidationContext.MinBoundsProblem(minSize));
        }
    }
    static void validateHolderSet(ValidationContext context, String name, HolderSet<? extends Validatable> holderSet) {
        validateElements(context.forField(name), holderSet);
    }
    static void validate(ValidationContext context, String name, List<? extends Validatable> list) {
        for (int i = 0; i < list.size(); i++) {
            list.get(i).validate(context.forIndexedField(name, i));
        }
    }
    static void validate(ValidationContext context, List<? extends Validatable> list) {
        for (int i = 0; i < list.size(); i++) {
            list.get(i).validate(context.forChild(new ProblemReporter.IndexedPathElement(i)));
        }
    }
    static <T extends Validatable> Function<T, DataResult<T>> validatorForContext(ContextKeySet params) {
        return value -> {
            ProblemReporter.Collector found = new ProblemReporter.Collector();
            value.validate(new ValidationContext(found, params));
            return found.isEmpty() ? DataResult.success(value) : DataResult.error(() -> "Validation error: " + found.getReport());
        };
    }
    static <T extends Validatable> Function<List<T>, DataResult<List<T>>> listValidatorForContext(ContextKeySet params) {
        return values -> {
            ProblemReporter.Collector found = new ProblemReporter.Collector();
            validate(new ValidationContext(found, params), values);
            return found.isEmpty() ? DataResult.success(values) : DataResult.error(() -> "Validation error: " + found.getReport());
        };
    }
}
