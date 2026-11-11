// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.item;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import com.mojang.serialization.DataResult;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.Function;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.TagParser;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.RegistryOps;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.Item;

public class ItemParser {
    private static final DynamicCommandExceptionType kUnknownItem = new DynamicCommandExceptionType(id -> Component.translatableEscape("argument.item.id.invalid", id));
    private static final DynamicCommandExceptionType kUnknownComponent = new DynamicCommandExceptionType(id -> Component.translatableEscape("arguments.item.component.unknown", id));
    private static final Dynamic2CommandExceptionType kMalformedComponent = new Dynamic2CommandExceptionType((type, message) -> Component.translatableEscape("arguments.item.component.malformed", type, message));
    private static final SimpleCommandExceptionType kExpectedComponent = new SimpleCommandExceptionType(Component.translatable("arguments.item.component.expected"));
    private static final DynamicCommandExceptionType kRepeatedComponent = new DynamicCommandExceptionType(id -> Component.translatableEscape("arguments.item.component.repeated", id));
    public static final char SYNTAX_START_COMPONENTS = '[';
    public static final char SYNTAX_END_COMPONENTS = ']';
    public static final char SYNTAX_COMPONENT_SEPARATOR = ',';
    public static final char SYNTAX_COMPONENT_ASSIGNMENT = '=';
    public static final char SYNTAX_REMOVED_COMPONENT = '!';
    private static final Function<SuggestionsBuilder, CompletableFuture<Suggestions>> kSuggestNothing = SuggestionsBuilder::buildFuture;
    private final HolderLookup.RegistryLookup<Item> items;
    private final RegistryOps<Tag> registryOps;
    private final TagParser<Tag> tagParser;

    public ItemParser(HolderLookup.Provider registries) {
        items = registries.lookupOrThrow(Registries.ITEM);
        registryOps = registries.<Tag>createSerializationContext(NbtOps.INSTANCE);
        tagParser = TagParser.<Tag>create(registryOps);
    }

    public ItemInput parse(StringReader reader) throws CommandSyntaxException {
        Holder<Item>[] chosen = new Holder[1];
        DataComponentPatch.Builder patch = DataComponentPatch.builder();
        parse(reader, new Visitor() {
            @Override
            public void visitItem(Holder<Item> item) {
                chosen[0] = item;
            }

            @Override
            public <T> void visitComponent(DataComponentType<T> type, T value) {
                patch.set(type, value);
            }

            @Override
            public <T> void visitRemovedComponent(DataComponentType<T> type) {
                patch.remove(type);
            }
        });
        if (chosen[0] == null) {
            throw new IllegalStateException("Parser gave no item");
        }
        return new ItemInput(chosen[0], patch.build());
    }

    public void parse(StringReader reader, Visitor visitor) throws CommandSyntaxException {
        int start = reader.getCursor();
        try {
            new State(reader, visitor).parse();
        } catch (CommandSyntaxException failure) {
            reader.setCursor(start);
            throw failure;
        }
    }

    public CompletableFuture<Suggestions> fillSuggestions(SuggestionsBuilder builder) {
        StringReader reader = new StringReader(builder.getInput());
        reader.setCursor(builder.getStart());
        SuggestionsVisitor collector = new SuggestionsVisitor();
        try {
            new State(reader, collector).parse();
        } catch (CommandSyntaxException ignored) {
            // suggestions describe the position where parsing stopped
        }
        return collector.resolveSuggestions(builder, reader);
    }

    class State {
        private final StringReader reader;
        private final Visitor visitor;

        private State(StringReader reader, Visitor visitor) {
            this.reader = reader;
            this.visitor = visitor;
        }

        public void parse() throws CommandSyntaxException {
            visitor.visitSuggestions(this::suggestItem);
            int start = reader.getCursor();
            Identifier id = Identifier.read(reader);
            visitor.visitItem(items.get(ResourceKey.create(Registries.ITEM, id)).orElseThrow(() -> {
                reader.setCursor(start);
                return kUnknownItem.createWithContext(reader, id);
            }));
            visitor.visitSuggestions(builder -> {
                if (builder.getRemaining().isEmpty()) {
                    builder.suggest(String.valueOf(SYNTAX_START_COMPONENTS));
                }
                return builder.buildFuture();
            });
            if (reader.canRead() && reader.peek() == SYNTAX_START_COMPONENTS) {
                visitor.visitSuggestions(kSuggestNothing);
                readComponents();
            }
        }

