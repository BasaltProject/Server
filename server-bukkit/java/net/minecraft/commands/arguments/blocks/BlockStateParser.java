// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.blocks;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.exceptions.Dynamic3CommandExceptionType;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import com.mojang.datafixers.util.Either;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderSet;
import net.minecraft.core.registries.Registries;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.TagParser;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;

public class BlockStateParser {
    public static final SimpleCommandExceptionType ERROR_NO_TAGS_ALLOWED = new SimpleCommandExceptionType(Component.translatable("argument.block.tag.disallowed"));
    public static final DynamicCommandExceptionType ERROR_UNKNOWN_BLOCK = new DynamicCommandExceptionType(block -> Component.translatableEscape("argument.block.id.invalid", block));
    public static final Dynamic2CommandExceptionType ERROR_UNKNOWN_PROPERTY = new Dynamic2CommandExceptionType(
        (block, property) -> Component.translatableEscape("argument.block.property.unknown", block, property));
    public static final Dynamic2CommandExceptionType ERROR_DUPLICATE_PROPERTY = new Dynamic2CommandExceptionType(
        (block, property) -> Component.translatableEscape("argument.block.property.duplicate", property, block));
    public static final Dynamic3CommandExceptionType ERROR_INVALID_VALUE = new Dynamic3CommandExceptionType(
        (block, property, value) -> Component.translatableEscape("argument.block.property.invalid", block, value, property));
    public static final Dynamic2CommandExceptionType ERROR_EXPECTED_VALUE = new Dynamic2CommandExceptionType(
        (block, property) -> Component.translatableEscape("argument.block.property.novalue", block, property));
    public static final SimpleCommandExceptionType ERROR_EXPECTED_END_OF_PROPERTIES = new SimpleCommandExceptionType(Component.translatable("argument.block.property.unclosed"));
    public static final DynamicCommandExceptionType ERROR_UNKNOWN_TAG = new DynamicCommandExceptionType(tag -> Component.translatableEscape("arguments.block.tag.unknown", tag));

    private final HolderLookup<Block> blocks;
    private final StringReader reader;
    private final boolean forTesting;
    private final boolean allowNbt;
    private final Map<Property<?>, Comparable<?>> properties = new LinkedHashMap<>();
    private final Map<String, String> vagueProperties = new HashMap<>();
    private BlockState state;
    private HolderSet<Block> tag;
    private CompoundTag nbt;

    private BlockStateParser(HolderLookup<Block> blocks, StringReader reader, boolean forTesting, boolean allowNbt) {
        this.blocks = blocks;
        this.reader = reader;
        this.forTesting = forTesting;
        this.allowNbt = allowNbt;
    }

    public static BlockResult parseForBlock(HolderLookup<Block> blocks, String value, boolean allowNbt) throws CommandSyntaxException {
        return parseForBlock(blocks, new StringReader(value), allowNbt);
    }

    public static BlockResult parseForBlock(HolderLookup<Block> blocks, StringReader reader, boolean allowNbt) throws CommandSyntaxException {
        BlockStateParser parser = new BlockStateParser(blocks, reader, false, allowNbt);
        parser.parse();
        return new BlockResult(parser.state, parser.properties, parser.nbt);
    }

    public static Either<BlockResult, TagResult> parseForTesting(HolderLookup<Block> blocks, String value, boolean allowNbt) throws CommandSyntaxException {
        return parseForTesting(blocks, new StringReader(value), allowNbt);
    }

    public static Either<BlockResult, TagResult> parseForTesting(HolderLookup<Block> blocks, StringReader reader, boolean allowNbt) throws CommandSyntaxException {
        BlockStateParser parser = new BlockStateParser(blocks, reader, true, allowNbt);
        parser.parse();
        return parser.tag != null ? Either.right(new TagResult(parser.tag, parser.vagueProperties, parser.nbt))
            : Either.left(new BlockResult(parser.state, parser.properties, parser.nbt));
    }

    public static CompletableFuture<Suggestions> fillSuggestions(HolderLookup<Block> blocks, SuggestionsBuilder builder, boolean forTesting, boolean allowNbt) {
        return builder.buildFuture();
    }

    public static String serialize(BlockState state) {
        StringBuilder text = new StringBuilder(state.getBlock().blockKey().toString());
        if (!state.getProperties().isEmpty()) {
            text.append('[');
            boolean first = true;
            for (Property<?> property : state.getProperties()) {
                if (!first) {
                    text.append(',');
                }
                first = false;
                appendProperty(text, state, property);
            }
            text.append(']');
        }
        return text.toString();
    }

