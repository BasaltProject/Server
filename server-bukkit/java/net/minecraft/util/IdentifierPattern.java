// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Predicate;
import java.util.regex.Pattern;
import net.minecraft.resources.Identifier;
public class IdentifierPattern {
    public static final Codec<IdentifierPattern> CODEC = RecordCodecBuilder.create(i -> i.group(
        ExtraCodecs.PATTERN.optionalFieldOf("namespace").forGetter(o -> o.namespacePattern),
        ExtraCodecs.PATTERN.optionalFieldOf("path").forGetter(o -> o.pathPattern)).apply(i, IdentifierPattern::new));
    private final Optional<Pattern> namespacePattern;
    private final Predicate<String> namespacePredicate;
    private final Optional<Pattern> pathPattern;
    private final Predicate<String> pathPredicate;
    private final Predicate<Identifier> locationPredicate;
    private IdentifierPattern(Optional<Pattern> namespacePattern, Optional<Pattern> pathPattern) {
        this.namespacePattern = namespacePattern;
        this.namespacePredicate = namespacePattern.<Predicate<String>>map(Pattern::asPredicate).orElse(text -> true);
        this.pathPattern = pathPattern;
        this.pathPredicate = pathPattern.<Predicate<String>>map(Pattern::asPredicate).orElse(text -> true);
        this.locationPredicate = id -> namespacePredicate.test(id.getNamespace()) && pathPredicate.test(id.getPath());
    }
    public Predicate<String> namespacePredicate() {
        return namespacePredicate;
    }
    public Predicate<String> pathPredicate() {
        return pathPredicate;
    }
    public Predicate<Identifier> locationPredicate() {
        return locationPredicate;
    }
}
