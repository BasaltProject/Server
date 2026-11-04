// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution;
import com.mojang.brigadier.Command;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.context.ContextChain;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.commands.ExecutionCommandSource;
public interface CustomCommandExecutor<T> {
    void run(T sender, ContextChain<T> currentStep, ChainModifiers modifiers, ExecutionControl<T> output);
    public interface CommandAdapter<T> extends CustomCommandExecutor<T>, Command<T> {
        default int run(CommandContext<T> context) throws CommandSyntaxException {
            throw new UnsupportedOperationException("This function should not run");
        }
    }
    public abstract static class WithErrorHandling<T extends ExecutionCommandSource<T>> implements CustomCommandExecutor<T> {
        public final void run(T sender, ContextChain<T> currentStep, ChainModifiers modifiers, ExecutionControl<T> output) {
            try {
                runGuarded(sender, currentStep, modifiers, output);
            } catch (CommandSyntaxException e) {
                onError(e, sender, modifiers, output.tracer());
                sender.callback().onFailure();
            }
        }
        protected void onError(CommandSyntaxException e, T sender, ChainModifiers modifiers, TraceCallbacks tracer) {
            sender.handleError(e, modifiers.isForked(), tracer);
        }
        protected abstract void runGuarded(T sender, ContextChain<T> currentStep, ChainModifiers modifiers, ExecutionControl<T> output) throws CommandSyntaxException;
    }
}
