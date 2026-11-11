// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.selector;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.function.BiConsumer;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.commands.arguments.selector.options.EntitySelectorOptions;
import net.minecraft.commands.arguments.selector.options.InvertableSetOptionState;
import net.minecraft.commands.arguments.selector.options.SetOnceOptionState;
import net.minecraft.network.chat.Component;
import net.minecraft.server.permissions.PermissionSetSupplier;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.phys.Vec3;

public class EntitySelectorParser {
    public static final char SYNTAX_SELECTOR_START = '@';
    public static final char SYNTAX_OPTIONS_START = '[';
    public static final char SYNTAX_OPTIONS_END = ']';
    public static final char SYNTAX_OPTIONS_KEY_VALUE_SEPARATOR = '=';
    public static final char SYNTAX_OPTIONS_SEPARATOR = ',';
    public static final char SYNTAX_NOT = '!';
    public static final char SYNTAX_TAG = '#';
    private static final char kNearest = 'p';
    private static final char kAllPlayers = 'a';
    private static final char kRandom = 'r';
    private static final char kSelf = 's';
    private static final char kAllEntities = 'e';
    private static final char kNearestEntity = 'n';
    public static final SimpleCommandExceptionType ERROR_INVALID_NAME_OR_UUID =
        new SimpleCommandExceptionType(Component.translatable("argument.entity.invalid"));
    public static final DynamicCommandExceptionType ERROR_UNKNOWN_SELECTOR_TYPE =
        new DynamicCommandExceptionType(type -> Component.translatableEscape("argument.entity.selector.unknown", type));
    public static final SimpleCommandExceptionType ERROR_SELECTORS_NOT_ALLOWED =
        new SimpleCommandExceptionType(Component.translatable("argument.entity.selector.not_allowed"));
    public static final SimpleCommandExceptionType ERROR_MISSING_SELECTOR_TYPE =
        new SimpleCommandExceptionType(Component.translatable("argument.entity.selector.missing"));
    public static final SimpleCommandExceptionType ERROR_EXPECTED_END_OF_OPTIONS =
        new SimpleCommandExceptionType(Component.translatable("argument.entity.options.unterminated"));
    public static final DynamicCommandExceptionType ERROR_EXPECTED_OPTION_VALUE =
        new DynamicCommandExceptionType(key -> Component.translatableEscape("argument.entity.options.valueless", key));
    public static final BiConsumer<Vec3, List<? extends Entity>> ORDER_NEAREST =
        (origin, entities) -> entities.sort(Comparator.comparingDouble(entity -> entity.distanceToSqr(origin)));
    public static final BiConsumer<Vec3, List<? extends Entity>> ORDER_FURTHEST =
        (origin, entities) -> entities.sort(Comparator.<Entity>comparingDouble(entity -> entity.distanceToSqr(origin)).reversed());
    public static final BiConsumer<Vec3, List<? extends Entity>> ORDER_RANDOM =
        (origin, entities) -> java.util.Collections.shuffle(entities);
    public static final BiFunction<SuggestionsBuilder, Consumer<SuggestionsBuilder>, CompletableFuture<Suggestions>> SUGGEST_NOTHING =
        (builder, names) -> builder.buildFuture();
    private final StringReader reader;
    private final boolean allowSelectors;
    private final int start;
    private int maxResults;
    private boolean includesEntities;
    private boolean worldLimited;
    private MinMaxBounds.Doubles distance = MinMaxBounds.Doubles.ANY;
    private MinMaxBounds.Ints level = MinMaxBounds.Ints.ANY;
    private Double x;
    private Double y;
    private Double z;
    private Double deltaX;
    private Double deltaY;
    private Double deltaZ;
    private MinMaxBounds.FloatDegrees rotX = MinMaxBounds.FloatDegrees.ANY;
    private MinMaxBounds.FloatDegrees rotY = MinMaxBounds.FloatDegrees.ANY;
    private final List<Predicate<Entity>> predicates = new ArrayList<>();
    private BiConsumer<Vec3, List<? extends Entity>> order = EntitySelector.ORDER_ARBITRARY;
    private boolean currentEntity;
    private String playerName;
    private UUID entityUuid;
    private EntityType<?> type;
    private boolean usesSelectors;
    private boolean invertedValue;
    private boolean tagValue;
    private BiFunction<SuggestionsBuilder, Consumer<SuggestionsBuilder>, CompletableFuture<Suggestions>> suggestions = SUGGEST_NOTHING;
    private final InvertableSetOptionState nameOption = new InvertableSetOptionState();
    private final SetOnceOptionState limitedOption = new SetOnceOptionState();
    private final SetOnceOptionState sortedOption = new SetOnceOptionState();
    private final InvertableSetOptionState gamemodeOption = new InvertableSetOptionState();
    private final InvertableSetOptionState teamOption = new InvertableSetOptionState();
    private final InvertableSetOptionState typeOption = new InvertableSetOptionState();
    private final SetOnceOptionState scoresOption = new SetOnceOptionState();
    private final SetOnceOptionState advancementsOption = new SetOnceOptionState();

