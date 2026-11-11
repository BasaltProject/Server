// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.SnbtGrammar;
import net.minecraft.nbt.Tag;
import net.minecraft.util.parsing.packrat.commands.CommandArgumentParser;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.ComponentUtils;
import net.minecraft.network.chat.ResolutionContext;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import java.util.Collection;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.HolderLookup;
import net.minecraft.network.chat.Component;
import net.minecraft.util.parsing.packrat.commands.ParserBasedArgument;
import net.minecraft.world.entity.Entity;
public class ComponentArgument extends ParserBasedArgument<Component> {
    public static final DynamicCommandExceptionType ERROR_INVALID_COMPONENT = new DynamicCommandExceptionType(message -> Component.translatableEscape("argument.component.invalid", message));
    private static final Collection<String> kExamples = java.util.List.of("\"hello world\"", "'hello world'", "\"\"", "{text:\"hello world\"}", "[\"\"]");
    private static final CommandArgumentParser<Tag> kTags = SnbtGrammar.<Tag>createParser(NbtOps.INSTANCE);
    private ComponentArgument(HolderLookup.Provider registries) {
        super(kTags.withCodec(registries.createSerializationContext(NbtOps.INSTANCE), kTags, ComponentSerialization.CODEC, ERROR_INVALID_COMPONENT));
    }
    public static Component getRawComponent(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Component.class);
    }
    public static Component getResolvedComponent(CommandContext<CommandSourceStack> context, String name, Entity contentEntity) throws CommandSyntaxException {
        return ComponentUtils.resolve(ResolutionContext.builder().withSource(context.getSource()).withEntityOverride(contentEntity).build(), getRawComponent(context, name));
    }
    public static Component getResolvedComponent(CommandContext<CommandSourceStack> context, String name) throws CommandSyntaxException {
        return ComponentUtils.resolve(ResolutionContext.create(context.getSource()), getRawComponent(context, name));
    }
    public static ComponentArgument textComponent(CommandBuildContext context) {
        return new ComponentArgument(context);
    }
    public Collection<String> getExamples() {
        return kExamples;
    }
}
