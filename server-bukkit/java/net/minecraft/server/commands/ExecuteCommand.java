// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import java.util.ArrayList;
import net.minecraft.commands.CommandResultCallback;
import net.minecraft.commands.FunctionInstantiationException;
import net.minecraft.commands.arguments.item.FunctionArgument;
import net.minecraft.commands.execution.tasks.IsolatedCall;
import net.minecraft.commands.execution.tasks.CallFunction;
import net.minecraft.commands.execution.tasks.FallthroughTask;
import net.minecraft.commands.execution.tasks.BuildContexts;
import net.minecraft.commands.functions.InstantiatedFunction;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.context.ContextChain;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import java.util.Collection;
import java.util.List;
import java.util.function.Function;
import java.util.function.IntPredicate;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.ChainModifiers;
import net.minecraft.commands.execution.CustomModifierExecutor;
import net.minecraft.commands.execution.ExecutionControl;
import net.minecraft.commands.functions.CommandFunction;
import net.minecraft.nbt.CompoundTag;
public class ExecuteCommand {
    public static final Dynamic2CommandExceptionType ERROR_FUNCTION_CONDITION_INSTANTATION_FAILURE = new Dynamic2CommandExceptionType((id, reason) -> Component.translatableEscape("commands.execute.function.instantiationFailure", id, reason));
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "execute");
    }
    public static <T extends ExecutionCommandSource<T>> void scheduleFunctionConditionsAndTest(T originalSource, List<T> currentSources, Function<T, T> functionContextModifier, IntPredicate check, ContextChain<T> currentStep, CompoundTag parameters, ExecutionControl<T> output, InCommandFunction<CommandContext<T>, Collection<CommandFunction<T>>> functionGetter, ChainModifiers modifiers) {
        List<T> passing = new ArrayList<>(currentSources.size());
        Collection<CommandFunction<T>> functions;
        try {
            functions = functionGetter.apply(currentStep.getTopContext().copyFor(originalSource));
        } catch (CommandSyntaxException failure) {
            originalSource.handleError(failure, modifiers.isForked(), output.tracer());
            return;
        }
        if (functions.isEmpty()) {
            return;
        }
        List<InstantiatedFunction<T>> bodies = new ArrayList<>(functions.size());
        try {
            for (CommandFunction<T> function : functions) {
                try {
                    bodies.add(function.instantiate(parameters, originalSource.dispatcher()));
                } catch (FunctionInstantiationException failure) {
                    throw ERROR_FUNCTION_CONDITION_INSTANTATION_FAILURE.create(function.id(), failure.messageComponent());
                }
            }
        } catch (CommandSyntaxException failure) {
            originalSource.handleError(failure, modifiers.isForked(), output.tracer());
        }
        for (T source : currentSources) {
            T inner = functionContextModifier.apply(source.clearCallbacks());
            CommandResultCallback collector = (success, result) -> {
                if (check.test(result)) {
                    passing.add(source);
                }
            };
            output.queueNext(new IsolatedCall<T>(control -> {
                for (InstantiatedFunction<T> body : bodies) {
                    control.queueNext(new CallFunction<T>(body, control.currentFrame().returnValueConsumer(), true).bind(inner));
                }
                control.queueNext(FallthroughTask.instance());
            }, collector));
        }
        ContextChain<T> next = currentStep.nextStage();
        output.queueNext(new BuildContexts.Continuation<T>(currentStep.getTopContext().getInput(), next, modifiers, originalSource, passing));
    }
    static class ExecuteIfFunctionCustomModifier implements CustomModifierExecutor.ModifierAdapter<CommandSourceStack> {
        private final java.util.function.IntPredicate check;

        private ExecuteIfFunctionCustomModifier(boolean check) {
            this.check = check ? value -> value != 0 : value -> value == 0;
            
        }
        public void apply(CommandSourceStack originalSource, List<CommandSourceStack> currentSources, ContextChain<CommandSourceStack> currentStep, ChainModifiers modifiers, ExecutionControl<CommandSourceStack> output) {
            ExecuteCommand.scheduleFunctionConditionsAndTest(originalSource, currentSources, FunctionCommand::modifySenderForExecution, check, currentStep, null, output,
                context -> FunctionArgument.getFunctions(context, "name"), modifiers);
        }
    }
    interface CommandNumericPredicate {
        int test(CommandContext<CommandSourceStack> c) throws CommandSyntaxException;
    }
    interface CommandPredicate {
        boolean test(CommandContext<CommandSourceStack> c) throws CommandSyntaxException;
    }
    interface IntBiPredicate {
        boolean test(int a, int b);
    }
}