    public EntitySelectorParser(StringReader reader, boolean allowSelectors) {
        this.reader = reader;
        this.allowSelectors = allowSelectors;
        this.start = reader.getCursor();
    }

    public static <S> boolean allowSelectors(S source) {
        return source instanceof PermissionSetSupplier supplier && allowSelectors(supplier);
    }

    public static boolean allowSelectors(PermissionSetSupplier source) {
        return source.permissions().hasPermission(net.minecraft.server.permissions.Permissions.COMMANDS_ENTITY_SELECTORS);
    }

    public EntitySelector getSelector() {
        return EntitySelector.ofText(reader.getString().substring(start, reader.getCursor()), maxResults, includesEntities);
    }

    protected void parseSelector() throws CommandSyntaxException {
        usesSelectors = true;
        suggestions = this::suggestSelector;
        if (!reader.canRead()) {
            throw ERROR_MISSING_SELECTOR_TYPE.createWithContext(reader);
        }
        int typeStart = reader.getCursor();
        char kind = reader.read();
        switch (kind) {
            case kNearest -> select(1, false, ORDER_NEAREST, net.minecraft.world.entity.EntityTypes.PLAYER);
            case kAllPlayers -> select(EntitySelector.INFINITE, false, EntitySelector.ORDER_ARBITRARY, net.minecraft.world.entity.EntityTypes.PLAYER);
            case kRandom -> select(1, false, ORDER_RANDOM, net.minecraft.world.entity.EntityTypes.PLAYER);
            case kSelf -> {
                maxResults = 1;
                includesEntities = true;
                currentEntity = true;
            }
            case kAllEntities -> select(EntitySelector.INFINITE, true, EntitySelector.ORDER_ARBITRARY, null);
            case kNearestEntity -> select(1, true, ORDER_NEAREST, null);
            default -> {
                reader.setCursor(typeStart);
                throw ERROR_UNKNOWN_SELECTOR_TYPE.createWithContext(reader, "@" + kind);
            }
        }
        suggestions = this::suggestOpenOptions;
        if (reader.canRead() && reader.peek() == SYNTAX_OPTIONS_START) {
            reader.skip();
            suggestions = this::suggestOptionsKeyOrClose;
            parseOptions();
        }
    }

    private void select(int limit, boolean entities, BiConsumer<Vec3, List<? extends Entity>> sorting, EntityType<?> only) {
        maxResults = limit;
        includesEntities = entities;
        order = sorting;
        if (only != null) {
            limitToType(only);
        }
    }