    private static <T extends Comparable<T>> void appendProperty(StringBuilder text, BlockState state, Property<T> property) {
        text.append(property.getName()).append('=').append(property.getName(state.getValue(property)));
    }

    private void parse() throws CommandSyntaxException {
        if (forTesting && reader.canRead() && reader.peek() == '#') {
            reader.skip();
            Identifier tagId = Identifier.read(reader);
            tag = blocks.get(TagKey.create(Registries.BLOCK, tagId)).orElseThrow(() -> {
                reader.setCursor(reader.getCursor());
                return ERROR_UNKNOWN_TAG.createWithContext(reader, tagId);
            });
            if (reader.canRead() && reader.peek() == '[') {
                readVagueProperties();
            }
        } else {
            int start = reader.getCursor();
            Identifier blockId = Identifier.read(reader);
            net.minecraft.core.Holder.Reference<Block> block = blocks.get(ResourceKey.create(Registries.BLOCK, blockId)).orElseThrow(() -> {
                reader.setCursor(start);
                return ERROR_UNKNOWN_BLOCK.createWithContext(reader, blockId.toString());
            });
            state = block.value().defaultBlockState();
            if (reader.canRead() && reader.peek() == '[') {
                readProperties(blockId);
            }
        }
        if (allowNbt && reader.canRead() && reader.peek() == '{') {
            nbt = TagParser.parseCompoundAsArgument(reader);
        }
    }

    private void readVagueProperties() throws CommandSyntaxException {
        reader.skip();
        reader.skipWhitespace();
        while (reader.canRead() && reader.peek() != ']') {
            String name = reader.readString();
            if (vagueProperties.containsKey(name)) {
                throw ERROR_DUPLICATE_PROPERTY.createWithContext(reader, tag.toString(), name);
            }
            reader.skipWhitespace();
            if (!reader.canRead() || reader.peek() != '=') {
                throw ERROR_EXPECTED_VALUE.createWithContext(reader, tag.toString(), name);
            }
            reader.skip();
            reader.skipWhitespace();
            vagueProperties.put(name, reader.readString());
            reader.skipWhitespace();
            if (reader.canRead() && reader.peek() == ',') {
                reader.skip();
                reader.skipWhitespace();
            }
        }
        if (!reader.canRead()) {
            throw ERROR_EXPECTED_END_OF_PROPERTIES.createWithContext(reader);
        }
        reader.skip();
    }

    private void readProperties(Identifier blockId) throws CommandSyntaxException {
        reader.skip();
        reader.skipWhitespace();
        while (reader.canRead() && reader.peek() != ']') {
            int start = reader.getCursor();
            String name = reader.readString();
            Property<?> property = state.getBlock().getStateDefinition().getProperty(name);
            if (property == null) {
                reader.setCursor(start);
                throw ERROR_UNKNOWN_PROPERTY.createWithContext(reader, blockId.toString(), name);
            }
            if (properties.containsKey(property)) {
                reader.setCursor(start);
                throw ERROR_DUPLICATE_PROPERTY.createWithContext(reader, blockId.toString(), name);
            }
            reader.skipWhitespace();
            if (!reader.canRead() || reader.peek() != '=') {
                throw ERROR_EXPECTED_VALUE.createWithContext(reader, blockId.toString(), name);
            }
            reader.skip();
            reader.skipWhitespace();
            int valueStart = reader.getCursor();
            String value = reader.readString();
            apply(property, value, valueStart, blockId);
            reader.skipWhitespace();
            if (reader.canRead() && reader.peek() == ',') {
                reader.skip();
                reader.skipWhitespace();
            }
        }
        if (!reader.canRead()) {
            throw ERROR_EXPECTED_END_OF_PROPERTIES.createWithContext(reader);
        }
        reader.skip();
    }

    private <T extends Comparable<T>> void apply(Property<T> property, String text, int valueStart, Identifier blockId) throws CommandSyntaxException {
        Optional<T> value = property.getValue(text);
        if (value.isEmpty()) {
            reader.setCursor(valueStart);
            throw ERROR_INVALID_VALUE.createWithContext(reader, blockId.toString(), property.getName(), text);
        }
        state = state.setValue(property, value.get());
        properties.put(property, value.get());
    }

    public static record BlockResult(BlockState blockState, Map<Property<?>, Comparable<?>> properties, CompoundTag nbt) {}

    public static record TagResult(HolderSet<Block> tag, Map<String, String> vagueProperties, CompoundTag nbt) {}
}
