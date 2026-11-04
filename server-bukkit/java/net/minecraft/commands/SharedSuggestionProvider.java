// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import com.google.common.base.Strings;
import com.google.common.collect.Lists;
import com.google.common.base.CharMatcher;
import com.mojang.brigadier.Message;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.permissions.PermissionSetSupplier;
import net.minecraft.tags.TagKey;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.level.Level;
public interface SharedSuggestionProvider extends PermissionSetSupplier {
    CharMatcher MATCH_SPLITTER = CharMatcher.anyOf("._/");
    Collection<String> getOnlinePlayerNames();
    default Collection<String> getCustomTabSuggestions() {
        return getOnlinePlayerNames();
    }
    default Collection<String> getSelectedEntities() {
        return Collections.emptyList();
    }
    Collection<String> getAllTeams();
    Stream<Identifier> getAvailableSounds();
    Stream<Identifier> getAvailablePostEffects();
    CompletableFuture<Suggestions> customSuggestion(CommandContext<?> context);
    default Collection<TextCoordinates> getRelevantCoordinates() {
        return Collections.singleton(TextCoordinates.DEFAULT_GLOBAL);
    }
    default Collection<TextCoordinates> getAbsoluteCoordinates() {
        return Collections.singleton(TextCoordinates.DEFAULT_GLOBAL);
    }
    Set<ResourceKey<Level>> levels();
    RegistryAccess registryAccess();
    FeatureFlagSet enabledFeatures();
    default <E> void suggestRegistryElements(HolderLookup<E> registry, ElementSuggestionType elements, SuggestionsBuilder builder, Predicate<E> filter) {
        if (elements.shouldSuggestTags()) {
            suggestResource(registry.listTags().map(tag -> tag.key().location()), builder, "#");
        }
        if (elements.shouldSuggestElements()) {
            suggestResource(registry.listElements().filter(holder -> filter.test(holder.value())).map(Holder.Reference::key).map(ResourceKey::identifier), builder);
        }
    }
    static <S, E> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder, ResourceKey<? extends Registry<E>> registryKey, ElementSuggestionType type) {
        return listSuggestions(context, builder, registryKey, type, element -> true);
    }
    static <S, E> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder, ResourceKey<? extends Registry<E>> registryKey, ElementSuggestionType type, Predicate<E> filter) {
        if (context.getSource() instanceof SharedSuggestionProvider provider) {
            return provider.suggestRegistryElements(registryKey, type, builder, context, filter);
        }
        return builder.buildFuture();
    }
    <E> CompletableFuture<Suggestions> suggestRegistryElements(ResourceKey<? extends Registry<E>> key, ElementSuggestionType elements, SuggestionsBuilder builder, CommandContext<?> context, Predicate<E> filter);
    static <T> void filterResources(Iterable<T> values, String contents, Function<T, Identifier> converter, Consumer<T> consumer) {
        boolean hasNamespace = contents.indexOf(':') > -1;
        for (T value : values) {
            Identifier id = converter.apply(value);
            if (hasNamespace ? matchesSubStr(contents, id.toString()) : matchesSubStr(contents, id.getNamespace()) || matchesSubStr(contents, id.getPath())) {
                consumer.accept(value);
            }
        }
    }
    static <T> void filterResources(Iterable<T> values, String contents, String prefix, Function<T, Identifier> converter, Consumer<T> consumer) {
        if (contents.isEmpty()) {
            values.forEach(consumer);
            return;
        }
        String common = Strings.commonPrefix(contents, prefix);
        if (!common.isEmpty()) {
            filterResources(values, contents.substring(common.length()), converter, consumer);
        }
    }
    static CompletableFuture<Suggestions> suggestResource(Iterable<Identifier> values, SuggestionsBuilder builder, String prefix) {
        String contents = builder.getRemaining().toLowerCase(Locale.ROOT);
        filterResources(values, contents, prefix, id -> id, id -> builder.suggest(prefix + id));
        return builder.buildFuture();
    }
    static CompletableFuture<Suggestions> suggestResource(Stream<Identifier> values, SuggestionsBuilder builder, String prefix) {
        return suggestResource(values::iterator, builder, prefix);
    }
    static CompletableFuture<Suggestions> suggestResource(Iterable<Identifier> values, SuggestionsBuilder builder) {
        String contents = builder.getRemaining().toLowerCase(Locale.ROOT);
        filterResources(values, contents, id -> id, id -> builder.suggest(id.toString()));
        return builder.buildFuture();
    }
    static <T> CompletableFuture<Suggestions> suggestResource(Iterable<T> values, SuggestionsBuilder builder, Function<T, Identifier> id, Function<T, Message> tooltip) {
        String contents = builder.getRemaining().toLowerCase(Locale.ROOT);
        filterResources(values, contents, id, value -> builder.suggest(id.apply(value).toString(), tooltip.apply(value)));
        return builder.buildFuture();
    }
    static CompletableFuture<Suggestions> suggestResource(Stream<Identifier> values, SuggestionsBuilder builder) {
        return suggestResource(values::iterator, builder);
    }
    static <T> CompletableFuture<Suggestions> suggestResource(Stream<T> values, SuggestionsBuilder builder, Function<T, Identifier> id, Function<T, Message> tooltip) {
        return suggestResource(values::iterator, builder, id, tooltip);
    }
    static CompletableFuture<Suggestions> suggestCoordinates(String currentInput, Collection<TextCoordinates> allSuggestions, SuggestionsBuilder builder, Predicate<String> validator) {
        List<String> offered = new ArrayList<>();
        String[] typed = Strings.isNullOrEmpty(currentInput) ? new String[0] : currentInput.split(" ");
        if (typed.length > 2 || typed.length == 0 && !Strings.isNullOrEmpty(currentInput)) {
            return suggest(offered, builder);
        }
        for (TextCoordinates option : allSuggestions) {
            String[] axes = {option.x, option.y, option.z};
            System.arraycopy(typed, 0, axes, 0, typed.length);
            String complete = String.join(" ", axes);
            if (!validator.test(complete)) {
                continue;
            }
            for (int shown = typed.length + 1; shown < 3; shown++) {
                offered.add(String.join(" ", Arrays.copyOf(axes, shown)));
            }
            offered.add(complete);
        }
        return suggest(offered, builder);
    }
    static CompletableFuture<Suggestions> suggest2DCoordinates(String currentInput, Collection<TextCoordinates> allSuggestions, SuggestionsBuilder builder, Predicate<String> validator) {
        List<String> offered = new ArrayList<>();
        String[] typed = Strings.isNullOrEmpty(currentInput) ? new String[0] : currentInput.split(" ");
        if (typed.length > 1 || typed.length == 0 && !Strings.isNullOrEmpty(currentInput)) {
            return suggest(offered, builder);
        }
        for (TextCoordinates option : allSuggestions) {
            String complete = (typed.length == 0 ? option.x : typed[0]) + " " + option.z;
            if (!validator.test(complete)) {
                continue;
            }
            if (typed.length == 0) {
                offered.add(option.x);
            }
            offered.add(complete);
        }
        return suggest(offered, builder);
    }
    static CompletableFuture<Suggestions> suggest(Iterable<String> values, SuggestionsBuilder builder) {
        String prefix = builder.getRemaining().toLowerCase(Locale.ROOT);
        for (String name : values) {
            if (matchesSubStr(prefix, name.toLowerCase(Locale.ROOT))) {
                builder.suggest(name);
            }
        }
        return builder.buildFuture();
    }
    static CompletableFuture<Suggestions> suggest(Stream<String> values, SuggestionsBuilder builder) {
        String prefix = builder.getRemaining().toLowerCase(Locale.ROOT);
        values.filter(v -> matchesSubStr(prefix, v.toLowerCase(Locale.ROOT))).forEach(builder::suggest);
        return builder.buildFuture();
    }
    static CompletableFuture<Suggestions> suggest(String[] values, SuggestionsBuilder builder) {
        String prefix = builder.getRemaining().toLowerCase(Locale.ROOT);
        for (String name : values) {
            if (matchesSubStr(prefix, name.toLowerCase(Locale.ROOT))) {
                builder.suggest(name);
            }
        }
        return builder.buildFuture();
    }
    static <T> CompletableFuture<Suggestions> suggest(Iterable<T> values, SuggestionsBuilder builder, Function<T, String> toString, Function<T, Message> tooltip) {
        String prefix = builder.getRemaining().toLowerCase(Locale.ROOT);
        for (T value : values) {
            String name = toString.apply(value);
            if (matchesSubStr(prefix, name.toLowerCase(Locale.ROOT))) {
                builder.suggest(name, tooltip.apply(value));
            }
        }
        return builder.buildFuture();
    }
    static boolean matchesSubStr(String pattern, String input) {
        for (int index = 0; !input.startsWith(pattern, index); ) {
            int splitter = MATCH_SPLITTER.indexIn(input, index);
            if (splitter < 0) {
                return false;
            }
            index = splitter + 1;
        }
        return true;
    }
    public static class TextCoordinates {
        public static final TextCoordinates DEFAULT_LOCAL = new TextCoordinates("^", "^", "^");
        public static final TextCoordinates DEFAULT_GLOBAL = new TextCoordinates("~", "~", "~");
        public final String x;
        public final String y;
        public final String z;
        public TextCoordinates(String x, String y, String z) {
            this.x = x;
            this.y = y;
            this.z = z;
        }
    }
    public static enum ElementSuggestionType {
        TAGS, ELEMENTS, ALL;
        public boolean shouldSuggestTags() {
            return this == TAGS || this == ALL;
        }
        public boolean shouldSuggestElements() {
            return this == ELEMENTS || this == ALL;
        }
    }
}
