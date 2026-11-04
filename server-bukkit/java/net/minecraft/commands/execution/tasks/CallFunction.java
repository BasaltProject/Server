// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution.tasks;
import java.util.List;
import net.minecraft.commands.CommandResultCallback;
import net.minecraft.commands.ExecutionCommandSource;
import net.minecraft.commands.execution.CommandQueueEntry;
import net.minecraft.commands.execution.ExecutionContext;
import net.minecraft.commands.execution.Frame;
import net.minecraft.commands.execution.TraceCallbacks;
import net.minecraft.commands.execution.UnboundEntryAction;
import net.minecraft.commands.functions.InstantiatedFunction;
public class CallFunction<T extends ExecutionCommandSource<T>> implements UnboundEntryAction<T> {
    private final InstantiatedFunction<T> function;
    private final CommandResultCallback resultCallback;
    private final boolean returnParentFrame;
    public CallFunction(InstantiatedFunction<T> function, CommandResultCallback resultCallback, boolean returnParentFrame) {
        this.function = function;
        this.resultCallback = resultCallback;
        this.returnParentFrame = returnParentFrame;
    }
    public void execute(T sender, ExecutionContext<T> context, Frame frame) {
        context.incrementCost();
        List<UnboundEntryAction<T>> contents = function.entries();
        TraceCallbacks tracer = context.tracer();
        if (tracer != null) {
            tracer.onCall(frame.depth(), function.id(), contents.size());
        }
        int newDepth = frame.depth() + 1;
        Frame.FrameControl control = returnParentFrame ? frame.frameControl() : context.frameControlForDepth(newDepth);
        Frame newFrame = new Frame(newDepth, resultCallback, control);
        ContinuationTask.schedule(context, newFrame, contents, (inner, action) -> new CommandQueueEntry<>(inner, action.bind(sender)));
    }
}
