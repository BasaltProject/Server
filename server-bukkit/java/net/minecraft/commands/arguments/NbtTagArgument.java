// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.SnbtGrammar;
import net.minecraft.nbt.Tag;
import com.mojang.brigadier.context.CommandContext;
import java.util.Collection;
import net.minecraft.nbt.Tag;
import net.minecraft.util.parsing.packrat.commands.ParserBasedArgument;
public class NbtTagArgument extends ParserBasedArgument<Tag> {
    private static final Collection<String> kExamples = java.util.List.of("0", "0b", "0l", "0.0", "\"foo\"", "{foo=bar}", "[0]");

    private NbtTagArgument() {
        super(SnbtGrammar.<Tag>createParser(NbtOps.INSTANCE));
    }
    public static NbtTagArgument nbtTag() {
        return new NbtTagArgument();
    }
    public static <S> Tag getNbtTag(CommandContext<S> context, String name) {
        return context.getArgument(name, Tag.class);
    }
    public Collection<String> getExamples() {
        return kExamples;
    }
}
