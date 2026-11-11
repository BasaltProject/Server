// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import net.minecraft.network.chat.Component;
import com.mojang.datafixers.util.Pair;
import net.minecraft.network.chat.ComponentUtils;
import net.minecraft.server.ServerFunctionManager;
import net.minecraft.commands.SharedSuggestionProvider;
import net.minecraft.commands.CommandResultCallback;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import net.minecraft.commands.arguments.item.FunctionArgument;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.context.ContextChain;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.Dynamic2CommandExceptionType;
import com.mojang.brigadier.suggestion.SuggestionProvider;
import java.util.Collection;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.ChainModifiers;
import net.minecraft.commands.execution.CustomCommandExecutor;
import net.minecraft.commands.execution.ExecutionControl;
import net.minecraft.commands.functions.CommandFunction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
public class FunctionCommand {
    public static final Dynamic2CommandExceptionType ERROR_FUNCTION_INSTANTATION_FAILURE = new Dynamic2CommandExceptionType((id, reason) -> Component.translatableEscape("commands.function.instantiationFailure", id, reason));
    public static final SuggestionProvider<CommandSourceStack> SUGGEST_FUNCTION = (context, builder) -> {
        ServerFunctionManager manager = context.getSource().getServer().getFunctions();
        SharedSuggestionProvider.suggestResource(manager.getTagNames(), builder, "#");
        return SharedSuggestionProvider.suggestResource(manager.getFunctionNames(), builder);
    };
    private static final Callbacks<CommandSourceStack> FULL_CONTEXT_CALLBACKS = (source, id, value) ->
        source.sendSuccess(() -> Component.translatable("commands.function.result", Component.translationArg(id), value), true);
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "function");
    }
    public static CommandSourceStack modifySenderForExecution(CommandSourceStack sender) {
        return sender.withSuppressedOutput().withMaximumPermission(net.minecraft.server.permissions.LevelBasedPermissionSet.GAMEMASTER);
    }
    public static <T extends ExecutionCommandSource<T>> void queueFunctions(Collection<CommandFunction<T>> functions, CompoundTag arguments, T originalSource, T functionSource, ExecutionControl<T> output, Callbacks<T> callbacks, ChainModifiers modifiers) throws CommandSyntaxException {
        CommandDispatcher<T> dispatcher = originalSource.dispatcher();
        T quiet = functionSource.clearCallbacks();
        CommandResultCallback total = modifiers.isReturn()
            ? CommandResultCallback.chain(originalSource.callback(), output.currentFrame().returnValueConsumer()) : originalSource.callback();
        for (CommandFunction<T> function : functions) {
            Identifier id = function.id();
            CommandResultCallback sink = originalSource.isSilent() ? total : (success, result) -> {
                callbacks.signalResult(originalSource, id, result);
                total.onResult(success, result);
            };
            try {
                output.queueNext(new net.minecraft.commands.execution.tasks.CallFunction<>(function.instantiate(arguments, dispatcher), sink, modifiers.isReturn()).bind(quiet));
            } catch (net.minecraft.commands.FunctionInstantiationException failure) {
                throw ERROR_FUNCTION_INSTANTATION_FAILURE.create(id, failure.messageComponent());
            }
        }
        if (modifiers.isReturn()) {
            output.queueNext(net.minecraft.commands.execution.tasks.FallthroughTask.instance());
        }
    }
    static abstract class FunctionCustomExecutor extends CustomCommandExecutor.WithErrorHandling<CommandSourceStack> implements CustomCommandExecutor.CommandAdapter<CommandSourceStack> {
        protected abstract CompoundTag arguments(CommandContext<CommandSourceStack> context) throws CommandSyntaxException;
        public void runGuarded(CommandSourceStack sender, ContextChain<CommandSourceStack> currentStep, ChainModifiers modifiers, ExecutionControl<CommandSourceStack> output) throws CommandSyntaxException {
            CommandContext<CommandSourceStack> context = currentStep.getTopContext().copyFor(sender);
            Pair<Identifier, Collection<CommandFunction<CommandSourceStack>>> named = FunctionArgument.getFunctionCollection(context, "name");
            Collection<CommandFunction<CommandSourceStack>> functions = named.getSecond();
            if (functions.isEmpty()) {
                throw new DynamicCommandExceptionType(name -> Component.translatableEscape("commands.function.scheduled.no_functions", name))
                    .create(Component.translationArg(named.getFirst()));
            }
            CompoundTag arguments = arguments(context);
            sender.sendSuccess(() -> functions.size() == 1
                ? Component.translatable("commands.function.scheduled.single", Component.translationArg(functions.iterator().next().id()))
                : Component.translatable("commands.function.scheduled.multiple", ComponentUtils.formatList(functions.stream().map(CommandFunction::id).toList(), Component::translationArg)), true);
            queueFunctions(functions, arguments, sender, modifySenderForExecution(sender), output, FULL_CONTEXT_CALLBACKS, modifiers);
        }
    }
    public interface Callbacks<T> {
        void signalResult(T originalSource, Identifier functionId, int newValue);
    }
}