    protected void parseNameOrUUID() throws CommandSyntaxException {
        int begin = reader.getCursor();
        String text = reader.readString();
        try {
            entityUuid = UUID.fromString(text);
            includesEntities = true;
            return;
        } catch (IllegalArgumentException ignored) {
            // not a uuid, treat as a player name
        }
        if (text.isEmpty() || text.length() > 16) {
            reader.setCursor(begin);
            throw ERROR_INVALID_NAME_OR_UUID.createWithContext(reader);
        }
        includesEntities = false;
        playerName = text;
        maxResults = 1;
    }

    protected void parseOptions() throws CommandSyntaxException {
        suggestions = this::suggestOptionsKey;
        reader.skipWhitespace();
        while (reader.canRead() && reader.peek() != SYNTAX_OPTIONS_END) {
            reader.skipWhitespace();
            int keyStart = reader.getCursor();
            String key = reader.readString();
            EntitySelectorOptions.Modifier modifier = EntitySelectorOptions.get(this, key, keyStart);
            reader.skipWhitespace();
            if (!reader.canRead() || reader.peek() != SYNTAX_OPTIONS_KEY_VALUE_SEPARATOR) {
                reader.setCursor(keyStart);
                throw ERROR_EXPECTED_OPTION_VALUE.createWithContext(reader, key);
            }
            reader.skip();
            reader.skipWhitespace();
            suggestions = SUGGEST_NOTHING;
            modifier.handle(this);
            reader.skipWhitespace();
            suggestions = this::suggestOptionsNextOrClose;
            if (!reader.canRead()) {
                break;
            }
            if (reader.peek() == SYNTAX_OPTIONS_SEPARATOR) {
                reader.skip();
                suggestions = this::suggestOptionsKey;
                continue;
            }
            if (reader.peek() != SYNTAX_OPTIONS_END) {
                throw ERROR_EXPECTED_END_OF_OPTIONS.createWithContext(reader);
            }
            break;
        }
        if (!reader.canRead()) {
            throw ERROR_EXPECTED_END_OF_OPTIONS.createWithContext(reader);
        }
        reader.skip();
        suggestions = SUGGEST_NOTHING;
    }

    public boolean shouldInvertValue() {
        reader.skipWhitespace();
        if (reader.canRead() && reader.peek() == SYNTAX_NOT) {
            reader.skip();
            reader.skipWhitespace();
            return true;
        }
        return false;
    }

    public boolean isTag() {
        reader.skipWhitespace();
        if (reader.canRead() && reader.peek() == SYNTAX_TAG) {
            reader.skip();
            reader.skipWhitespace();
            return true;
        }
        return false;
    }

    public StringReader getReader() {
        return reader;
    }

    public void addPredicate(Predicate<Entity> predicate) {
        predicates.add(predicate);
    }

    public void setWorldLimited() {
        worldLimited = true;
    }

    public MinMaxBounds.Doubles getDistance() {
        return distance;
    }

    public void setDistance(MinMaxBounds.Doubles distance) {
        this.distance = distance;
    }

    public MinMaxBounds.Ints getLevel() {
        return level;
    }

    public void setLevel(MinMaxBounds.Ints level) {
        this.level = level;
    }

    public MinMaxBounds.FloatDegrees getRotX() {
        return rotX;
    }

    public void setRotX(MinMaxBounds.FloatDegrees rotX) {
        this.rotX = rotX;
    }

    public MinMaxBounds.FloatDegrees getRotY() {
        return rotY;
    }

    public void setRotY(MinMaxBounds.FloatDegrees rotY) {
        this.rotY = rotY;
    }

    public Double getX() {
        return x;
    }

    public Double getY() {
        return y;
    }

    public Double getZ() {
        return z;
    }

    public void setX(double x) {
        this.x = x;
    }

    public void setY(double y) {
        this.y = y;
    }

    public void setZ(double z) {
        this.z = z;
    }

    public void setDeltaX(double deltaX) {
        this.deltaX = deltaX;
    }

    public void setDeltaY(double deltaY) {
        this.deltaY = deltaY;
    }

