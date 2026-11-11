// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.selector.options;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;
import java.util.function.Predicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.commands.arguments.selector.EntitySelectorParser;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.nbt.TagParser;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.level.GameType;

public class EntitySelectorOptions {
    public static final DynamicCommandExceptionType ERROR_UNKNOWN_OPTION =
        new DynamicCommandExceptionType(key -> Component.translatableEscape("argument.entity.options.unknown", key));
    public static final DynamicCommandExceptionType ERROR_INAPPLICABLE_OPTION =
        new DynamicCommandExceptionType(key -> Component.translatableEscape("argument.entity.options.inapplicable", key));
    public static final SimpleCommandExceptionType ERROR_RANGE_NEGATIVE =
        new SimpleCommandExceptionType(Component.translatable("argument.entity.options.distance.negative"));
    public static final SimpleCommandExceptionType ERROR_LEVEL_NEGATIVE =
        new SimpleCommandExceptionType(Component.translatable("argument.entity.options.level.negative"));
    public static final SimpleCommandExceptionType ERROR_LIMIT_TOO_SMALL =
        new SimpleCommandExceptionType(Component.translatable("argument.entity.options.limit.toosmall"));
    public static final DynamicCommandExceptionType ERROR_SORT_UNKNOWN =
        new DynamicCommandExceptionType(sort -> Component.translatableEscape("argument.entity.options.sort.irreversible", sort));
    public static final DynamicCommandExceptionType ERROR_GAME_MODE_INVALID =
        new DynamicCommandExceptionType(mode -> Component.translatableEscape("argument.entity.options.mode.invalid", mode));
    public static final DynamicCommandExceptionType ERROR_ENTITY_TYPE_INVALID =
        new DynamicCommandExceptionType(type -> Component.translatableEscape("argument.entity.options.type.invalid", type));
    private static final Map<String, Option> kOptions = new TreeMap<>();

    public static synchronized void bootStrap() {
        if (!kOptions.isEmpty()) {
            return;
        }
        add("name", parser -> {
            boolean negated = parser.shouldInvertValue();
            parser.getReader().readString();
            parser.nameOption().markParsedElement(negated);
        }, parser -> parser.nameOption().canParseAny(), "argument.entity.options.name.description");
        add("distance", parser -> {
            MinMaxBounds.Doubles range = MinMaxBounds.Doubles.fromReader(parser.getReader());
            if (nonNegative(range.min()) && nonNegative(range.max())) {
                parser.setDistance(range);
                parser.setWorldLimited();
                return;
            }
            throw ERROR_RANGE_NEGATIVE.createWithContext(parser.getReader());
        }, parser -> parser.getDistance().isAny(), "argument.entity.options.distance.description");
        add("level", parser -> {
            MinMaxBounds.Ints range = MinMaxBounds.Ints.fromReader(parser.getReader());
            if (nonNegative(range.min()) && nonNegative(range.max())) {
                parser.setLevel(range);
                parser.setIncludesEntities(false);
                return;
            }
            throw ERROR_LEVEL_NEGATIVE.createWithContext(parser.getReader());
        }, parser -> parser.getLevel().isAny(), "argument.entity.options.level.description");
        add("x", parser -> { parser.setWorldLimited(); parser.setX(parser.getReader().readDouble()); },
            parser -> parser.getX() == null, "argument.entity.options.x.description");
        add("y", parser -> { parser.setWorldLimited(); parser.setY(parser.getReader().readDouble()); },
            parser -> parser.getY() == null, "argument.entity.options.y.description");
        add("z", parser -> { parser.setWorldLimited(); parser.setZ(parser.getReader().readDouble()); },
            parser -> parser.getZ() == null, "argument.entity.options.z.description");
        add("dx", parser -> { parser.setWorldLimited(); parser.setDeltaX(parser.getReader().readDouble()); },
            parser -> parser.getDeltaX() == null, "argument.entity.options.dx.description");
        add("dy", parser -> { parser.setWorldLimited(); parser.setDeltaY(parser.getReader().readDouble()); },
            parser -> parser.getDeltaY() == null, "argument.entity.options.dy.description");
        add("dz", parser -> { parser.setWorldLimited(); parser.setDeltaZ(parser.getReader().readDouble()); },
            parser -> parser.getDeltaZ() == null, "argument.entity.options.dz.description");
        add("x_rotation", parser -> parser.setRotX(MinMaxBounds.FloatDegrees.fromReader(parser.getReader())),
            parser -> parser.getRotX().isAny(), "argument.entity.options.x_rotation.description");
        add("y_rotation", parser -> parser.setRotY(MinMaxBounds.FloatDegrees.fromReader(parser.getReader())),
            parser -> parser.getRotY().isAny(), "argument.entity.options.y_rotation.description");
        add("limit", parser -> {
            int from = parser.getReader().getCursor();
            int limit = parser.getReader().readInt();
            if (limit < 1) {
                parser.getReader().setCursor(from);
                throw ERROR_LIMIT_TOO_SMALL.createWithContext(parser.getReader());
            }
            parser.setMaxResults(limit);
            parser.limitedOption().markParsed();
        }, parser -> !parser.isCurrentEntity() && parser.limitedOption().canParse(), "argument.entity.options.limit.description");
        add("sort", parser -> {
            int from = parser.getReader().getCursor();
            String sort = parser.getReader().readUnquotedString();
            parser.setOrder(switch (sort) {
                case "nearest" -> EntitySelectorParser.ORDER_NEAREST;
                case "furthest" -> EntitySelectorParser.ORDER_FURTHEST;
                case "random" -> EntitySelectorParser.ORDER_RANDOM;
                case "arbitrary" -> net.minecraft.commands.arguments.selector.EntitySelector.ORDER_ARBITRARY;
                default -> {
                    parser.getReader().setCursor(from);
                    throw ERROR_SORT_UNKNOWN.createWithContext(parser.getReader(), sort);
                }
            });
            parser.sortedOption().markParsed();
        }, parser -> !parser.isCurrentEntity() && parser.sortedOption().canParse(), "argument.entity.options.sort.description");
        add("gamemode", parser -> {
            boolean negated = parser.shouldInvertValue();
            int from = parser.getReader().getCursor();
            String mode = parser.getReader().readUnquotedString();
            if (GameType.byName(mode, null) == null) {
                parser.getReader().setCursor(from);
                throw ERROR_GAME_MODE_INVALID.createWithContext(parser.getReader(), mode);
            }
            parser.setIncludesEntities(false);
            parser.gamemodeOption().markParsedElement(negated);
        }, parser -> parser.gamemodeOption().canParseAny(), "argument.entity.options.gamemode.description");
        add("team", parser -> {
            boolean negated = parser.shouldInvertValue();
            parser.getReader().readUnquotedString();
            parser.teamOption().markParsedElement(negated);
        }, parser -> parser.teamOption().canParseAny(), "argument.entity.options.team.description");
        add("type", parser -> {
            parser.setSuggestions((builder, names) -> {
                net.minecraft.commands.SharedSuggestionProvider.suggestResource(BuiltInRegistries.ENTITY_TYPE.keySet(), builder, String.valueOf('!'));
                return builder.buildFuture();
            });
            int from = parser.getReader().getCursor();
            boolean negated = parser.shouldInvertValue();
            if (parser.isTag()) {
                Identifier.read(parser.getReader());
                parser.typeOption().markParsedTag(Identifier.parse("minecraft:tag"));
                return;
            }
            Identifier typeId = Identifier.read(parser.getReader());
            EntityType<?> type = BuiltInRegistries.ENTITY_TYPE.getOptional(typeId).orElse(null);
            if (type == null) {
                parser.getReader().setCursor(from);
                throw ERROR_ENTITY_TYPE_INVALID.createWithContext(parser.getReader(), typeId.toString());
            }
            if (type == net.minecraft.world.entity.EntityTypes.PLAYER && !negated) {
                parser.setIncludesEntities(false);
            }
            parser.typeOption().markParsedElement(negated);
            if (!negated) {
                parser.limitToType(type);
            }
        }, parser -> parser.typeOption().canParseAny(), "argument.entity.options.type.description");
        add("tag", parser -> {
            parser.shouldInvertValue();
            parser.getReader().readUnquotedString();
        }, parser -> true, "argument.entity.options.tag.description");
        add("nbt", parser -> {
            parser.shouldInvertValue();
            TagParser.parseCompoundAsArgument(parser.getReader());
        }, parser -> true, "argument.entity.options.nbt.description");
        add("scores", parser -> {
            skipBraced(parser.getReader());
            parser.scoresOption().markParsed();
        }, parser -> parser.scoresOption().canParse(), "argument.entity.options.scores.description");
        add("advancements", parser -> {
            skipBraced(parser.getReader());
            parser.advancementsOption().markParsed();
            parser.setIncludesEntities(false);
        }, parser -> parser.advancementsOption().canParse(), "argument.entity.options.advancements.description");
        add("predicate", parser -> {
            parser.shouldInvertValue();
            Identifier.read(parser.getReader());
        }, parser -> true, "argument.entity.options.predicate.description");
    }

