// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import net.minecraft.world.level.storage.loot.functions.LootItemFunctions;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.TagParser;
import net.minecraft.util.parsing.packrat.Atom;
import net.minecraft.util.parsing.packrat.Dictionary;
import net.minecraft.util.parsing.packrat.NamedRule;
import net.minecraft.util.parsing.packrat.ParseState;
import net.minecraft.util.parsing.packrat.Rule;
import net.minecraft.util.parsing.packrat.commands.Grammar;
import net.minecraft.resources.Identifier;
import com.mojang.brigadier.ImmutableStringReader;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.nbt.Tag;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.dialog.Dialog;
import net.minecraft.util.parsing.packrat.commands.Grammar;
import net.minecraft.world.level.levelgen.feature.Feature;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class ResourceOrIdArgument<T> implements ArgumentType<Holder<T>> {
    static final Collection<String> EXAMPLES = java.util.List.of("foo", "foo:bar", "012", "{}", "true");
    public static final DynamicCommandExceptionType ERROR_FAILED_TO_PARSE = new DynamicCommandExceptionType(error -> Component.translatableEscape("argument.resource_or_id.failed_to_parse", error));
    public static final Dynamic2CommandExceptionType ERROR_NO_SUCH_ELEMENT = new Dynamic2CommandExceptionType((id, registry) -> Component.translatableEscape("argument.resource_or_id.no_such_element", id, registry));
    public static final DynamicOps<Tag> OPS = net.minecraft.nbt.NbtOps.INSTANCE;
    private final HolderLookup.Provider registries;
    private final ResourceKey<? extends Registry<T>> registryKey;
    private final Codec<T> codec;
    private final Grammar<Result<T, Tag>> grammar;
    protected ResourceOrIdArgument(CommandBuildContext context, ResourceKey<? extends Registry<T>> registryKey, Codec<T> codec) {
        this.registries = context;
        this.registryKey = registryKey;
        this.codec = codec;
        grammar = createGrammar(registryKey, OPS);
        
    }
    public static <T, O> Grammar<Result<T, O>> createGrammar(ResourceKey<? extends Registry<T>> registryKey, DynamicOps<O> ops) {
        Dictionary<StringReader> rules = new Dictionary<>();
        TagParser<O> values = TagParser.create(ops);
        NamedRule<StringReader, Result<T, O>> top = rules.put(Atom.of("result"), new Rule<StringReader, Result<T, O>>() {
            @Override
            public Result<T, O> parse(ParseState<StringReader> state) {
                StringReader reader = state.input();
                reader.skipWhitespace();
                int start = reader.getCursor();
                try {
                    if (reader.canRead() && "{[\"'".indexOf(reader.peek()) >= 0) {
                        return new InlineResult<>(values.parseAsArgument(reader));
                    }
                    Identifier id = Identifier.readNonEmpty(reader);
                    return new ReferenceResult<>(ResourceKey.create(registryKey, id));
                } catch (CommandSyntaxException failure) {
                    state.errorCollector().store(reader.getCursor(), failure);
                    reader.setCursor(start);
                    return null;
                }
            }
        });
        return new Grammar<>(rules, top);
    }
    private static <T> Holder<T> getResource(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Holder.class);
    }

    public static LootTableArgument lootTable(CommandBuildContext buildContext) {
        return new LootTableArgument(buildContext);
    }
    public static Holder<LootTable> getLootTable(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return getResource(context, name);
    }
    public static LootModifierArgument lootModifier(CommandBuildContext buildContext) {
        return new LootModifierArgument(buildContext);
    }
    public static Holder<LootItemFunction> getLootModifier(CommandContext<CommandSourceStack> context, String name) {
        return getResource(context, name);
    }
    public static LootPredicateArgument lootPredicate(CommandBuildContext buildContext) {
        return new LootPredicateArgument(buildContext);
    }
    public static Holder<LootItemCondition> getLootPredicate(CommandContext<CommandSourceStack> context, String name) {
        return getResource(context, name);
    }
    public static ContextFloatProviderArgument floatProvider(CommandBuildContext buildContext) {
        return new ContextFloatProviderArgument(buildContext);
    }
    public static Holder<ContextFloatProvider> getFloatProvider(CommandContext<CommandSourceStack> context, String name) {
        return getResource(context, name);
    }
    public static ContextIntProviderArgument intProvider(CommandBuildContext buildContext) {
        return new ContextIntProviderArgument(buildContext);
    }
    public static Holder<ContextIntProvider> getIntProvider(CommandContext<CommandSourceStack> context, String name) {
        return getResource(context, name);
    }
    public static FeatureArgument feature(CommandBuildContext buildContext) {
        return new FeatureArgument(buildContext);
    }
    public static Holder<Feature> getFeature(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return getResource(context, name);
    }
    public static DialogArgument dialog(CommandBuildContext buildContext) {
        return new DialogArgument(buildContext);
    }
    public static Holder<Dialog> getDialog(CommandContext<CommandSourceStack> context, String name) {
        return getResource(context, name);
    }
    public Holder<T> parse(StringReader reader) throws CommandSyntaxException {
        Result<T, Tag> result = grammar.parseForCommands(reader);
        return result.parse(reader, registries, OPS, codec, registries.lookupOrThrow(registryKey));
    }
    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        return SharedSuggestionProvider.listSuggestions(context, builder, registryKey, SharedSuggestionProvider.ElementSuggestionType.ELEMENTS);
    }
    public Collection<String> getExamples() {
        return EXAMPLES;
    }
    public static class LootTableArgument extends ResourceOrIdArgument<LootTable> {
        protected LootTableArgument(CommandBuildContext context) {
            super(context, (net.minecraft.resources.ResourceKey) net.minecraft.core.registries.Registries.LOOT_TABLE, LootTable.DIRECT_CODEC);
        }
    }
    public static class LootModifierArgument extends ResourceOrIdArgument<LootItemFunction> {
        protected LootModifierArgument(CommandBuildContext context) {
            super(context, (net.minecraft.resources.ResourceKey) net.minecraft.core.registries.Registries.ITEM_MODIFIER, LootItemFunctions.DIRECT_CODEC);
        }
    }
    public static class LootPredicateArgument extends ResourceOrIdArgument<LootItemCondition> {
        protected LootPredicateArgument(CommandBuildContext context) {
            super(context, (net.minecraft.resources.ResourceKey) net.minecraft.core.registries.Registries.PREDICATE, LootItemCondition.DIRECT_CODEC);
        }
    }
    public static class ContextFloatProviderArgument extends ResourceOrIdArgument<ContextFloatProvider> {
        protected ContextFloatProviderArgument(CommandBuildContext context) {
            super(context, (net.minecraft.resources.ResourceKey) net.minecraft.core.registries.Registries.CONTEXT_FLOAT_PROVIDER, ContextFloatProviders.DIRECT_CODEC);
        }
    }
    public static class ContextIntProviderArgument extends ResourceOrIdArgument<ContextIntProvider> {
        protected ContextIntProviderArgument(CommandBuildContext context) {
            super(context, (net.minecraft.resources.ResourceKey) net.minecraft.core.registries.Registries.CONTEXT_INT_PROVIDER, ContextIntProviders.DIRECT_CODEC);
        }
    }
    public static class FeatureArgument extends ResourceOrIdArgument<Feature> {
        protected FeatureArgument(CommandBuildContext context) {
            super(context, (net.minecraft.resources.ResourceKey) net.minecraft.core.registries.Registries.FEATURE, Feature.DIRECT_CODEC);
        }
    }
    public static class DialogArgument extends ResourceOrIdArgument<Dialog> {
        protected DialogArgument(CommandBuildContext context) {
            super(context, (net.minecraft.resources.ResourceKey) net.minecraft.core.registries.Registries.DIALOG, Dialog.DIRECT_CODEC);
        }
    }
    public static record InlineResult<T, O>(O value) implements Result<T, O> {
        public Holder<T> parse(ImmutableStringReader reader, HolderLookup.Provider lookup, DynamicOps<O> ops, Codec<T> codec, HolderLookup.RegistryLookup<T> elementLookup) throws CommandSyntaxException {
            return Holder.direct(codec.parse(lookup.createSerializationContext(ops), value)
                .getOrThrow(message -> ERROR_FAILED_TO_PARSE.createWithContext(reader, message)));
        }
    }
    public static record ReferenceResult<T, O>(ResourceKey<T> key) implements Result<T, O> {
        public Holder<T> parse(ImmutableStringReader reader, HolderLookup.Provider lookup, DynamicOps<O> ops, Codec<T> codec, HolderLookup.RegistryLookup<T> elementLookup) throws CommandSyntaxException {
            return elementLookup.get(key).orElseThrow(() -> ERROR_NO_SUCH_ELEMENT.createWithContext(reader, key.identifier(), key.registry()));
        }
    }
    public interface Result<T, O> {
        Holder<T> parse(ImmutableStringReader reader, HolderLookup.Provider lookup, DynamicOps<O> ops, Codec<T> codec, HolderLookup.RegistryLookup<T> elementLookup) throws CommandSyntaxException;
    }
}
