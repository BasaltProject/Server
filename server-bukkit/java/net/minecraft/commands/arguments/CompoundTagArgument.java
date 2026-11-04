// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.Collection;
import net.minecraft.nbt.CompoundTag;
public class CompoundTagArgument implements ArgumentType<CompoundTag> {
    private CompoundTagArgument() {
        
    }
    public static CompoundTagArgument compoundTag() {
        return new CompoundTagArgument();
    }
    public static <S> CompoundTag getCompoundTag(CommandContext<S> context, String name) {
        return context.getArgument(name, CompoundTag.class);
    }
    public CompoundTag parse(StringReader reader) throws CommandSyntaxException {
        return net.minecraft.nbt.TagParser.parseCompoundAsArgument(reader);
    }
    public Collection<String> getExamples() {
        return java.util.List.of("{}", "{foo=bar}");
    }
}