    private static boolean nonNegative(java.util.Optional<? extends Number> bound) {
        return bound.isEmpty() || bound.get().doubleValue() >= 0.0D;
    }

    private static void skipBraced(StringReader reader) throws CommandSyntaxException {
        reader.expect('{');
        int depth = 1;
        while (reader.canRead() && depth > 0) {
            char c = reader.read();
            if (c == '"' || c == '\'') {
                reader.setCursor(reader.getCursor() - 1);
                reader.readQuotedString();
            } else if (c == '{') {
                depth++;
            } else if (c == '}') {
                depth--;
            }
        }
        if (depth > 0) {
            throw com.mojang.brigadier.exceptions.CommandSyntaxException.BUILT_IN_EXCEPTIONS.readerExpectedSymbol().createWithContext(reader, "}");
        }
    }

    private static void add(String key, Modifier modifier, Predicate<EntitySelectorParser> canUse, String description) {
        kOptions.put(key, new Option(modifier, canUse, Component.translatable(description)));
    }

    public static Modifier get(EntitySelectorParser parser, String key, int start) throws CommandSyntaxException {
        Option option = kOptions.get(key);
        if (option == null) {
            parser.getReader().setCursor(start);
            throw ERROR_UNKNOWN_OPTION.createWithContext(parser.getReader(), key);
        }
        if (!option.canUse().test(parser)) {
            parser.getReader().setCursor(start);
            throw ERROR_INAPPLICABLE_OPTION.createWithContext(parser.getReader(), key);
        }
        return option.modifier();
    }

    public static void suggestNames(EntitySelectorParser parser, SuggestionsBuilder builder) {
        String prefix = builder.getRemaining().toLowerCase(Locale.ROOT);
        kOptions.forEach((key, option) -> {
            if (option.canUse().test(parser) && key.toLowerCase(Locale.ROOT).startsWith(prefix)) {
                builder.suggest(key + "=", option.description());
            }
        });
    }

    static record Option(Modifier modifier, Predicate<EntitySelectorParser> canUse, Component description) {
    }

    public interface Modifier {
        void handle(EntitySelectorParser parser) throws CommandSyntaxException;
    }
}
