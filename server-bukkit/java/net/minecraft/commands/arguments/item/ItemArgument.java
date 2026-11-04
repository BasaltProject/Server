// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.item;

import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.Item;

public class ItemArgument implements ArgumentType<ItemInput> {
    private static final Collection<String> EXAMPLES = Arrays.asList("stick", "minecraft:stick");
    private static final DynamicCommandExceptionType ERROR_UNKNOWN_ITEM = new DynamicCommandExceptionType(id -> new LiteralMessage("Unknown item '" + id + "'"));
    private static final SimpleCommandExceptionType ERROR_COMPONENTS = new SimpleCommandExceptionType(new LiteralMessage("Item components in commands are not supported by the Basalt facade"));

    private static final DynamicCommandExceptionType ERROR_UNKNOWN_COMPONENT = new DynamicCommandExceptionType(id -> new LiteralMessage("Unknown item component '" + id + "'"));
    private static final DynamicCommandExceptionType ERROR_MALFORMED_COMPONENT = new DynamicCommandExceptionType(message -> new LiteralMessage(String.valueOf(message)));
    private static final SimpleCommandExceptionType ERROR_EXPECTED_COMPONENT_VALUE = new SimpleCommandExceptionType(new LiteralMessage("Expected '=' after the item component"));
    private final CommandBuildContext context;

    public ItemArgument(CommandBuildContext context) {
        this.context = context;
    }

    public static ItemArgument item(CommandBuildContext context) {
        return new ItemArgument(context);
    }

    public ItemInput parse(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        Identifier id = Identifier.read(reader);
        Holder<Item> item = BuiltInRegistries.ITEM.get(id).map(found -> (Holder<Item>) found).orElse(null);
        if (item == null) {
            reader.setCursor(start);
            throw ERROR_UNKNOWN_ITEM.createWithContext(reader, id);
        }
        DataComponentPatch components = DataComponentPatch.EMPTY;
        if (reader.canRead() && reader.peek() == '[') {
            components = readComponents(reader);
        }
        return new ItemInput(item, components);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private DataComponentPatch readComponents(StringReader reader) throws CommandSyntaxException {
        DataComponentPatch.Builder patch = DataComponentPatch.builder();
        var ops = context.createSerializationContext(net.minecraft.nbt.NbtOps.INSTANCE);
        var parser = net.minecraft.nbt.TagParser.create(net.minecraft.nbt.NbtOps.INSTANCE);
        reader.expect('[');
        reader.skipWhitespace();
        while (reader.canRead() && reader.peek() != ']') {
            boolean removal = reader.peek() == '!';
            if (removal) {
                reader.skip();
            }
            int start = reader.getCursor();
            Identifier id = Identifier.read(reader);
            DataComponentType type = org.basaltmc.bukkit.facade.ComponentTypes.byId(id);
            if (type == null) {
                reader.setCursor(start);
                throw ERROR_UNKNOWN_COMPONENT.createWithContext(reader, id);
            }
            if (removal) {
                patch.remove(type);
            } else {
                reader.skipWhitespace();
                if (!reader.canRead() || reader.peek() != '=') {
                    throw ERROR_EXPECTED_COMPONENT_VALUE.createWithContext(reader);
                }
                reader.skip();
                reader.skipWhitespace();
                net.minecraft.nbt.Tag tag = parser.parseAsArgument(reader);
                com.mojang.serialization.DataResult<Object> parsed = type.codec().parse(ops, tag);
                if (parsed.error().isPresent()) {
                    throw ERROR_MALFORMED_COMPONENT.createWithContext(reader, parsed.error().get().message());
                }
                patch.set(type, parsed.result().get());
            }
            reader.skipWhitespace();
            if (!reader.canRead() || reader.peek() != ',') {
                break;
            }
            reader.skip();
            reader.skipWhitespace();
        }
        reader.expect(']');
        return patch.build();
    }

    public static <S> ItemInput getItem(CommandContext<S> context, String name) {
        return context.getArgument(name, ItemInput.class);
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        String remaining = builder.getRemaining();
        for (Identifier id : BuiltInRegistries.ITEM.keySet()) {
            if (id.toString().startsWith(remaining) || id.getPath().startsWith(remaining)) {
                builder.suggest(id.toString());
            }
        }
        return builder.buildFuture();
    }

    public Collection<String> getExamples() {
        return EXAMPLES;
    }
}
