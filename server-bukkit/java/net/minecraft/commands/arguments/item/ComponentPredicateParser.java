// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.item;
import java.util.ArrayList;
import java.util.Optional;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.nbt.NbtOps;
import net.minecraft.util.Unit;
import net.minecraft.util.parsing.packrat.Atom;
import net.minecraft.util.parsing.packrat.Dictionary;
import net.minecraft.util.parsing.packrat.Scope;
import net.minecraft.util.parsing.packrat.Term;
import net.minecraft.util.parsing.packrat.commands.IdentifierParseRule;
import net.minecraft.util.parsing.packrat.commands.StringReaderTerms;
import net.minecraft.util.parsing.packrat.commands.TagParseRule;
import com.mojang.brigadier.ImmutableStringReader;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.Dynamic;
import java.util.List;
import java.util.stream.Stream;
import net.minecraft.resources.Identifier;
import net.minecraft.util.parsing.packrat.NamedRule;
import net.minecraft.util.parsing.packrat.commands.Grammar;
import net.minecraft.util.parsing.packrat.commands.ResourceLookupRule;
public class ComponentPredicateParser {
    public static <T, C, P> Grammar<List<T>> createGrammar(Context<T, C, P> context) {
        Atom<List<T>> topAtom = Atom.of("top");
        Atom<Optional<T>> typeAtom = Atom.of("type");
        Atom<Unit> anyAtom = Atom.of("any_type");
        Atom<T> elementAtom = Atom.of("element_type");
        Atom<T> tagTypeAtom = Atom.of("tag_type");
        Atom<List<T>> conditionsAtom = Atom.of("conditions");
        Atom<List<T>> alternativesAtom = Atom.of("alternatives");
        Atom<T> termAtom = Atom.of("term");
        Atom<T> negationAtom = Atom.of("negation");
        Atom<T> testAtom = Atom.of("test");
        Atom<C> componentAtom = Atom.of("component_type");
        Atom<P> predicateAtom = Atom.of("predicate_type");
        Atom<Identifier> idAtom = Atom.of("id");
        Atom<Dynamic<?>> valueAtom = Atom.of("tag");
        Dictionary<StringReader> rules = new Dictionary<>();
        NamedRule<StringReader, Identifier> idRule = rules.put(idAtom, IdentifierParseRule.INSTANCE);
        Term<StringReader> comma = StringReaderTerms.character(',');
        Term<StringReader> bar = StringReaderTerms.character('|');
        NamedRule<StringReader, List<T>> top = rules.put(topAtom,
            Term.alternative(Term.sequence(rules.named(typeAtom), StringReaderTerms.character('['), Term.cut(),
                Term.optional(rules.named(conditionsAtom)), StringReaderTerms.character(']')), rules.named(typeAtom)),
            scope -> {
                List<T> all = new ArrayList<>();
                scope.getOrThrow(typeAtom).ifPresent(all::add);
                List<T> extra = scope.get(conditionsAtom);
                if (extra != null) {
                    all.addAll(extra);
                }
                return List.copyOf(all);
            });
        rules.put(typeAtom, Term.alternative(rules.named(elementAtom), Term.sequence(StringReaderTerms.character('#'), Term.cut(), rules.named(tagTypeAtom)),
            rules.named(anyAtom)), scope -> Optional.ofNullable(scope.getAny(elementAtom, tagTypeAtom)));
        rules.put(anyAtom, StringReaderTerms.character('*'), scope -> Unit.INSTANCE);
        rules.put(elementAtom, new ElementLookupRule<T, C, P>(idRule, context));
        rules.put(tagTypeAtom, new TagLookupRule<T, C, P>(idRule, context));
        rules.put(conditionsAtom, Term.sequence(rules.named(alternativesAtom), Term.optional(Term.sequence(comma, rules.named(conditionsAtom)))), scope -> {
            List<T> joined = new ArrayList<>();
            joined.add(context.anyOf(scope.getOrThrow(alternativesAtom)));
            List<T> rest = scope.get(conditionsAtom);
            if (rest != null) {
                joined.addAll(rest);
            }
            return List.copyOf(joined);
        });
        rules.put(alternativesAtom, Term.sequence(rules.named(termAtom), Term.optional(Term.sequence(bar, rules.named(alternativesAtom)))), scope -> {
            List<T> joined = new ArrayList<>();
            joined.add(scope.getOrThrow(termAtom));
            List<T> rest = scope.get(alternativesAtom);
            if (rest != null) {
                joined.addAll(rest);
            }
            return List.copyOf(joined);
        });
        rules.put(termAtom, Term.alternative(rules.named(testAtom), Term.sequence(StringReaderTerms.character('!'), rules.named(negationAtom))),
            scope -> scope.getAnyOrThrow(testAtom, negationAtom));
        rules.put(negationAtom, rules.named(testAtom), scope -> context.negate(scope.getOrThrow(testAtom)));
        rules.putComplex(testAtom, Term.alternative(
            Term.sequence(rules.named(componentAtom), StringReaderTerms.character('='), Term.cut(), rules.named(valueAtom)),
            Term.sequence(rules.named(predicateAtom), StringReaderTerms.character('~'), Term.cut(), rules.named(valueAtom)),
            rules.named(componentAtom)), state -> {
            Scope scope = state.scope();
            P predicate = scope.get(predicateAtom);
            try {
                if (predicate != null) {
                    return context.createPredicateTest(state.input(), predicate, scope.getOrThrow(valueAtom));
                }
                C component = scope.getOrThrow(componentAtom);
                Dynamic<?> value = scope.get(valueAtom);
                return value != null ? context.createComponentTest(state.input(), component, value) : context.createComponentTest(state.input(), component);
            } catch (CommandSyntaxException failure) {
                state.errorCollector().store(state.mark(), failure);
                return null;
            }
        });
        rules.put(componentAtom, new ComponentLookupRule<T, C, P>(idRule, context));
        rules.put(predicateAtom, new PredicateLookupRule<T, C, P>(idRule, context));
        rules.put(valueAtom, new TagParseRule<>(NbtOps.INSTANCE));
        return new Grammar<>(rules, top);
    }
    static class ElementLookupRule<T, C, P> extends ResourceLookupRule<Context<T, C, P>, T> {
        private ElementLookupRule(NamedRule<StringReader, Identifier> idParser, Context<T, C, P> context) {
            super(idParser, context);
        }
        protected T validateElement(ImmutableStringReader reader, Identifier id) throws Exception {
            return context.forElementType(reader, id);
        }
        public Stream<Identifier> possibleResources() {
            return context.listElementTypes();
        }
    }
    static class TagLookupRule<T, C, P> extends ResourceLookupRule<Context<T, C, P>, T> {
        private TagLookupRule(NamedRule<StringReader, Identifier> idParser, Context<T, C, P> context) {
            super(idParser, context);
        }
        protected T validateElement(ImmutableStringReader reader, Identifier id) throws Exception {
            return context.forTagType(reader, id);
        }
        public Stream<Identifier> possibleResources() {
            return context.listTagTypes();
        }
    }
    static class ComponentLookupRule<T, C, P> extends ResourceLookupRule<Context<T, C, P>, C> {
        private ComponentLookupRule(NamedRule<StringReader, Identifier> idParser, Context<T, C, P> context) {
            super(idParser, context);
        }
        protected C validateElement(ImmutableStringReader reader, Identifier id) throws Exception {
            return context.lookupComponentType(reader, id);
        }
        public Stream<Identifier> possibleResources() {
            return context.listComponentTypes();
        }
    }
    static class PredicateLookupRule<T, C, P> extends ResourceLookupRule<Context<T, C, P>, P> {
        private PredicateLookupRule(NamedRule<StringReader, Identifier> idParser, Context<T, C, P> context) {
            super(idParser, context);
        }
        protected P validateElement(ImmutableStringReader reader, Identifier id) throws Exception {
            return context.lookupPredicateType(reader, id);
        }
        public Stream<Identifier> possibleResources() {
            return context.listPredicateTypes();
        }
    }
    public interface Context<T, C, P> {
        T forElementType(ImmutableStringReader input, Identifier id) throws CommandSyntaxException;
        Stream<Identifier> listElementTypes();
        T forTagType(ImmutableStringReader input, Identifier id) throws CommandSyntaxException;
        Stream<Identifier> listTagTypes();
        C lookupComponentType(ImmutableStringReader input, Identifier id) throws CommandSyntaxException;
        Stream<Identifier> listComponentTypes();
        T createComponentTest(ImmutableStringReader input, C componentType, Dynamic<?> value) throws CommandSyntaxException;
        T createComponentTest(ImmutableStringReader input, C componentType);
        P lookupPredicateType(ImmutableStringReader input, Identifier id) throws CommandSyntaxException;
        Stream<Identifier> listPredicateTypes();
        T createPredicateTest(ImmutableStringReader input, P predicateType, Dynamic<?> value) throws CommandSyntaxException;
        T negate(T value);
        T anyOf(List<T> alternatives);
    }
}
