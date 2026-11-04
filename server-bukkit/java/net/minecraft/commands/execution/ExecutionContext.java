// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import com.mojang.brigadier.context.ContextChain;
import net.minecraft.commands.CommandResultCallback;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.tasks.BuildContexts;
import net.minecraft.commands.execution.tasks.CallFunction;
import net.minecraft.commands.functions.InstantiatedFunction;
import net.minecraft.util.profiling.ProfilerFiller;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class ExecutionContext<T> implements AutoCloseable {
    private static final int kMaxQueueDepth = 10000000;
    private static final Logger LOGGER = LoggerFactory.getLogger(ExecutionContext.class);
    private final int commandLimit;
    private final int forkLimit;
    private final ProfilerFiller profiler;
    private TraceCallbacks tracer;
    private int commandQuota;
    private boolean queueOverflow;
    private final Deque<CommandQueueEntry<T>> commandQueue = new ArrayDeque<>();
    private final List<CommandQueueEntry<T>> newTopCommands = new ArrayList<>();
    private int currentFrameDepth;
    public ExecutionContext(int commandLimit, int forkLimit, ProfilerFiller profiler) {
        this.commandLimit = commandLimit;
        this.forkLimit = forkLimit;
        this.profiler = profiler;
        this.commandQuota = commandLimit;
    }
    private static <T extends ExecutionCommandSource<T>> Frame createTopFrame(ExecutionContext<T> context, CommandResultCallback frameResult) {
        if (context.currentFrameDepth == 0) {
            return new Frame(0, frameResult, context.commandQueue::clear);
        }
        int depth = context.currentFrameDepth + 1;
        return new Frame(depth, frameResult, context.frameControlForDepth(depth));
    }
    public static <T extends ExecutionCommandSource<T>> void queueInitialFunctionCall(ExecutionContext<T> context, InstantiatedFunction<T> function, T sender, CommandResultCallback functionReturn) {
        context.queueNext(new CommandQueueEntry<>(createTopFrame(context, functionReturn), new CallFunction<>(function, sender.callback(), false).bind(sender)));
    }
    public static <T extends ExecutionCommandSource<T>> void queueInitialCommandExecution(ExecutionContext<T> context, String command, ContextChain<T> executionChain, T sender, CommandResultCallback commandReturn) {
        context.queueNext(new CommandQueueEntry<>(createTopFrame(context, commandReturn), new BuildContexts.TopLevel<>(command, executionChain, sender)));
    }
    private void handleQueueOverflow() {
        queueOverflow = true;
        newTopCommands.clear();
        commandQueue.clear();
    }
    public void queueNext(CommandQueueEntry<T> entry) {
        if (newTopCommands.size() + commandQueue.size() > kMaxQueueDepth) {
            handleQueueOverflow();
        }
        if (!queueOverflow) {
            newTopCommands.add(entry);
        }
    }
    public void discardAtDepthOrHigher(int depthToDiscard) {
        while (!commandQueue.isEmpty() && commandQueue.peek().frame().depth() >= depthToDiscard) {
            commandQueue.removeFirst();
        }
    }
    public Frame.FrameControl frameControlForDepth(int depthToDiscard) {
        return () -> discardAtDepthOrHigher(depthToDiscard);
    }
    public void runCommandQueue() {
        pushNewCommands();
        while (true) {
            if (commandQuota <= 0) {
                LOGGER.info("Command execution stopped due to limit (executed {} commands)", commandLimit);
                break;
            }
            CommandQueueEntry<T> command = commandQueue.pollFirst();
            if (command == null) {
                return;
            }
            currentFrameDepth = command.frame().depth();
            command.execute(this);
            if (queueOverflow) {
                LOGGER.error("Command execution stopped due to command queue overflow (max {})", kMaxQueueDepth);
                break;
            }
            pushNewCommands();
        }
        currentFrameDepth = 0;
    }
    private void pushNewCommands() {
        for (int i = newTopCommands.size() - 1; i >= 0; i--) {
            commandQueue.addFirst(newTopCommands.get(i));
        }
        newTopCommands.clear();
    }
    public void tracer(TraceCallbacks tracer) {
        this.tracer = tracer;
    }
    public TraceCallbacks tracer() {
        return tracer;
    }
    public ProfilerFiller profiler() {
        return profiler;
    }
    public int forkLimit() {
        return forkLimit;
    }
    public void incrementCost() {
        commandQuota--;
    }
    public void close() {
        if (tracer != null) {
            tracer.close();
        }
    }
}
