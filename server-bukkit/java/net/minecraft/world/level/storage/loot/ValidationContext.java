// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.util.ProblemReporter;
import net.minecraft.util.context.ContextKey;
import net.minecraft.util.context.ContextKeySet;
public class ValidationContext {
    private final ProblemReporter reporter;
    private final ContextKeySet contextKeySet;
    private final Optional<HolderGetter.Provider> resolver;
    private final Set<Object> visitedObjects;
    private ValidationContext enter(ProblemReporter.PathElement subContext, Object object) {
        Set<Object> seen = new java.util.HashSet<>(visitedObjects);
        seen.add(object);
        return new ValidationContext(reporter.forChild(subContext), contextKeySet, resolver, java.util.Collections.unmodifiableSet(seen));
    }
    public ValidationContext(ProblemReporter reporter, ContextKeySet contextKeySet, HolderGetter.Provider resolver) {
        this(reporter, contextKeySet, Optional.of(resolver), Set.of());
    }
    public ValidationContext(ProblemReporter reporter, ContextKeySet contextKeySet) {
        this(reporter, contextKeySet, Optional.empty(), Set.of());
    }
    private ValidationContext(ProblemReporter reporter, ContextKeySet contextKeySet, Optional<HolderGetter.Provider> resolver, Set<Object> visitedObjects) {
        this.reporter = reporter;
        this.contextKeySet = contextKeySet;
        this.resolver = resolver;
        this.visitedObjects = visitedObjects;
    }
    public ValidationContext forChild(ProblemReporter.PathElement subContext) {
        return new ValidationContext(reporter.forChild(subContext), contextKeySet, resolver, visitedObjects);
    }
    public ValidationContext forField(String name) {
        return forChild(new ProblemReporter.FieldPathElement(name));
    }
    public ValidationContext forIndexedField(String name, int index) {
        return forChild(new ProblemReporter.IndexedFieldPathElement(name, index));
    }
    public ValidationContext forMapField(String name, String key) {
        return forChild(new ProblemReporter.MapEntryPathElement(name, key));
    }
    public ValidationContext enterElement(ProblemReporter.PathElement subContext, ResourceKey<?> element) {
        return enter(subContext, element);
    }
    public boolean hasVisitedElement(ResourceKey<?> element) {
        return visitedObjects.contains(element);
    }
    public ValidationContext enterTag(ProblemReporter.PathElement subContext, TagKey<?> tag) {
        return enter(subContext, tag);
    }
    public boolean hasVisitedTag(TagKey<?> tag) {
        return visitedObjects.contains(tag);
    }
    public void reportProblem(ProblemReporter.Problem description) {
        reporter.report(description);
    }
    public void validateContextUsage(LootContextUser lootContextUser) {
        Set<ContextKey<?>> missing = new java.util.HashSet<>(lootContextUser.getReferencedContextParams());
        missing.removeAll(contextKeySet.allowed());
        if (!missing.isEmpty()) {
            reporter.report(new ParametersNotProvidedProblem(missing));
        }
    }
    public HolderGetter.Provider resolver() {
        return resolver.orElseThrow(() -> new UnsupportedOperationException("References not allowed"));
    }
    public ProblemReporter reporter() {
        return reporter;
    }
    public static record ParametersNotProvidedProblem(Set<ContextKey<?>> notProvided) implements ProblemReporter.Problem {
        public String description() {
            return "Parameters " + notProvided + " are not provided in this context";
        }
    }
    public static record RecursiveElementReferenceProblem(ResourceKey<?> referenced) implements ProblemReporter.Problem {
        public String description() {
            return referenced.identifier() + " of type " + referenced.registry() + " is recursively called";
        }
        public boolean isFatal() {
            return true;
        }
    }
    public static record RecursiveTagReferenceProblem(TagKey<?> referenced) implements ProblemReporter.Problem {
        public String description() {
            return "#" + referenced.location() + " of type " + referenced.registry() + " is recursively called";
        }
        public boolean isFatal() {
            return true;
        }
    }
    public static record MinBoundsProblem(int minBounds) implements ProblemReporter.Problem {
        public String description() {
            return "List must contain at least " + minBounds + " element(s)";
        }
    }
}