    public void setDeltaZ(double deltaZ) {
        this.deltaZ = deltaZ;
    }

    public Double getDeltaX() {
        return deltaX;
    }

    public Double getDeltaY() {
        return deltaY;
    }

    public Double getDeltaZ() {
        return deltaZ;
    }

    public void setMaxResults(int maxResults) {
        this.maxResults = maxResults;
    }

    public void setIncludesEntities(boolean includesEntities) {
        this.includesEntities = includesEntities;
    }

    public BiConsumer<Vec3, List<? extends Entity>> getOrder() {
        return order;
    }

    public void setOrder(BiConsumer<Vec3, List<? extends Entity>> order) {
        this.order = order;
    }

    public EntitySelector parse() throws CommandSyntaxException {
        if (reader.canRead() && reader.peek() == SYNTAX_SELECTOR_START) {
            if (!allowSelectors) {
                throw ERROR_SELECTORS_NOT_ALLOWED.createWithContext(reader);
            }
            reader.skip();
            parseSelector();
        } else {
            parseNameOrUUID();
        }
        return getSelector();
    }

    private CompletableFuture<Suggestions> suggestSelector(SuggestionsBuilder builder, Consumer<SuggestionsBuilder> names) {
        for (String kind : List.of("@p", "@a", "@r", "@s", "@e", "@n")) {
            builder.suggest(kind);
        }
        return builder.buildFuture();
    }

    private CompletableFuture<Suggestions> suggestOpenOptions(SuggestionsBuilder builder, Consumer<SuggestionsBuilder> names) {
        builder.suggest(String.valueOf(SYNTAX_OPTIONS_START));
        return builder.buildFuture();
    }

    private CompletableFuture<Suggestions> suggestOptionsKeyOrClose(SuggestionsBuilder builder, Consumer<SuggestionsBuilder> names) {
        builder.suggest(String.valueOf(SYNTAX_OPTIONS_END));
        EntitySelectorOptions.suggestNames(this, builder);
        return builder.buildFuture();
    }

    private CompletableFuture<Suggestions> suggestOptionsKey(SuggestionsBuilder builder, Consumer<SuggestionsBuilder> names) {
        EntitySelectorOptions.suggestNames(this, builder);
        return builder.buildFuture();
    }

    private CompletableFuture<Suggestions> suggestOptionsNextOrClose(SuggestionsBuilder builder, Consumer<SuggestionsBuilder> names) {
        builder.suggest(String.valueOf(SYNTAX_OPTIONS_SEPARATOR));
        builder.suggest(String.valueOf(SYNTAX_OPTIONS_END));
        return builder.buildFuture();
    }

    public boolean isCurrentEntity() {
        return currentEntity;
    }

    public void setSuggestions(BiFunction<SuggestionsBuilder, Consumer<SuggestionsBuilder>, CompletableFuture<Suggestions>> suggestions) {
        this.suggestions = suggestions;
    }

    public CompletableFuture<Suggestions> fillSuggestions(SuggestionsBuilder builder, Consumer<SuggestionsBuilder> names) {
        return suggestions.apply(builder.createOffset(reader.getCursor()), names);
    }

    public InvertableSetOptionState nameOption() {
        return nameOption;
    }

    public SetOnceOptionState limitedOption() {
        return limitedOption;
    }

    public SetOnceOptionState sortedOption() {
        return sortedOption;
    }

    public InvertableSetOptionState gamemodeOption() {
        return gamemodeOption;
    }

    public InvertableSetOptionState teamOption() {
        return teamOption;
    }

    public void limitToType(EntityType<?> type) {
        this.type = type;
    }

    public InvertableSetOptionState typeOption() {
        return typeOption;
    }

    public SetOnceOptionState scoresOption() {
        return scoresOption;
    }

    public SetOnceOptionState advancementsOption() {
        return advancementsOption;
    }
}
