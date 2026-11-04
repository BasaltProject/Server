// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution.tasks;
import com.mojang.brigadier.Command;
import com.mojang.brigadier.RedirectModifier;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.context.ContextChain;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import net.minecraft.commands.CommandResultCallback;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.ChainModifiers;
import net.minecraft.commands.execution.CommandQueueEntry;
import net.minecraft.commands.execution.CustomCommandExecutor;
import net.minecraft.commands.execution.CustomModifierExecutor;
import net.minecraft.commands.execution.EntryAction;
import net.minecraft.commands.execution.ExecutionContext;
import net.minecraft.commands.execution.ExecutionControl;
import net.minecraft.commands.execution.Frame;
import net.minecraft.commands.execution.TraceCallbacks;
import net.minecraft.commands.execution.UnboundEntryAction;
import net.minecraft.network.chat.Component;
public class BuildContexts<T extends ExecutionCommandSource<T>> {
    public static final DynamicCommandExceptionType ERROR_FORK_LIMIT_REACHED = new DynamicCommandExceptionType(limit -> Component.translatableEscape("command.forkLimit", limit));
    private final String commandInput;
    private final ContextChain<T> command;
    public BuildContexts(String commandInput, ContextChain<T> command) {
        this.commandInput = commandInput;
        this.command = command;
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    protected void execute(T originalSource, List<T> initialSources, ExecutionContext<T> context, Frame frame, ChainModifiers initialModifiers) {
        ContextChain<T> stage = command;
        ChainModifiers modifiers = initialModifiers;
        List<T> sources = initialSources;
        if (stage.getStage() != ContextChain.Stage.EXECUTE) {
            context.profiler().push(() -> "prepare " + commandInput);
            try {
                for (int forkLimit = context.forkLimit(); stage.getStage() != ContextChain.Stage.EXECUTE; stage = stage.nextStage()) {
                    CommandContext<T> toRun = stage.getTopContext();
                    if (toRun.isForked()) {
                        modifiers = modifiers.setForked();
                    }
                    RedirectModifier<T> modifier = toRun.getRedirectModifier();
                    if (modifier instanceof CustomModifierExecutor custom) {
                        custom.apply(originalSource, sources, stage, modifiers, ExecutionControl.create(context, frame));
                        return;
                    }
                    if (modifier != null) {
                        context.incrementCost();
                        boolean forkedMode = modifiers.isForked();
                        List<T> next = new ArrayList<>();
                        for (T source : sources) {
                            try {
                                Collection<T> produced = ContextChain.runModifier(toRun, source, (c, s, r) -> {
                                }, forkedMode);
                                if (next.size() + produced.size() >= forkLimit) {
                                    originalSource.handleError(ERROR_FORK_LIMIT_REACHED.create(forkLimit), forkedMode, context.tracer());
                                    return;
                                }
                                next.addAll(produced);
                            } catch (CommandSyntaxException e) {
                                source.handleError(e, forkedMode, context.tracer());
                                if (!forkedMode) {
                                    return;
                                }
                            }
                        }
                        sources = next;
                    }
                }
            } finally {
                context.profiler().pop();
            }
        }
        if (sources.isEmpty()) {
            if (modifiers.isReturn()) {
                context.queueNext(new CommandQueueEntry<>(frame, FallthroughTask.<T>instance()));
            }
            return;
        }
        CommandContext<T> executeContext = stage.getTopContext();
        Command<T> executable = executeContext.getCommand();
        if (executable instanceof CustomCommandExecutor custom) {
            ExecutionControl<T> control = ExecutionControl.create(context, frame);
            for (T executionSource : sources) {
                custom.run(executionSource, stage, modifiers, control);
            }
            return;
        }
        if (modifiers.isReturn()) {
            T returning = sources.get(0);
            returning = returning.withCallback(CommandResultCallback.chain(returning.callback(), frame.returnValueConsumer()));
            sources = List.of(returning);
        }
        ExecuteCommand<T> action = new ExecuteCommand<>(commandInput, modifiers, executeContext);
        ContinuationTask.schedule(context, frame, sources, (inner, source) -> new CommandQueueEntry<>(inner, action.bind(source)));
    }
    protected void traceCommandStart(ExecutionContext<T> context, Frame frame) {
        TraceCallbacks tracer = context.tracer();
        if (tracer != null) {
            tracer.onCommand(frame.depth(), commandInput);
        }
    }
    public String toString() {
        return commandInput;
    }
    public static class Unbound<T extends ExecutionCommandSource<T>> extends BuildContexts<T> implements UnboundEntryAction<T> {
        public Unbound(String commandInput, ContextChain<T> command) {
            super(commandInput, command);
        }
        public void execute(T sender, ExecutionContext<T> context, Frame frame) {
            traceCommandStart(context, frame);
            execute(sender, List.of(sender), context, frame, ChainModifiers.DEFAULT);
        }
    }
    public static class Continuation<T extends ExecutionCommandSource<T>> extends BuildContexts<T> implements EntryAction<T> {
        private final ChainModifiers modifiers;
        private final T originalSource;
        private final List<T> sources;
        public Continuation(String commandInput, ContextChain<T> command, ChainModifiers modifiers, T originalSource, List<T> sources) {
            super(commandInput, command);
            this.originalSource = originalSource;
            this.sources = sources;
            this.modifiers = modifiers;
        }
        public void execute(ExecutionContext<T> context, Frame frame) {
            execute(originalSource, sources, context, frame, modifiers);
        }
    }
    public static class TopLevel<T extends ExecutionCommandSource<T>> extends BuildContexts<T> implements EntryAction<T> {
        private final T source;
        public TopLevel(String commandInput, ContextChain<T> command, T source) {
            super(commandInput, command);
            this.source = source;
        }
        public void execute(ExecutionContext<T> context, Frame frame) {
            traceCommandStart(context, frame);
            execute(source, List.of(source), context, frame, ChainModifiers.DEFAULT);
        }
    }
}
