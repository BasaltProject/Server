// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.network.chat.Component;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.SnbtGrammar;
import net.minecraft.nbt.Tag;
import net.minecraft.util.parsing.packrat.commands.CommandArgumentParser;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import java.util.Collection;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.HolderLookup;
import net.minecraft.network.chat.Style;
import net.minecraft.util.parsing.packrat.commands.ParserBasedArgument;
public class StyleArgument extends ParserBasedArgument<Style> {
    public static final DynamicCommandExceptionType ERROR_INVALID_STYLE = new DynamicCommandExceptionType(message -> Component.translatableEscape("argument.style.invalid", message));
    private static final Collection<String> kExamples = java.util.List.of("{bold: true}", "{color: 'red'}", "{}");
    private static final CommandArgumentParser<Tag> kTags = SnbtGrammar.<Tag>createParser(NbtOps.INSTANCE);
    private StyleArgument(HolderLookup.Provider registries) {
        super(kTags.withCodec(registries.createSerializationContext(NbtOps.INSTANCE), kTags, Style.Serializer.CODEC, ERROR_INVALID_STYLE));
    }
    public static Style getStyle(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Style.class);
    }
    public static StyleArgument style(CommandBuildContext context) {
        return new StyleArgument(context);
    }
    public Collection<String> getExamples() {
        return kExamples;
    }
}
