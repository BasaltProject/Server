// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import com.mojang.brigadier.ParseResults;
import com.mojang.brigadier.context.CommandContextBuilder;
import com.mojang.brigadier.context.ParsedArgument;
import com.mojang.brigadier.context.ParsedCommandNode;
import com.mojang.brigadier.tree.ArgumentCommandNode;
import com.mojang.brigadier.tree.CommandNode;
import java.util.Map;
public class ArgumentVisitor {
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <S> void visitArguments(ParseResults<S> command, Output<S> output, boolean rejectRootRedirects) {
        CommandContextBuilder<S> rootContext = command.getContext();
        CommandContextBuilder<S> context = rootContext;
        visitNodeArguments(rootContext, output);
        CommandContextBuilder<S> child;
        while ((child = context.getChild()) != null && (!rejectRootRedirects || child.getRootNode() != rootContext.getRootNode())) {
            visitNodeArguments(child, output);
            context = child;
        }
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <S> void visitNodeArguments(CommandContextBuilder<S> context, Output<S> output) {
        Map<String, ParsedArgument<S, ?>> values = context.getArguments();
        for (ParsedCommandNode<S> node : context.getNodes()) {
            CommandNode<S> candidate = node.getNode();
            if (candidate instanceof ArgumentCommandNode<S, ?> argument) {
                output.accept(context, (ArgumentCommandNode) argument, (ParsedArgument) values.get(argument.getName()));
            }
        }
    }
    @FunctionalInterface
    public interface Output<S> {
        <T> void accept(CommandContextBuilder<S> context, ArgumentCommandNode<S, T> argument, ParsedArgument<S, T> value);
    }
}