        private void readComponents() throws CommandSyntaxException {
            reader.expect(SYNTAX_START_COMPONENTS);
            visitor.visitSuggestions(this::suggestAssignmentOrRemoval);
            Set<DataComponentType<?>> seen = new HashSet<>();
            while (reader.canRead() && reader.peek() != SYNTAX_END_COMPONENTS) {
                reader.skipWhitespace();
                boolean removal = reader.canRead() && reader.peek() == SYNTAX_REMOVED_COMPONENT;
                if (removal) {
                    reader.skip();
                    visitor.visitSuggestions(builder -> suggestComponent(builder, ""));
                }
                DataComponentType<?> type = readComponentType(reader);
                if (!seen.add(type)) {
                    throw kRepeatedComponent.create(type);
                }
                if (removal) {
                    visitor.visitRemovedComponent(type);
                } else {
                    visitor.visitSuggestions(builder -> suggestSingle(builder, SYNTAX_COMPONENT_ASSIGNMENT));
                    reader.skipWhitespace();
                    reader.expect(SYNTAX_COMPONENT_ASSIGNMENT);
                    visitor.visitSuggestions(kSuggestNothing);
                    reader.skipWhitespace();
                    readValue(type);
                }
                visitor.visitSuggestions(kSuggestNothing);
                reader.skipWhitespace();
                visitor.visitSuggestions(builder -> {
                    if (builder.getRemaining().isEmpty()) {
                        builder.suggest(String.valueOf(SYNTAX_COMPONENT_SEPARATOR));
                        builder.suggest(String.valueOf(SYNTAX_END_COMPONENTS));
                    }
                    return builder.buildFuture();
                });
                if (!reader.canRead() || reader.peek() != SYNTAX_COMPONENT_SEPARATOR) {
                    break;
                }
                reader.skip();
                reader.skipWhitespace();
                visitor.visitSuggestions(this::suggestAssignmentOrRemoval);
                if (!reader.canRead()) {
                    throw kExpectedComponent.createWithContext(reader);
                }
            }
            reader.expect(SYNTAX_END_COMPONENTS);
            visitor.visitSuggestions(kSuggestNothing);
        }

        public static DataComponentType<?> readComponentType(StringReader reader) throws CommandSyntaxException {
            if (!reader.canRead()) {
                throw kExpectedComponent.createWithContext(reader);
            }
            int start = reader.getCursor();
            Identifier id = Identifier.read(reader);
            DataComponentType<?> found = BuiltInRegistries.DATA_COMPONENT_TYPE.getValue(id);
            if (found == null || found.isTransient()) {
                reader.setCursor(start);
                throw kUnknownComponent.createWithContext(reader, id);
            }
            return found;
        }

        private <T> void readValue(DataComponentType<T> type) throws CommandSyntaxException {
            int start = reader.getCursor();
            Tag raw = tagParser.parseAsArgument(reader);
            DataResult<T> decoded = type.codecOrThrow().parse(registryOps, raw);
            visitor.visitComponent(type, decoded.getOrThrow(message -> {
                reader.setCursor(start);
                return kMalformedComponent.createWithContext(reader, type.toString(), message);
            }));
        }

        private CompletableFuture<Suggestions> suggestSingle(SuggestionsBuilder builder, char symbol) {
            if (builder.getRemaining().isEmpty()) {
                builder.suggest(String.valueOf(symbol));
            }
            return builder.buildFuture();
        }

        private CompletableFuture<Suggestions> suggestItem(SuggestionsBuilder builder) {
            return SharedSuggestionProvider.suggestResource(items.listElementIds().map(ResourceKey::identifier), builder);
        }

        private CompletableFuture<Suggestions> suggestAssignmentOrRemoval(SuggestionsBuilder builder) {
            builder.suggest(String.valueOf(SYNTAX_REMOVED_COMPONENT));
            return suggestComponent(builder, String.valueOf(SYNTAX_COMPONENT_ASSIGNMENT));
        }

        private CompletableFuture<Suggestions> suggestComponent(SuggestionsBuilder builder, String suffix) {
            String typed = builder.getRemaining().toLowerCase(Locale.ROOT);
            SharedSuggestionProvider.filterResources(BuiltInRegistries.DATA_COMPONENT_TYPE.entrySet(), typed,
                entry -> entry.getKey().identifier(), entry -> {
                    if (entry.getValue().codec() != null) {
                        builder.suggest(entry.getKey().identifier() + suffix);
                    }
                });
            return builder.buildFuture();
        }
    }

    static class SuggestionsVisitor implements Visitor {
        private Function<SuggestionsBuilder, CompletableFuture<Suggestions>> suggestions = kSuggestNothing;

        private SuggestionsVisitor() {}

        @Override
        public void visitSuggestions(Function<SuggestionsBuilder, CompletableFuture<Suggestions>> suggestions) {
            this.suggestions = suggestions;
        }

        public CompletableFuture<Suggestions> resolveSuggestions(SuggestionsBuilder builder, StringReader reader) {
            return suggestions.apply(builder.createOffset(reader.getCursor()));
        }
    }

    public interface Visitor {
        default void visitItem(Holder<Item> item) {}

        default <T> void visitComponent(DataComponentType<T> type, T value) {}

        default <T> void visitRemovedComponent(DataComponentType<T> type) {}

        default void visitSuggestions(Function<SuggestionsBuilder, CompletableFuture<Suggestions>> suggestions) {}
    }
}
