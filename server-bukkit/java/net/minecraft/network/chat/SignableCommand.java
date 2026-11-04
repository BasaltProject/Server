// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import com.mojang.brigadier.ParseResults;
import com.mojang.brigadier.context.CommandContextBuilder;
import com.mojang.brigadier.context.ParsedArgument;
import com.mojang.brigadier.tree.ArgumentCommandNode;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.commands.ArgumentVisitor;
import net.minecraft.commands.arguments.SignedArgument;
public record SignableCommand<S>(List<Argument<S>> arguments) {
    public static <S> boolean hasSignableArguments(ParseResults<S> command) {
        return !of(command).arguments().isEmpty();
    }
    public static <S> SignableCommand<S> of(ParseResults<S> command) {
        String commandString = command.getReader().getString();
        List<Argument<S>> arguments = new ArrayList<>();
        ArgumentVisitor.visitArguments(command, new ArgumentVisitor.Output<S>() {
            public <T> void accept(CommandContextBuilder<S> context, ArgumentCommandNode<S, T> argument, ParsedArgument<S, T> value) {
                if (value != null && argument.getType() instanceof SignedArgument) {
                    arguments.add(new Argument<>(argument, value.getRange().get(commandString)));
                }
            }
        }, true);
        return new SignableCommand<>(arguments);
    }
    public Argument<S> getArgument(String name) {
        for (Argument<S> argument : arguments) {
            if (name.equals(argument.name())) {
                return argument;
            }
        }
        return null;
    }
    public static record Argument<S>(ArgumentCommandNode<S, ?> node, String value) {
        public String name() {
            return node.getName();
        }
    }
}
