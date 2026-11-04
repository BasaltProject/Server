// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution.tasks;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.context.ContextChain;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.ChainModifiers;
import net.minecraft.commands.execution.ExecutionContext;
import net.minecraft.commands.execution.Frame;
import net.minecraft.commands.execution.TraceCallbacks;
import net.minecraft.commands.execution.UnboundEntryAction;
public class ExecuteCommand<T extends ExecutionCommandSource<T>> implements UnboundEntryAction<T> {
    private final String commandInput;
    private final ChainModifiers modifiers;
    private final CommandContext<T> executionContext;
    public ExecuteCommand(String commandInput, ChainModifiers modifiers, CommandContext<T> executionContext) {
        this.commandInput = commandInput;
        this.modifiers = modifiers;
        this.executionContext = executionContext;
    }
    public void execute(T sender, ExecutionContext<T> context, Frame frame) {
        context.profiler().push(() -> "execute " + commandInput);
        try {
            context.incrementCost();
            int result = ContextChain.runExecutable(executionContext, sender, ExecutionCommandSource.resultConsumer(), modifiers.isForked());
            TraceCallbacks tracer = context.tracer();
            if (tracer != null) {
                tracer.onReturn(frame.depth(), commandInput, result);
            }
        } catch (CommandSyntaxException e) {
            sender.handleError(e, modifiers.isForked(), context.tracer());
        } finally {
            context.profiler().pop();
        }
    }
}
