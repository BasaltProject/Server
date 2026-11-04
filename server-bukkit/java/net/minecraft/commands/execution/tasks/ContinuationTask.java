// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution.tasks;
import java.util.List;
import net.minecraft.commands.execution.CommandQueueEntry;
import net.minecraft.commands.execution.EntryAction;
import net.minecraft.commands.execution.ExecutionContext;
import net.minecraft.commands.execution.Frame;
public class ContinuationTask<T, P> implements EntryAction<T> {
    private final TaskProvider<T, P> taskFactory;
    private final List<P> arguments;
    private final CommandQueueEntry<T> selfEntry;
    private int index;
    private ContinuationTask(TaskProvider<T, P> taskFactory, List<P> arguments, Frame frame) {
        this.taskFactory = taskFactory;
        this.arguments = arguments;
        this.selfEntry = new CommandQueueEntry<>(frame, this);
    }
    public void execute(ExecutionContext<T> context, Frame frame) {
        context.queueNext(taskFactory.create(frame, arguments.get(index)));
        if (++index < arguments.size()) {
            context.queueNext(selfEntry);
        }
    }
    public static <T, P> void schedule(ExecutionContext<T> context, Frame frame, List<P> arguments, TaskProvider<T, P> taskFactory) {
        switch (arguments.size()) {
            case 0:
                break;
            case 1:
                context.queueNext(taskFactory.create(frame, arguments.get(0)));
                break;
            case 2:
                context.queueNext(taskFactory.create(frame, arguments.get(0)));
                context.queueNext(taskFactory.create(frame, arguments.get(1)));
                break;
            default:
                context.queueNext(new ContinuationTask<>(taskFactory, arguments, frame).selfEntry);
        }
    }
    @FunctionalInterface
    public interface TaskProvider<T, P> {
        CommandQueueEntry<T> create(Frame frame, P argument);
    }
}
