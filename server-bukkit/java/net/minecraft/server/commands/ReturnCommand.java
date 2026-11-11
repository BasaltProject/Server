// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.commands.execution.Frame;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.context.ContextChain;
import java.util.List;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.ChainModifiers;
import net.minecraft.commands.execution.CustomCommandExecutor;
import net.minecraft.commands.execution.CustomModifierExecutor;
import net.minecraft.commands.execution.ExecutionControl;
public class ReturnCommand {
    public static <T extends ExecutionCommandSource<T>> void register(CommandDispatcher<T> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate((CommandDispatcher<net.minecraft.commands.CommandSourceStack>) (CommandDispatcher<?>) dispatcher, "return");
    }
    static class ReturnValueCustomExecutor<T extends ExecutionCommandSource<T>> implements CustomCommandExecutor.CommandAdapter<T> {
        public void run(T sender, ContextChain<T> currentStep, ChainModifiers modifiers, ExecutionControl<T> output) {
            int value = com.mojang.brigadier.arguments.IntegerArgumentType.getInteger(currentStep.getTopContext(), "value");
            sender.callback().onSuccess(value);
            Frame frame = output.currentFrame();
            frame.returnSuccess(value);
            frame.discard();
        }
    }
    static class ReturnFailCustomExecutor<T extends ExecutionCommandSource<T>> implements CustomCommandExecutor.CommandAdapter<T> {
        public void run(T sender, ContextChain<T> currentStep, ChainModifiers modifiers, ExecutionControl<T> output) {
            sender.callback().onFailure();
            Frame frame = output.currentFrame();
            frame.returnFailure();
            frame.discard();
        }
    }
    static class ReturnFromCommandCustomModifier<T extends ExecutionCommandSource<T>> implements CustomModifierExecutor.ModifierAdapter<T> {
        public void apply(T originalSource, List<T> currentSources, ContextChain<T> currentStep, ChainModifiers modifiers, ExecutionControl<T> output) {
            if (currentSources.isEmpty()) {
                if (modifiers.isReturn()) {
                    output.queueNext(net.minecraft.commands.execution.tasks.FallthroughTask.instance());
                }
                return;
            }
            output.currentFrame().discard();
            ContextChain<T> next = currentStep.nextStage();
            output.queueNext(new net.minecraft.commands.execution.tasks.BuildContexts.Continuation<>(next.getTopContext().getInput(), next,
                modifiers.setReturn(), originalSource, currentSources));
        }
    }
}
